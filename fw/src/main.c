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

// EC11 on the right half. MCU pads 14/13/15 -> GP9/GP10/GP8 (ENC_A/ENC_B/ENC_SW).
#define ENC_A_PIN  9
#define ENC_B_PIN  10
#define ENC_SW_PIN 8

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
#define MOD(mod, key) ((uint32_t)(mod) << 8 | (key))

// Mod-tap: the modifier when held, the keycode when tapped. Needs its own flag bit
// because layer 1 already has plain entries carrying both a modifier and a keycode
// (the shifted symbols), so "has both" can't be the discriminator.
#define MT_FLAG      0x10000u
#define MT(mod, key) (MT_FLAG | (uint32_t)(mod) << 8 | (key))
#define IS_MT(e)     ((e) & MT_FLAG)
#define MT_TERM_US   200000ULL

static const uint8_t row_pins[NUM_ROWS] = { 26, 15, 14, 13 };
static const uint8_t col_pins[NUM_COLS] = {  2, 29,  3, 28,  4, 27,  5 };

#define NUM_LAYERS 2

#define TRNS  0xFFFF // fall through to layer 0
#define FN    0xFFFE // momentary layer 1

// Both keymaps are written in physical left-to-right order. On the left half that is
// col_pins[] order; the right half is the same PCB flipped, so its physical order is
// the mirror image and keymap_at() reverses the lookup.
static const uint32_t keymap[NUM_LAYERS][2][NUM_ROWS][NUM_COLS] = {
    [0] = {
    // outer, pinky, ring, middle, index, inner, thumb
    [HAND_LEFT] = {
    { HID_KEY_NONE , HID_KEY_Q     , HID_KEY_W   , HID_KEY_F                        , HID_KEY_P                          , HID_KEY_G        , HID_KEY_NONE },
    { HID_KEY_EQUAL, HID_KEY_A     , HID_KEY_R   , HID_KEY_S                        , HID_KEY_T                          , HID_KEY_D        , HID_KEY_NONE },
    { HID_KEY_NONE , HID_KEY_Z     , HID_KEY_X   , HID_KEY_C                        , HID_KEY_V                          , HID_KEY_B        , HID_KEY_DELETE },
    { HID_KEY_NONE , HID_KEY_ESCAPE, HID_KEY_NONE, MOD(KEYBOARD_MODIFIER_LEFTGUI, 0), MOD(KEYBOARD_MODIFIER_LEFTSHIFT, 0), HID_KEY_BACKSPACE, MT(KEYBOARD_MODIFIER_LEFTCTRL, HID_KEY_TAB) },
    },
    // thumb, inner, index, middle, ring, pinky, outer
    [HAND_RIGHT] = {
    { HID_KEY_NONE                     , HID_KEY_J    , HID_KEY_L, HID_KEY_U    , HID_KEY_Y     , HID_KEY_SEMICOLON, HID_KEY_NONE },
    { HID_KEY_NONE                     , HID_KEY_H    , HID_KEY_N, HID_KEY_E    , HID_KEY_I     , HID_KEY_O        , HID_KEY_APOSTROPHE },
    { HID_KEY_NONE                     , HID_KEY_K    , HID_KEY_M, HID_KEY_COMMA, HID_KEY_PERIOD, HID_KEY_SLASH    , HID_KEY_NONE },
    { MT(KEYBOARD_MODIFIER_LEFTALT, HID_KEY_ENTER), HID_KEY_SPACE, FN, HID_KEY_MINUS, HID_KEY_NONE  , HID_KEY_ENTER    , HID_KEY_NONE },
    },
    },
    [1] = {
    // outer, pinky, ring, middle, index, inner, thumb
    [HAND_LEFT] = {
    { TRNS, MOD(KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_1), MOD(KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_2), HID_KEY_ARROW_UP                           , MOD(KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_BRACKET_LEFT), MOD(KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_BRACKET_RIGHT), TRNS },
    { TRNS, MOD(KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_3), HID_KEY_ARROW_LEFT                         , HID_KEY_ARROW_DOWN                         , HID_KEY_ARROW_RIGHT                                   , MOD(KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_4)            , TRNS },
    { TRNS, HID_KEY_BRACKET_LEFT                       , HID_KEY_BRACKET_RIGHT                      , MOD(KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_9), MOD(KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_0)           , MOD(KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_7)            , TRNS },
    { TRNS, TRNS                                       , HID_KEY_INSERT                             , MOD(KEYBOARD_MODIFIER_LEFTGUI, 0)          , MOD(KEYBOARD_MODIFIER_LEFTSHIFT, 0)                   , HID_KEY_BACKSPACE                                      , MOD(KEYBOARD_MODIFIER_LEFTCTRL, 0) },
    },
    // thumb, inner, index, middle, ring, pinky, outer
    [HAND_RIGHT] = {
    { TRNS                             , HID_KEY_PAGE_UP  , HID_KEY_7, HID_KEY_8     , HID_KEY_9, MOD(KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_8)    , TRNS },
    { TRNS                             , HID_KEY_PAGE_DOWN, HID_KEY_4, HID_KEY_5     , HID_KEY_6, MOD(KEYBOARD_MODIFIER_LEFTSHIFT, HID_KEY_EQUAL), TRNS },
    { TRNS                             , HID_KEY_GRAVE    , HID_KEY_1, HID_KEY_2     , HID_KEY_3, HID_KEY_BACKSLASH                              , TRNS },
    { MOD(KEYBOARD_MODIFIER_LEFTALT, 0), HID_KEY_SPACE    , TRNS     , HID_KEY_PERIOD, HID_KEY_0, HID_KEY_EQUAL                                  , TRNS },
    },
    },
};

