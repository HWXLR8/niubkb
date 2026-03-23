#include <stdlib.h>
#include "pico/bootrom.h"
#include "pico/stdlib.h"
#include "tusb.h"

#define NUM_ROWS 4
#define NUM_COLS 7
#define MOD(mod, key) ((uint16_t)(mod) << 8 | (key))

static const uint8_t row_pins[NUM_ROWS] = { 26, 15, 14, 13 };
static const uint8_t col_pins[NUM_COLS] = {  2, 29,  3, 28,  4, 27,  5 };

// 4x7 keymap — fill in your HID keycodes
static const uint16_t keymap[NUM_ROWS][NUM_COLS] = {
    { HID_KEY_NONE, HID_KEY_Q,      HID_KEY_W,    HID_KEY_F,                           HID_KEY_P,                           HID_KEY_G,         HID_KEY_NONE },
    { HID_KEY_NONE, HID_KEY_A,      HID_KEY_R,    HID_KEY_S,                           HID_KEY_T,                           HID_KEY_D,         HID_KEY_NONE },
    { HID_KEY_NONE, HID_KEY_Z,      HID_KEY_X,    HID_KEY_C,                           HID_KEY_V,                           HID_KEY_B,         HID_KEY_DELETE },
    { HID_KEY_NONE, HID_KEY_ESCAPE, HID_KEY_NONE, MOD(KEYBOARD_MODIFIER_LEFTGUI, 0),   MOD(KEYBOARD_MODIFIER_LEFTSHIFT, 0), HID_KEY_BACKSPACE, HID_KEY_TAB },
};

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

int main(void) {
    stdio_init_all();
    matrix_init();
    tusb_init();

    bool state[NUM_ROWS][NUM_COLS] = {0};
    uint8_t keycodes[6] = {0};

    while (1) {
        tud_task();

        if (!tud_hid_ready()) continue;

        matrix_scan(state);
        // jump to bootsel if top-left key is hit
        if (state[0][0]) {
          reset_usb_boot(0, 0);
        }

        memset(keycodes, 0, sizeof(keycodes));
        uint8_t modifier = 0;
        int idx = 0;

        for (int r = 0; r < NUM_ROWS; r++)
          for (int c = 0; c < NUM_COLS; c++)
            if (state[r][c]) {
              uint16_t entry = keymap[r][c];
              modifier |= (entry >> 8);
              uint8_t key = entry & 0xFF;
              if (key && idx < 6)
                keycodes[idx++] = key;
            }
        tud_hid_keyboard_report(0, modifier, keycodes);
    }
}
