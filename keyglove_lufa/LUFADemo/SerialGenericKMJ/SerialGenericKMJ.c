/*
             LUFA Library
     Copyright (C) Dean Camera, 2011.

  dean [at] fourwalledcubicle [dot] com
           www.lufa-lib.org
*/

/*
  Copyright 2011  Dean Camera (dean [at] fourwalledcubicle [dot] com)
  Copyright 2010  Denver Gingerich (denver [at] ossguy [dot] com)

  Permission to use, copy, modify, distribute, and sell this
  software and its documentation for any purpose is hereby granted
  without fee, provided that the above copyright notice appear in
  all copies and that both that the copyright notice and this
  permission notice and warranty disclaimer appear in supporting
  documentation, and that the name of the author not be used in
  advertising or publicity pertaining to distribution of the
  software without specific, written prior permission.

  The author disclaim all warranties with regard to this
  software, including all implied warranties of merchantability
  and fitness.  In no event shall the author be liable for any
  special, indirect or consequential damages or any damages
  whatsoever resulting from loss of use, data or profits, whether
  in an action of contract, negligence or other tortious action,
  arising out of or in connection with the use or performance of
  this software.
*/

/** \file
 *
 *  Main source file for the Virtual Serial + Generic HID Keyboard/Mouse/Joystick demo code.
 */

#include "SerialGenericKMJ.h"

/** Contains the current baud rate and other settings of the virtual serial port. While USB virtual serial does not use
 *  the physical USART and thus does not use these settings, they must still be retained and returned to the host
 *  upon request or the host will assume the device is non-functional.
 *
 *  These values are set by the host via a class-specific request, however they are not required to be used accurately.
 *  It is possible to completely ignore these value or use other settings as the host is completely unaware of the physical
 *  serial link characteristics and instead sends and receives data in endpoint streams.
 */
static CDC_LineEncoding_t LineEncodingData = { .BaudRateBPS = 0,
                                           .CharFormat  = CDC_LINEENCODING_OneStopBit,
                                           .ParityType  = CDC_PARITY_None,
                                           .DataBits    = 8                            };

/** Circular buffer to hold data from the host before it is processed on the AVR. */
static RingBuffer_t HostRXSerial_Buffer;

/** Underlying data buffer for \ref HostRXSerial_Buffer, where the stored bytes are located. */
static uint8_t      HostRXSerial_Buffer_Data[128];

/** Circular buffer to hold data from the from the AVR before it is sent to the host. */
static RingBuffer_t HostTXSerial_Buffer;

/** Underlying data buffer for \ref HostTXSerial_Buffer, where the stored bytes are located. */
static uint8_t      HostTXSerial_Buffer_Data[128];

/** Static buffer to hold the HID reports to and from the host. */
static uint8_t HIDReportInData[GENERIC_REPORT_SIZE];
static uint8_t HIDReportOutData[GENERIC_REPORT_SIZE]; // extra byte for the ReportID value

uint32_t tickCounter = 0;
uint32_t outputCounter = 1;
bool HostSerialLocalEcho = true;

/** Main program entry point. This routine configures the hardware required by the application, then
 *  enters a loop to run the application tasks in sequence.
 */