static inline uint32_t keymap_at(int layer, int hand, int r, int c) {
    int col = (hand == HAND_RIGHT) ? NUM_COLS - 1 - c : c;
    uint32_t e = keymap[layer][hand][r][col];
    return (e == TRNS) ? keymap[0][hand][r][col] : e;
}

static inline void led_set_rgb(uint8_t r, uint8_t g, uint8_t b) {
    // WS2812 expects GRB, packed into the top 24 bits
    uint32_t grb = ((uint32_t)g << 24) | ((uint32_t)r << 16) | ((uint32_t)b << 8);
    pio_sm_put_blocking(LED_PIO, LED_SM, grb);
}

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

//// BOOTSEL ESCAPES

// Two independent ways back into the bootloader, needed because the right half's
// module is mounted inverted and its BOOT button is unreachable:
//
//   1. hold the outer/top key while plugging in, or press it while running
//   2. pull BOOTSEL_PIN to ground
//
// Both are checked at the top of main(), before any init that can block, and both
// stay live afterwards. The pin is additionally polled from a timer IRQ, so it works
// even if the main loop wedges (e.g. a stalled pio_sm_put_blocking).
//
// GP7 is MCU pad 16 (net P16) and is unconnected on the PCB, so its through-hole
// doubles as the test point. Active low: short it to any ground.

#define BOOTSEL_PIN     7
#define BOOTSEL_KEY_R   0 // top row
#define BOOTSEL_KEY_C   0 // outer column
#define BOOTSEL_POLL_MS 20

static void bootsel_pin_init(void) {
    gpio_init(BOOTSEL_PIN);
    gpio_set_dir(BOOTSEL_PIN, GPIO_IN);
    gpio_pull_up(BOOTSEL_PIN);
}

static inline bool bootsel_pin_asserted(void) {
    return !gpio_get(BOOTSEL_PIN);
}

// Runs off a timer IRQ, so grounding the pin works while the keyboard is live and
// even if the main loop is wedged.
static bool bootsel_timer_cb(repeating_timer_t *t) {
    (void)t;
    if (bootsel_pin_asserted()) reset_usb_boot(0, 0);
    return true;
}

// Held key or grounded pin at power-on.
static void bootsel_check_at_boot(void) {
    bool state[NUM_ROWS][NUM_COLS];

    sleep_ms(1); // let the pull-up settle before the first read
    matrix_scan(state);

    if (bootsel_pin_asserted() || state[BOOTSEL_KEY_R][BOOTSEL_KEY_C])
        reset_usb_boot(0, 0);
}

//// SPLIT LINK

// Frame: 0xA5 | s0 | s1 | s2 | s3 | xor, where s0..s3 hold the 28 key bits and xor
// covers the sync byte too. 0xA5 can occur in the payload, but the fixed length plus
// the checksum resynchronises within a frame or two.
// The 4 payload bytes hold 28 key bits, so bits 28..31 are free: the peripheral uses
// two of them to pulse a volume tick for exactly one frame.
#define LINK_BIT_VOL_UP   28
#define LINK_BIT_VOL_DOWN 29
#define LINK_BIT_MUTE     30

#define LINK_SYNC         0xA5
#define LINK_FRAME_LEN    6
#define LINK_HEARTBEAT_US 20000ULL
#define LINK_TIMEOUT_US   100000ULL

static bool remote_state[NUM_ROWS][NUM_COLS] = {0};

#if IS_MASTER

static uint64_t link_last_rx_us = 0;

// Volume ticks arrive as one-frame pulses but need a press and a release report each,
// and only one report can go out per loop iteration.
static uint16_t consumer_queue[16];
static int consumer_head = 0, consumer_tail = 0;

