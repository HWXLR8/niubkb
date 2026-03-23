// src/tusb_config.h
#pragma once

// RP2040 device, full-speed
#define CFG_TUSB_RHPORT0_MODE   (OPT_MODE_DEVICE | OPT_MODE_FULL_SPEED)
#define CFG_TUSB_OS             OPT_OS_PICO

// EP0
#define CFG_TUD_ENDPOINT0_SIZE  64

// Memory alignment for USB DMA on RP2040
#define CFG_TUSB_MEM_SECTION
#define CFG_TUSB_MEM_ALIGN      __attribute__((aligned(4)))

// Enable only HID
#define CFG_TUD_HID             1
#define CFG_TUD_HID_EP_BUFSIZE  16