int8_t main(void)
{
    RingBuffer_InitBuffer(&HostRXSerial_Buffer, HostRXSerial_Buffer_Data, sizeof(HostRXSerial_Buffer_Data));
    RingBuffer_InitBuffer(&HostTXSerial_Buffer, HostTXSerial_Buffer_Data, sizeof(HostTXSerial_Buffer_Data));

    SetupHardware();

    LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
    sei();

    for (;;)
    {
        if (tickCounter % 10000 == 1000)
        {
            // virtual serial demo
            // (sends 'Tick [n]' where [n] is between 0 and 9 sequentially, looping back after 9)
            RingBuffer_Insert(&HostTXSerial_Buffer, 'T');
            RingBuffer_Insert(&HostTXSerial_Buffer, 'i');
            RingBuffer_Insert(&HostTXSerial_Buffer, 'c');
            RingBuffer_Insert(&HostTXSerial_Buffer, 'k');
            RingBuffer_Insert(&HostTXSerial_Buffer, ' ');
            RingBuffer_Insert(&HostTXSerial_Buffer, 48 + (((tickCounter - 1000) / 10000) % 10));
            RingBuffer_Insert(&HostTXSerial_Buffer, 10); // \r
            RingBuffer_Insert(&HostTXSerial_Buffer, 13); // \n
        }
        else if (tickCounter % 10000 == 3000)
        {
            // mouse demo
            // (moves mouse cursor down and right by 5px, and scrolls down 1 unit also)
            HIDReportOutData[0] = HID_REPORTID_MouseReport;
            HIDReportOutData[1] = 0; // buttons
            HIDReportOutData[2] = 10; // x movement
            HIDReportOutData[3] = 10; // y movement
            HIDReportOutData[4] = -1; // z movement (optional, scrolling)
            HIDReportOutData[5] = 0; // ]
            HIDReportOutData[6] = 0; // ] - unused
            HIDReportOutData[7] = 0; // ]
            HIDReportOutData[8] = 0; // ]
        }
        else if (tickCounter % 10000 == 5000)
        {
            // keyboard demo start
            // (presses 'a', 'b', 'c', etc. keys sequentially, looping back after z)
            HIDReportOutData[0] = HID_REPORTID_KeyboardReport;
            HIDReportOutData[1] = 0; // modifiers
            HIDReportOutData[2] = 0; // RESERVED
            HIDReportOutData[3] = 4 + (((tickCounter - 1000) / 10000) % 26); // key code [0]
            HIDReportOutData[4] = 0; // key code [1]
            HIDReportOutData[5] = 0; // key code [2]
            HIDReportOutData[6] = 0; // key code [3]
            HIDReportOutData[7] = 0; // key code [4]
            HIDReportOutData[8] = 0; // key code [5]
        }
        else if (tickCounter % 10000 == 6000)
        {
            // keyboard demo end
            // (releases previous keypresses, endless repeats otherwise)
            HIDReportOutData[0] = HID_REPORTID_KeyboardReport;
            HIDReportOutData[1] = 0; // modifiers
            HIDReportOutData[2] = 0; // RESERVED
            HIDReportOutData[3] = 0; // key code [0]
            HIDReportOutData[4] = 0; // key code [1]
            HIDReportOutData[5] = 0; // key code [2]
            HIDReportOutData[6] = 0; // key code [3]
            HIDReportOutData[7] = 0; // key code [4]
            HIDReportOutData[8] = 0; // key code [5]
        }
        else if (tickCounter % 10000 == 7000)
        {
            // joystick demo start
            // (moves left axis down/right, right axis up/left, and presses a button)
            HIDReportOutData[0] = HID_REPORTID_JoystickReport;
            HIDReportOutData[1] = 1; // buttons 1
            HIDReportOutData[2] = 0; // buttons 2
            HIDReportOutData[3] = 5; // left x axis
            HIDReportOutData[4] = 5; // left y axis
            HIDReportOutData[5] = -5; // right x axis
            HIDReportOutData[6] = -5; // right y exis
            HIDReportOutData[7] = 0; // ] - unused
            HIDReportOutData[8] = 0; // ]
        }
        else if (tickCounter % 10000 == 8000)
        {
            // joystick demo end
            // (releases previous joystick actions)
            HIDReportOutData[0] = HID_REPORTID_JoystickReport;
            HIDReportOutData[1] = 0; // buttons 1
            HIDReportOutData[2] = 0; // buttons 2
            HIDReportOutData[3] = 0; // left x axis
            HIDReportOutData[4] = 0; // left y axis
            HIDReportOutData[5] = 0; // right x axis
            HIDReportOutData[6] = 0; // right y exis
            HIDReportOutData[7] = 0; // ] - unused
            HIDReportOutData[8] = 0; // ]
        }

        CDC_Task();
        HID_Task();
        USB_USBTask();
        tickCounter++;
    }
}

/** Configures the board hardware and chip peripherals for the demo's functionality. */
void SetupHardware(void)
{
    /* Disable watchdog if enabled by bootloader/fuses */
    MCUSR &= ~(1 << WDRF);
    wdt_disable();

    /* Disable clock division */
    clock_prescale_set(clock_div_1);

    /* Hardware Initialization */
    LEDs_Init();
    USB_Init();
}

/** Event handler for the USB_Connect event. This indicates that the device is enumerating via the status LEDs and
 *  starts the library USB task to begin the enumeration and USB management process.
 */
void EVENT_USB_Device_Connect(void)
{
    /* Indicate USB enumerating */
    LEDs_SetAllLEDs(LEDMASK_USB_ENUMERATING);
}

/** Event handler for the USB_Disconnect event. This indicates that the device is no longer connected to a host via
 *  the status LEDs and stops the USB management task.
 */
void EVENT_USB_Device_Disconnect(void)
{
    /* Indicate USB not ready */
    LEDs_SetAllLEDs(LEDMASK_USB_NOTREADY);
}

/** Event handler for the USB_ConfigurationChanged event. This is fired when the host sets the current configuration
 *  of the USB device after enumeration, and configures the keyboard and mouse device endpoints.
 */
