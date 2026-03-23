Custom FW for Haute42 S16.

### Input Latency
Average latency is `0.814 ms` measured using the methodology described on [inputlag.science](http://inputlag.science)

### Feature list

| Feature              | State | Notes                                       |
|:---------------------|:-----:|:--------------------------------------------|
| HID Gamepad          | ✅    | Tested on Linux and Windows (DirectInput).  |
| HID Keyboard         | ❌    |                                             |
| XInput               | ✅    |                                             |
| 1000Hz Polling       | ✅    |                                             |
| Button Remap         | ✅    |                                             |
| Button Debounce      | ✅    | Leading-edge, configurable per-button.      |
| SOCD Cleaning        | ⚠️    | Neutral/Last-input only.                    |
| LED Support          | ✅    | Per-button LED config for idle/press color. |
| LED Brightness       | ⚠️    | On/off toggle only.                         |
| LED Animations       | ⚠️    | Button illumination on press only.          |
| OLED Support         | ✅    |                                             |
| OLED Images          | ✅    |                                             |
| OLED Animations      | ✅    |                                             |
| Live config          | ⚠️    | Turbo, LED/OLED toggle only.                |
| Persistent Settings  | ⚠️    | Does not save turbo settings.               |
| Turbo                | ✅    | 15.6/31.25/62.5Hz configurable per button.  |

| Platfrom Support     | State | Notes                                       |
|:---------------------|:-----:|:--------------------------------------------|
| Linux                | ✅    |                                             |
| Windows              | ✅    | DirectInput and XInput supported.           |
| MacOS                | ⚠️    | Untested, likely works.                     |
| MiSTer FPGA          | ✅    |                                             |
| Nintendo Switch      | ❌    |                                             |
| Nintendo Switch 2    | ❌    |                                             |
| Xbox 360             | ✅    |                                             |
| PS1                  | ❌    |                                             |
| PS2                  | ❌    |                                             |
| PS3                  | ❌    |                                             |
| PS4                  | ❌    |                                             |
| PS5                  | ❌    |                                             |

### Button Combos

| Combo             | Action                                      |
|:------------------|:--------------------------------------------|
| `TURBO + B13`     | Toggle LEDs on/off (hold 1s)                |
| `TURBO + B14`     | Toggle OLED on/off (hold 1s)                |
| `TURBO + B15`     | Toggle USB mode (HID/XInput, hold 1s)       |
| `TURBO + B16`     | Save settings to flash (hold 1s)            |
| `TURBO`           | Enter turbo config mode (hold 1s)           |
| `B8 + B9`         | Enter bootloader (instant)                  |

**Note:** USB mode changes take effect on next boot.