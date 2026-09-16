#include <string.h>

#include "hardware/pio.h"
#include "hardware/uart.h"
#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "tusb.h"
#include "uart_tx.pio.h"
#include "ws2812.pio.h"

//// BOARD CFG

#define LED_PIN 16
#define LED_PIO pio0
#define LED_SM  0

// TRRS ring 2. Straight-through to the other half's GP1, not crossed: the PCB is
// reversible, so both halves map the same jack pin to the same net. Ring 1 (GP0) is
// the mirror of that and stays unused, so nothing ever drives it.
#define LINK_PIN  1
#define LINK_BAUD 115200
#define LINK_PIO  pio1
#define LINK_SM   0

//// HANDEDNESS

#define HAND_LEFT  0
#define HAND_RIGHT 1

#ifndef HAND
#define HAND HAND_LEFT
#endif

// The left half owns USB and merges both halves; the right half only transmits.
#define IS_MASTER (HAND == HAND_LEFT)

//// LAYOUT

#define NUM_ROWS 4
#define NUM_COLS 7
#define MOD(mod, key) ((uint16_t)(mod) << 8 | (key))

static const uint8_t row_pins[NUM_ROWS] = { 26, 15, 14, 13 };
static const uint8_t col_pins[NUM_COLS] = {  2, 29,  3, 28,  4, 27,  5 };

// Both keymaps are written in physical left-to-right order. On the left half that is
// col_pins[] order; the right half is the same PCB flipped, so its physical order is
// the mirror image and keymap_at() reverses the lookup.
static const uint16_t keymap[2][NUM_ROWS][NUM_COLS] = {
    //  outer          pinky           ring          middle                               index                                inner              thumb
    [HAND_LEFT] = {
    { HID_KEY_NONE, HID_KEY_Q,      HID_KEY_W,    HID_KEY_F,                           HID_KEY_P,                           HID_KEY_G,         HID_KEY_NONE },
    { HID_KEY_NONE, HID_KEY_A,      HID_KEY_R,    HID_KEY_S,                           HID_KEY_T,                           HID_KEY_D,         HID_KEY_NONE },
    { HID_KEY_NONE, HID_KEY_Z,      HID_KEY_X,    HID_KEY_C,                           HID_KEY_V,                           HID_KEY_B,         HID_KEY_DELETE },
    { HID_KEY_NONE, HID_KEY_ESCAPE, HID_KEY_NONE, MOD(KEYBOARD_MODIFIER_LEFTGUI, 0),   MOD(KEYBOARD_MODIFIER_LEFTSHIFT, 0), HID_KEY_BACKSPACE, HID_KEY_TAB },
    },
    //  thumb                              inner          index                  middle         ring                pinky              outer
    [HAND_RIGHT] = {
    { HID_KEY_NONE,                      HID_KEY_J,     HID_KEY_L,             HID_KEY_U,     HID_KEY_Y,          HID_KEY_SEMICOLON, HID_KEY_NONE },
    { HID_KEY_NONE,                      HID_KEY_H,     HID_KEY_N,             HID_KEY_E,     HID_KEY_I,          HID_KEY_O,         HID_KEY_NONE },
    { HID_KEY_NONE,                      HID_KEY_K,     HID_KEY_M,             HID_KEY_COMMA, HID_KEY_PERIOD,     HID_KEY_SLASH,     HID_KEY_NONE },
    { MOD(KEYBOARD_MODIFIER_LEFTALT, 0), HID_KEY_SPACE, HID_KEY_NONE /* FN */, HID_KEY_MINUS, HID_KEY_APOSTROPHE, HID_KEY_ENTER,     HID_KEY_NONE },
    },
};

static inline uint16_t keymap_at(int hand, int r, int c) {
    return keymap[hand][r][hand == HAND_RIGHT ? NUM_COLS - 1 - c : c];
}

static inline void led_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    // WS2812 expects GRB, packed into the top 24 bits
    uint32_t grb = ((uint32_t)g << 24) | ((uint32_t)r << 16) | ((uint32_t)b << 8);
    pio_sm_put_blocking(LED_PIO, LED_SM, grb);
}

//// IDLE

#if IS_MASTER