void EVENT_USB_Device_ConfigurationChanged(void)
{
    bool ConfigSuccess = true;

    //* Setup CDC Data Endpoints */
    ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_NOTIFICATION_EPNUM, EP_TYPE_INTERRUPT, ENDPOINT_DIR_IN,
                                                CDC_NOTIFICATION_EPSIZE, ENDPOINT_BANK_SINGLE);
    ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_TX_EPNUM, EP_TYPE_BULK, ENDPOINT_DIR_IN,
                                                CDC_TXRX_EPSIZE, ENDPOINT_BANK_SINGLE);
    ConfigSuccess &= Endpoint_ConfigureEndpoint(CDC_RX_EPNUM, EP_TYPE_BULK, ENDPOINT_DIR_OUT,
                                                CDC_TXRX_EPSIZE, ENDPOINT_BANK_SINGLE);

    /* Reset line encoding baud rate so that the host knows to send new values */
    LineEncodingData.BaudRateBPS = 0;

    /* Setup HID Report Endpoints */
    ConfigSuccess &= Endpoint_ConfigureEndpoint(GENERIC_IN_EPNUM, EP_TYPE_INTERRUPT, ENDPOINT_DIR_IN,
                                                GENERIC_EPSIZE, ENDPOINT_BANK_SINGLE);
    ConfigSuccess &= Endpoint_ConfigureEndpoint(GENERIC_OUT_EPNUM, EP_TYPE_INTERRUPT, ENDPOINT_DIR_OUT,
                                                GENERIC_EPSIZE, ENDPOINT_BANK_SINGLE);

    /* Indicate endpoint configuration success or failure */
    LEDs_SetAllLEDs(ConfigSuccess ? LEDMASK_USB_READY : LEDMASK_USB_ERROR);
}

/** Event handler for the USB_ControlRequest event. This is used to catch and process control requests sent to
 *  the device from the USB host before passing along unhandled control requests to the library for processing
 *  internally.
 */
void EVENT_USB_Device_ControlRequest(void)
{
    switch (USB_ControlRequest.bRequest)
    {
        case HID_REQ_GetReport:
            if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                //CreateHIDReport(HIDReportOutData);

                Endpoint_ClearSETUP();

                /* Write the report data to the control endpoint */
                Endpoint_Write_Control_Stream_LE(&HIDReportOutData, sizeof(HIDReportOutData));

                /* Clean out report data after sending */
                memset(&HIDReportOutData, 0, GENERIC_REPORT_SIZE + 1);

                /* Finalize the stream transfer to send the last packet */
                Endpoint_ClearOUT();
            }

            break;
        case HID_REQ_SetReport:
            if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                Endpoint_ClearSETUP();

                /* Read the report data from the control endpoint */
                Endpoint_Read_Control_Stream_LE(&HIDReportInData, sizeof(HIDReportInData));
                Endpoint_ClearIN();

                ProcessHIDReport(HIDReportInData);
            }

            break;
        case CDC_REQ_GetLineEncoding:
            if (USB_ControlRequest.bmRequestType == (REQDIR_DEVICETOHOST | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                Endpoint_ClearSETUP();

                /* Write the line coding data to the control endpoint */
                Endpoint_Write_Control_Stream_LE(&LineEncodingData, sizeof(CDC_LineEncoding_t));
                Endpoint_ClearOUT();
            }

            break;
        case CDC_REQ_SetLineEncoding:
            if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                Endpoint_ClearSETUP();

                /* Read the line coding data in from the host into the global struct */
                Endpoint_Read_Control_Stream_LE(&LineEncodingData, sizeof(CDC_LineEncoding_t));
                Endpoint_ClearIN();
            }

            break;
        case CDC_REQ_SetControlLineState:
            if (USB_ControlRequest.bmRequestType == (REQDIR_HOSTTODEVICE | REQTYPE_CLASS | REQREC_INTERFACE))
            {
                Endpoint_ClearSETUP();
                Endpoint_ClearStatusStage();

                /* NOTE: Here you can read in the line state mask from the host, to get the current state of the output handshake
                         lines. The mask is read in from the wValue parameter in USB_ControlRequest, and can be masked against the
                         CONTROL_LINE_OUT_* masks to determine the RTS and DTR line states using the following code:
                */
            }

            break;
    }
}

/** Processes a given Keyboard LED report from the host, and sets the board LEDs to match. Since the Keyboard
 *  LED report can be sent through either the control endpoint (via a HID SetReport request) or the HID OUT
 *  endpoint, the processing code is placed here to avoid duplicating it and potentially having different
 *  behaviour depending on the method used to sent it.
 */