static void consumer_push(uint16_t usage) {
    // needs both slots free, or the release would be dropped and the key would stick
    if ((consumer_head + 2) % 16 == consumer_tail ||
        (consumer_head + 1) % 16 == consumer_tail) return;
    consumer_queue[consumer_head] = usage;
    consumer_head = (consumer_head + 1) % 16;
    consumer_queue[consumer_head] = 0; // release
    consumer_head = (consumer_head + 1) % 16;
}

// Returns true if a report was sent, so the caller skips the keyboard report.
static bool consumer_task(void) {
    if (consumer_head == consumer_tail) return false;
    uint16_t usage = consumer_queue[consumer_tail];
    consumer_tail = (consumer_tail + 1) % 16;
    tud_hid_report(REPORT_ID_CONSUMER, &usage, 2);
    return true;
}

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
        if ((buf[1 + LINK_BIT_VOL_UP / 8] >> (LINK_BIT_VOL_UP % 8)) & 1)
            consumer_push(HID_USAGE_CONSUMER_VOLUME_INCREMENT);
        if ((buf[1 + LINK_BIT_VOL_DOWN / 8] >> (LINK_BIT_VOL_DOWN % 8)) & 1)
            consumer_push(HID_USAGE_CONSUMER_VOLUME_DECREMENT);
        if ((buf[1 + LINK_BIT_MUTE / 8] >> (LINK_BIT_MUTE % 8)) & 1)
            consumer_push(HID_USAGE_CONSUMER_MUTE);
        link_last_rx_us = time_us_64();
    }

    // drop the far half if it goes quiet, so keys never stick on unplug
    if (time_us_64() - link_last_rx_us > LINK_TIMEOUT_US)
        memset(remote_state, 0, sizeof(remote_state));
}

//// MOD-TAP

// Hold-on-other-key-press: a mod-tap becomes its modifier when another key goes down
// after it, or after MT_TERM_US, whichever comes first. Nothing is ever deferred, so
// no keypress is delayed waiting for a decision.
enum { MT_IDLE, MT_PENDING, MT_HELD };

static uint8_t  mt_state[2][NUM_ROWS][NUM_COLS] = {0};
static uint64_t mt_t0[2][NUM_ROWS][NUM_COLS]    = {0};
static uint8_t  pending_tap                     = 0;

static void mt_task(int layer, bool local[NUM_ROWS][NUM_COLS],
                    bool remote[NUM_ROWS][NUM_COLS]) {
    static bool prev[2][NUM_ROWS][NUM_COLS] = {0};
    uint64_t now = time_us_64();
    bool other_pressed = false;

    // only a key that goes down *after* the mod-tap counts. A key already held when
    // the thumb lands must not resolve it, or rolling t -> thumb gives Alt instead of
    // the Enter tap.
    for (int r = 0; r < NUM_ROWS; r++)
        for (int c = 0; c < NUM_COLS; c++)
            for (int h = 0; h < 2; h++) {
                bool pressed = (h == HAND) ? local[r][c] : remote[r][c];
                uint32_t e = keymap_at(layer, h, r, c);
                if (pressed && !prev[h][r][c] && e && !IS_MT(e)) other_pressed = true;
                prev[h][r][c] = pressed;
            }

    for (int r = 0; r < NUM_ROWS; r++)
        for (int c = 0; c < NUM_COLS; c++)
            for (int h = 0; h < 2; h++) {
                uint32_t e = keymap_at(layer, h, r, c);
                bool pressed = (h == HAND) ? local[r][c] : remote[r][c];
                uint8_t *st = &mt_state[h][r][c];

                // clear on release whatever the entry is now, so a layer change while
                // the key is down can't strand it in MT_PENDING
                if (!pressed) {
                    if (*st == MT_PENDING && IS_MT(e)) pending_tap = e & 0xFF;
                    *st = MT_IDLE;
                    continue;
                }
                if (!IS_MT(e)) continue;

                if (*st == MT_IDLE) {
                    *st = MT_PENDING;
                    mt_t0[h][r][c] = now;
                } else if (*st == MT_PENDING &&
                           (other_pressed || now - mt_t0[h][r][c] >= MT_TERM_US)) {
                    *st = MT_HELD;
                }
            }
}

static void report_add(uint32_t entry, uint8_t state, uint8_t *modifier,
                       uint8_t *keycodes, int *idx) {
    if (entry == FN) return; // layer key, never reported
    if (IS_MT(entry)) {
        // undecided contributes nothing; once held it is the modifier only, and the
        // tap keycode is emitted separately on release
        if (state == MT_HELD) *modifier |= (entry >> 8) & 0xFF;
        return;
    }
    *modifier |= (entry >> 8) & 0xFF;
    uint8_t key = entry & 0xFF;
    if (key && *idx < 6) keycodes[(*idx)++] = key;
}