#define IDLE_TOGGLE_KEY  HID_KEY_A
#define IDLE_INTERVAL_US (60ULL * 1000000ULL)

#define IDLE_KEY HID_KEY_F17

static bool     idle_enabled         = false;
static bool     idle_key_prev        = false;
static uint64_t idle_last_fire_us    = 0;
static bool     idle_release_pending = false;

static bool idle_task(bool local[NUM_ROWS][NUM_COLS], bool remote[NUM_ROWS][NUM_COLS]) {
    bool toggle_pressed  = false;
    bool any_key_pressed = false;

    for (int r = 0; r < NUM_ROWS; r++)
        for (int c = 0; c < NUM_COLS; c++) {
            if (local[r][c]) {
                uint8_t key = keymap_at(HAND, r, c) & 0xFF;
                if (key == IDLE_TOGGLE_KEY)
                    toggle_pressed = true;
                else
                    any_key_pressed = true;
            }
            // any keypress on the far half cancels idle too
            if (remote[r][c] && keymap_at(!HAND, r, c))
                any_key_pressed = true;
        }

    if (toggle_pressed && !idle_key_prev) {
        idle_enabled = !idle_enabled;
        idle_last_fire_us = time_us_64();
    } else if (any_key_pressed && idle_enabled) {
        idle_enabled = false;
    }
    idle_key_prev = toggle_pressed;
    led_set_rgb(0, idle_enabled ? 16 : 0, 0);

    if (idle_release_pending) {
        idle_release_pending = false;
        uint8_t empty[6] = {0};
        tud_hid_keyboard_report(0, 0, empty);
        return true;
    }

    if (!idle_enabled) return false;

    uint64_t now = time_us_64();
    if (now - idle_last_fire_us < IDLE_INTERVAL_US) return false;
    idle_last_fire_us = now;

    uint8_t keycodes[6] = { IDLE_KEY, 0, 0, 0, 0, 0 };
    tud_hid_keyboard_report(0, 0, keycodes);
    idle_release_pending = true;
    return true;
}

#endif // IS_MASTER

//// MATRIX SCAN

static void matrix_init(void) {
    for (int r = 0; r < NUM_ROWS; r++) {
         gpio_init(row_pins[r]);
        gpio_set_dir(row_pins[r], GPIO_OUT);
        gpio_put(row_pins[r], 1);
    }
    for (int c = 0; c < NUM_COLS; c++) {
        gpio_init(col_pins[c]);
        gpio_set_dir(col_pins[c], GPIO_IN);
        gpio_pull_up(col_pins[c]);
    }
}

static void matrix_scan(bool state[NUM_ROWS][NUM_COLS]) {
    for (int r = 0; r < NUM_ROWS; r++) {
        gpio_put(row_pins[r], 0);
        busy_wait_us(10);
        for (int c = 0; c < NUM_COLS; c++)
            state[r][c] = !gpio_get(col_pins[c]); // active low
        gpio_put(row_pins[r], 1);
    }
}

//// SPLIT LINK

// Frame: 0xA5 | s0 | s1 | s2 | s3 | xor, where s0..s3 hold the 28 key bits and xor
// covers the sync byte too. 0xA5 can occur in the payload, but the fixed length plus
// the checksum resynchronises within a frame or two.
#define LINK_SYNC         0xA5
#define LINK_FRAME_LEN    6
#define LINK_HEARTBEAT_US 20000ULL
#define LINK_TIMEOUT_US   100000ULL

static bool remote_state[NUM_ROWS][NUM_COLS] = {0};

#if IS_MASTER

static uint64_t link_last_rx_us = 0;

static void link_init(void) {
    uart_init(uart0, LINK_BAUD);
    uart_set_format(uart0, 8, 1, UART_PARITY_NONE);
    uart_set_fifo_enabled(uart0, true);
    gpio_set_function(LINK_PIN, GPIO_FUNC_UART);
    gpio_pull_up(LINK_PIN); // idle high when nothing is plugged in
}

