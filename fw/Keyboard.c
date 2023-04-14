/*
             LUFA Library
     Copyright (C) Dean Camera, 2021.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2021  Dean Camera (dean [at] fourwalledcubicle [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaims all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

#include "Keyboard.h"

/** Buffer to hold the previously generated Keyboard HID report, for comparison purposes inside the HID class driver. */
static uint8_t PrevKeyboardHIDReportBuffer[sizeof(USB_KeyboardReport_Data_t)];

/** LUFA HID Class driver interface configuration and state information. This structure is
 *  passed to all HID Class driver functions, so that multiple instances of the same class
 *  within a device can be differentiated from one another.
 */
USB_ClassInfo_HID_Device_t Keyboard_HID_Interface =
  {
    .Config = {
      .InterfaceNumber = INTERFACE_ID_Keyboard,
      .ReportINEndpoint = {
	.Address = KEYBOARD_EPADDR,
	.Size = KEYBOARD_EPSIZE,
	.Banks = 1,
      },
      .PrevReportINBuffer = PrevKeyboardHIDReportBuffer,
      .PrevReportINBufferSize = sizeof(PrevKeyboardHIDReportBuffer),
    },
  };


int main(void)
{
	SetupHardware();

	GlobalInterruptEnable();

	// setup matrix
	uint8_t num_rows = 4;
	uint8_t num_cols = 6;
	matrix.num_rows = num_rows;
	matrix.num_cols = num_cols;
	pin_t rows[4] = {d1, d0, d4, c6};
	pin_t cols[6] =	{b2, b5, b4, e6, d7, b6};
	for (int i = 0; i < num_rows; ++i) {
	  matrix.rows[i] = rows[i];
	}
	for (int i = 0; i < num_cols; ++i) {
	  matrix.cols[i] = cols[i];
	}

	// set all rows at outputs
	DDRD |= _BV(1);
	DDRD |= _BV(0);
	DDRD |= _BV(4);
	DDRC |= _BV(6);
	// enable pull-up on all rows
	for (uint8_t i = 0; i < matrix.num_rows; ++i) {
	  setPinHigh(matrix.rows[i]);
	}
	// set all columns as inputs
	DDRB &= ~_BV(2); // B2
	DDRB &= ~_BV(5); // B5
	DDRB &= ~_BV(4); // B4
	DDRE &= ~_BV(6); // E6
	DDRD &= ~_BV(7); // D7
	DDRB &= ~_BV(6); // B6
	// enable pull-up on all columns
	for (uint8_t i = 0; i < matrix.num_cols; ++i) {
	  setPinHigh(matrix.cols[i]);
	}

	for (;;) {
	  for (uint8_t i = 0; i < matrix.num_rows; ++i) {
	    setPinLow(matrix.rows[i]);
	    matrix.active_col = i;
	    HID_Device_USBTask(&Keyboard_HID_Interface);
	    USB_USBTask();
	    setPinHigh(matrix.rows[i]);
	  }
	}
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware()
{
#if (ARCH == ARCH_AVR8)
	/* Disable watchdog if enabled by bootloader/fuses */
	MCUSR &= ~(1 << WDRF);
	wdt_disable();

	/* Disable clock division */
	clock_prescale_set(clock_div_1);
#elif (ARCH == ARCH_XMEGA)
	/* Start the PLL to multiply the 2MHz RC oscillator to 32MHz and switch the CPU core to run from it */
	XMEGACLK_StartPLL(CLOCK_SRC_INT_RC2MHZ, 2000000, F_CPU);
	XMEGACLK_SetCPUClockSource(CLOCK_SRC_PLL);

	/* Start the 32MHz internal RC oscillator and start the DFLL to increase it to 48MHz using the USB SOF as a reference */
	XMEGACLK_StartInternalOscillator(CLOCK_SRC_INT_RC32MHZ);
	XMEGACLK_StartDFLL(CLOCK_SRC_INT_RC32MHZ, DFLL_REF_INT_USBSOF, F_USB);

	PMIC.CTRL = PMIC_LOLVLEN_bm | PMIC_MEDLVLEN_bm | PMIC_HILVLEN_bm;
#endif

	/* Hardware Initialization */
	USB_Init();
}

/** Event handler for the library USB Connection event. */
void EVENT_USB_Device_Connect(void) { }

/** Event handler for the library USB Disconnection event. */
void EVENT_USB_Device_Disconnect(void) { }

/** Event handler for the library USB Configuration Changed event. */
void EVENT_USB_Device_ConfigurationChanged(void)
{
	bool ConfigSuccess = true;

	ConfigSuccess &= HID_Device_ConfigureEndpoints(&Keyboard_HID_Interface);

	USB_Device_EnableSOFEvents();
}

/** Event handler for the library USB Control Request reception event. */
void EVENT_USB_Device_ControlRequest(void)
{
	HID_Device_ProcessControlRequest(&Keyboard_HID_Interface);
}

/** Event handler for the USB device Start Of Frame event. */
void EVENT_USB_Device_StartOfFrame(void)
{
	HID_Device_MillisecondElapsed(&Keyboard_HID_Interface);
}

/** HID class driver callback function for the creation of HID reports to the host.
 *
 *  \param[in]     HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in,out] ReportID    Report ID requested by the host if non-zero, otherwise callback should set to the generated report ID
 *  \param[in]     ReportType  Type of the report to create, either HID_REPORT_ITEM_In or HID_REPORT_ITEM_Feature
 *  \param[out]    ReportData  Pointer to a buffer where the created report should be stored
 *  \param[out]    ReportSize  Number of bytes written in the report (or zero if no report is to be sent)
 *
 *  \return Boolean \c true to force the sending of the report, \c false to let the library determine if it needs to be sent
 */
bool CALLBACK_HID_Device_CreateHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                         uint8_t* const ReportID,
                                         const uint8_t ReportType,
                                         void* ReportData,
                                         uint16_t* const ReportSize)
{
	USB_KeyboardReport_Data_t* KeyboardReport = (USB_KeyboardReport_Data_t*)ReportData;

	uint8_t UsedKeyCodes = 0;
	uint8_t offset = matrix.active_col * matrix.num_cols;
	// check columns one at a time to determine which key is pressed
	for (uint8_t i = 0; i < matrix.num_cols; ++i) {
	  if (isPinLow(matrix.cols[i])) {
	    KeyboardReport->KeyCode[UsedKeyCodes++] = layout[i+offset];
	  }
	}

	/* if (UsedKeyCodes) */
	/*   KeyboardReport->Modifier = HID_KEYBOARD_MODIFIER_LEFTSHIFT; */

	*ReportSize = sizeof(USB_KeyboardReport_Data_t);
	return false;
}

/** HID class driver callback function for the processing of HID reports from the host.
 *
 *  \param[in] HIDInterfaceInfo  Pointer to the HID class interface configuration structure being referenced
 *  \param[in] ReportID    Report ID of the received report from the host
 *  \param[in] ReportType  The type of report that the host has sent, either HID_REPORT_ITEM_Out or HID_REPORT_ITEM_Feature
 *  \param[in] ReportData  Pointer to a buffer where the received report has been stored
 *  \param[in] ReportSize  Size in bytes of the received HID report
 */
void CALLBACK_HID_Device_ProcessHIDReport(USB_ClassInfo_HID_Device_t* const HIDInterfaceInfo,
                                          const uint8_t ReportID,
                                          const uint8_t ReportType,
                                          const void* ReportData,
                                          const uint16_t ReportSize) {}

void setPinLow(pin_t pin) {
  *(pin.port) &= ~_BV(pin.pin_num);
}

void setPinHigh(pin_t pin) {
  *(pin.port) |= _BV(pin.pin_num);
}

bool isPinLow(pin_t pin) {
  return !(*(pin.pin) & _BV(pin.pin_num));
}