#else

static void link_init(void) {
    gpio_init(ENC_A_PIN);
    gpio_set_dir(ENC_A_PIN, GPIO_IN);
    gpio_pull_up(ENC_A_PIN);
    gpio_init(ENC_B_PIN);
    gpio_set_dir(ENC_B_PIN, GPIO_IN);
    gpio_pull_up(ENC_B_PIN);
    gpio_init(ENC_SW_PIN);
    gpio_set_dir(ENC_SW_PIN, GPIO_IN);
    gpio_pull_up(ENC_SW_PIN);

    uint offset = pio_add_program(LINK_PIO, &uart_tx_program);
    uart_tx_program_init(LINK_PIO, LINK_SM, offset, LINK_PIN, LINK_BAUD);
}

// Quadrature decode with a 4-step detent, ported from the v4 QMK keymap: a reversal
// discards the partial detent so half-steps never emit.
static int encoder_task(void) {
    static uint8_t prev_state = 0;
    static int position = 0, last_direction = 0;

    uint8_t a = gpio_get(ENC_A_PIN) ? 1 : 0;
    uint8_t b = gpio_get(ENC_B_PIN) ? 1 : 0;
    uint8_t cur_state = (a << 1) | b;
    if (cur_state == prev_state) return 0;

    int direction = ((prev_state == 0b00 && cur_state == 0b01) ||
                     (prev_state == 0b01 && cur_state == 0b11) ||
                     (prev_state == 0b11 && cur_state == 0b10) ||
                     (prev_state == 0b10 && cur_state == 0b00)) ? 1 : -1;

    if (direction != last_direction && position != 0) position = 0;
    position += direction;
    last_direction = direction;
    prev_state = cur_state;

    if (position >= 4)  { position = 0; return  1; }
    if (position <= -4) { position = 0; return -1; }
    return 0;
}

// One pulse per press of the shaft, active low.
static bool encoder_sw_task(void) {
    static bool prev = false;
    bool now = !gpio_get(ENC_SW_PIN);
    bool pressed = now && !prev;
    prev = now;
    return pressed;
}

static void link_task(bool state[NUM_ROWS][NUM_COLS]) {
    static uint8_t prev[4] = {0};
    static uint64_t last_us = 0;

    uint8_t cur[4] = {0};
    int tick = encoder_task();
    if (tick > 0) cur[LINK_BIT_VOL_UP / 8]   |= 1u << (LINK_BIT_VOL_UP % 8);
    if (tick < 0) cur[LINK_BIT_VOL_DOWN / 8] |= 1u << (LINK_BIT_VOL_DOWN % 8);
    if (encoder_sw_task()) cur[LINK_BIT_MUTE / 8] |= 1u << (LINK_BIT_MUTE % 8);
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
    // before anything that can block, so the escapes survive a broken main loop
    static repeating_timer_t bootsel_timer;
    bootsel_pin_init();
    matrix_init();
    bootsel_check_at_boot();
    add_repeating_timer_ms(-BOOTSEL_POLL_MS, bootsel_timer_cb, NULL, &bootsel_timer);

    stdio_init_all();
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
        if (state[BOOTSEL_KEY_R][BOOTSEL_KEY_C]) {
          reset_usb_boot(0, 0);
        }

#if IS_MASTER
        link_task();

        if (consumer_task()) continue;

        memset(keycodes, 0, sizeof(keycodes));
        uint8_t modifier = 0;
        int idx = 0;

        int layer = 0;
        for (int r = 0; r < NUM_ROWS; r++)
          for (int c = 0; c < NUM_COLS; c++)
            if ((state[r][c]        && keymap_at(0, HAND,  r, c) == FN) ||
                (remote_state[r][c] && keymap_at(0, !HAND, r, c) == FN))
              layer = 1;

        mt_task(layer, state, remote_state);

        for (int r = 0; r < NUM_ROWS; r++)
          for (int c = 0; c < NUM_COLS; c++) {
            if (state[r][c])        report_add(keymap_at(layer, HAND,  r, c), mt_state[HAND][r][c],  &modifier, keycodes, &idx);
            if (remote_state[r][c]) report_add(keymap_at(layer, !HAND, r, c), mt_state[!HAND][r][c], &modifier, keycodes, &idx);
          }

        // one-report pulse; the next report rebuilds without it, which is the release
        if (pending_tap) {
          if (idx < 6) keycodes[idx++] = pending_tap;
          pending_tap = 0;
        }
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, modifier, keycodes);
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