void Keyboard_ProcessLEDReport(const uint8_t LEDStatus)
{
    uint8_t LEDMask = LEDS_LED2;

    if (LEDStatus & HID_KEYBOARD_LED_NUMLOCK)
      LEDMask |= LEDS_LED2;

    if (LEDStatus & HID_KEYBOARD_LED_CAPSLOCK)
      LEDMask |= LEDS_LED3;

    if (LEDStatus & HID_KEYBOARD_LED_SCROLLLOCK)
      LEDMask |= LEDS_LED4;

    /* Set the status LEDs to the current Keyboard LED status */
    LEDs_SetAllLEDs(LEDMask);
}

/** Function to manage CDC data transmission and reception to and from the host. */
void CDC_Task(void)
{
    /* Device must be connected and configured for the task to run */
    if (USB_DeviceState != DEVICE_STATE_Configured)
        return;

    uint16_t BufferCount;

    /* Send data packet if anything is waiting */
    BufferCount = RingBuffer_GetCount(&HostTXSerial_Buffer);
    if (BufferCount && LineEncodingData.BaudRateBPS)
    {
        PORTD |= (1 << 6);
        /* Select the Serial TX Endpoint */
        Endpoint_SelectEndpoint(CDC_TX_EPNUM);

        /* Write the byte(s) to the Endpoint */
        for (uint16_t i = 0; i < CDC_TXRX_EPSIZE && BufferCount; i++, BufferCount--)
        {
            UEDATX = RingBuffer_Remove(&HostTXSerial_Buffer); // write one byte
        }

        /* Remember if the packet to send completely fills the endpoint */
        bool IsFull = (Endpoint_BytesInEndpoint() == CDC_TXRX_EPSIZE);

        /* Finalize the stream transfer to send the last packet */
        Endpoint_ClearIN();

        /* If the last packet filled the endpoint, send an empty packet to release the buffer on
         * the receiver (otherwise all data will be cached until a non-full packet is received) */
        if (IsFull)
        {
            /* Wait until the endpoint is ready for another packet */
            Endpoint_WaitUntilReady();

            /* Send an empty packet to ensure that the host does not buffer data sent to it */
            Endpoint_ClearIN();
        }
        PORTD &= ~(1 << 6);
    }

    /* Select the Serial RX Endpoint */
    Endpoint_SelectEndpoint(CDC_RX_EPNUM);

    /* Store any received data from the host */
    if (Endpoint_IsOUTReceived())
    {
        BufferCount = Endpoint_BytesInEndpoint();
        for (uint16_t i = 0; i < BufferCount; i++)
        {
            uint8_t b = UEDATX;
            RingBuffer_Insert(&HostRXSerial_Buffer, b); // read byte from endpoint

            // insert this byte into the TX buffer if local echo is enabled
            if (HostSerialLocalEcho)
            {
                RingBuffer_Insert(&HostTXSerial_Buffer, b);
            }
        }

        /* Finalize the stream transfer to receive the last packet */
        Endpoint_ClearOUT();
    }

}

/** Function to process the lest received report from the host.
 *
 *  \param[in] DataArray  Pointer to a buffer where the last report data is stored
 */
void ProcessHIDReport(uint8_t* DataArray)
{
    /*
        This is where you need to process the reports being sent from the host to the device.
        DataArray is an array holding the last report from the host. This function is called
        each time the host has sent a report to the device.
    */

}

void HID_Task(void)
{
    /* Device must be connected and configured for the task to run */
    if (USB_DeviceState != DEVICE_STATE_Configured)
      return;

    Endpoint_SelectEndpoint(GENERIC_OUT_EPNUM);

    /* Check to see if a packet has been sent from the host */
    if (Endpoint_IsOUTReceived())
    {
        /* Check to see if the packet contains data */
        if (Endpoint_IsReadWriteAllowed())
        {
            /* Read Generic Report Data */
            Endpoint_Read_Stream_LE(&HIDReportInData, sizeof(HIDReportInData), NULL);

            /* Process Generic Report Data */
            ProcessHIDReport(HIDReportOutData);
        }

        /* Finalize the stream transfer to send the last packet */
        Endpoint_ClearOUT();
    }

    Endpoint_SelectEndpoint(GENERIC_IN_EPNUM);

    /* Check to see if the host is ready to accept another packet */
    if (Endpoint_IsINReady())
    {
        /* Write Generic Report Data */
        Endpoint_Write_Stream_LE(&HIDReportOutData, sizeof(HIDReportOutData), NULL);
        
        /* Clean out report data after sending */
        memset(&HIDReportOutData, 0, GENERIC_REPORT_SIZE + 1);

        /* Finalize the stream transfer to send the last packet */
        Endpoint_ClearIN();
    }
}