static void link_task(void) {
    static uint8_t buf[LINK_FRAME_LEN];
    static int idx = 0;

    while (uart_is_readable(uart0)) {
        uint8_t b = uart_getc(uart0);
        if (idx == 0 && b != LINK_SYNC) continue;
        buf[idx++] = b;
        if (idx < LINK_FRAME_LEN) continue;
        idx = 0;

        uint8_t sum = 0;
        for (int i = 0; i < LINK_FRAME_LEN - 1; i++) sum ^= buf[i];
        if (sum != buf[LINK_FRAME_LEN - 1]) continue;

        for (int r = 0; r < NUM_ROWS; r++)
            for (int c = 0; c < NUM_COLS; c++) {
                int bit = r * NUM_COLS + c;
                remote_state[r][c] = (buf[1 + bit / 8] >> (bit % 8)) & 1;
            }
        link_last_rx_us = time_us_64();
    }

    // drop the far half if it goes quiet, so keys never stick on unplug
    if (time_us_64() - link_last_rx_us > LINK_TIMEOUT_US)
        memset(remote_state, 0, sizeof(remote_state));
}

static void report_add(uint16_t entry, uint8_t *modifier, uint8_t *keycodes, int *idx) {
    *modifier |= (entry >> 8);
    uint8_t key = entry & 0xFF;
    if (key && *idx < 6) keycodes[(*idx)++] = key;
}

#else

static void link_init(void) {
    uint offset = pio_add_program(LINK_PIO, &uart_tx_program);
    uart_tx_program_init(LINK_PIO, LINK_SM, offset, LINK_PIN, LINK_BAUD);
}

static void link_task(bool state[NUM_ROWS][NUM_COLS]) {
    static uint8_t prev[4] = {0};
    static uint64_t last_us = 0;

    uint8_t cur[4] = {0};
    for (int r = 0; r < NUM_ROWS; r++)
        for (int c = 0; c < NUM_COLS; c++)
            if (state[r][c]) {
                int bit = r * NUM_COLS + c;
                cur[bit / 8] |= 1u << (bit % 8);
            }

    uint64_t now = time_us_64();
    if (memcmp(cur, prev, sizeof(cur)) == 0 && now - last_us < LINK_HEARTBEAT_US) return;
    memcpy(prev, cur, sizeof(cur));
    last_us = now;

    uint8_t sum = LINK_SYNC;
    uart_tx_program_putc(LINK_PIO, LINK_SM, LINK_SYNC);
    for (int i = 0; i < 4; i++) {
        uart_tx_program_putc(LINK_PIO, LINK_SM, cur[i]);
        sum ^= cur[i];
    }
    uart_tx_program_putc(LINK_PIO, LINK_SM, sum);
}

#endif

int main(void) {
    stdio_init_all();
    matrix_init();
    link_init();
#if IS_MASTER
    tusb_init();
#endif

    // WS2812 init
    uint offset = pio_add_program(LED_PIO, &ws2812_program);
    ws2812_program_init(LED_PIO, LED_SM, offset, LED_PIN, 800000, false);
    led_set_rgb(0, 0, 0);

    bool state[NUM_ROWS][NUM_COLS] = {0};
#if IS_MASTER
    uint8_t keycodes[6] = {0};
#endif

    while (1) {
#if IS_MASTER
        tud_task();

        if (!tud_hid_ready()) continue;
#endif

        matrix_scan(state);

        // jump to bootsel if the outer/top key is hit
        if (state[0][0]) {
          reset_usb_boot(0, 0);
        }

#if IS_MASTER
        link_task();

        if (idle_task(state, remote_state)) continue;

        memset(keycodes, 0, sizeof(keycodes));
        uint8_t modifier = 0;
        int idx = 0;

        for (int r = 0; r < NUM_ROWS; r++)
          for (int c = 0; c < NUM_COLS; c++) {
            if (state[r][c])        report_add(keymap_at(HAND,  r, c), &modifier, keycodes, &idx);
            if (remote_state[r][c]) report_add(keymap_at(!HAND, r, c), &modifier, keycodes, &idx);
          }
        tud_hid_keyboard_report(0, modifier, keycodes);
#else
        link_task(state);
#endif
    }
}

//// TUD SHIT

// Called by TinyUSB when host is ready
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len) {
    (void)instance; (void)report; (void)len;
}

// Required TinyUSB HID callback
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize) {
    (void)instance; (void)report_id; (void)report_type; (void)buffer; (void)bufsize;
}
