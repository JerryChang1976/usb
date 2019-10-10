/*******************************************************************************
* Copyright (C) 2019 Microchip Technology Inc. and its subsidiaries.
*
* Subject to your compliance with these terms, you may use Microchip software
* and any derivatives exclusively with Microchip products. It is your
* responsibility to comply with third party license terms applicable to your
* use of third party software (including open source software) that may
* accompany Microchip software.
*
* THIS SOFTWARE IS SUPPLIED BY MICROCHIP "AS IS". NO WARRANTIES, WHETHER
* EXPRESS, IMPLIED OR STATUTORY, APPLY TO THIS SOFTWARE, INCLUDING ANY IMPLIED
* WARRANTIES OF NON-INFRINGEMENT, MERCHANTABILITY, AND FITNESS FOR A
* PARTICULAR PURPOSE.
*
* IN NO EVENT WILL MICROCHIP BE LIABLE FOR ANY INDIRECT, SPECIAL, PUNITIVE,
* INCIDENTAL OR CONSEQUENTIAL LOSS, DAMAGE, COST OR EXPENSE OF ANY KIND
* WHATSOEVER RELATED TO THE SOFTWARE, HOWEVER CAUSED, EVEN IF MICROCHIP HAS
* BEEN ADVISED OF THE POSSIBILITY OR THE DAMAGES ARE FORESEEABLE. TO THE
* FULLEST EXTENT ALLOWED BY LAW, MICROCHIP'S TOTAL LIABILITY ON ALL CLAIMS IN
* ANY WAY RELATED TO THIS SOFTWARE WILL NOT EXCEED THE AMOUNT OF FEES, IF ANY,
* THAT YOU HAVE PAID DIRECTLY TO MICROCHIP FOR THIS SOFTWARE.
*******************************************************************************/

/*******************************************************************************
  MPLAB Harmony Application Source File

  Company:
    Microchip Technology Inc.

  File Name:
    app.c

  Summary:
    This file contains the source code for the MPLAB Harmony application.

  Description:
    This file contains the source code for the MPLAB Harmony application.  It
    implements the logic of the application's state machine and it may call
    API routines of other MPLAB Harmony modules in the system, such as drivers,
    system services, and middleware.  However, it does not call any of the
    system interfaces (such as the "Initialize" and "Tasks" functions) of any of
    the modules in the system or make any assumptions about when those functions
    are called.  That is the responsibility of the configuration-specific system
    files.
 *******************************************************************************/

// *****************************************************************************
// *****************************************************************************
// Section: Included Files
// *****************************************************************************
// *****************************************************************************

#include "app.h"

// *****************************************************************************
// *****************************************************************************
// Section: Global Data Definitions
// *****************************************************************************
// *****************************************************************************
#define APP_USB_SWITCH_DEBOUNCE_COUNT 220 

/* Macro defines the conversion factor to be
 * multiplied to convert to millisecs*/
#define APP_USB_CONVERT_TO_MILLISECOND (1)
// *****************************************************************************
/* Application Data

  Summary:
    Holds application data

  Description:
    This structure holds the application's data.

  Remarks:
    This structure should be initialized by the APP_Initialize function.

    Application strings and buffers are be defined outside this structure.
*/

APP_DATA appData;

/* Mouse Report */
MOUSE_REPORT mouseReport __attribute__ ((aligned(16)));
MOUSE_REPORT mouseReportPrevious __attribute__ ((aligned(16)));


// *****************************************************************************
// *****************************************************************************
// Section: Application Callback Functions
// *****************************************************************************
// *****************************************************************************

void APP_USBDeviceHIDEventHandler(USB_DEVICE_HID_INDEX hidInstance,
        USB_DEVICE_HID_EVENT event, void * eventData, uintptr_t userData)
{
    APP_DATA * appData = (APP_DATA *)userData;

    switch(event)
    {
        case USB_DEVICE_HID_EVENT_REPORT_SENT:

            /* This means the mouse report was sent.
             We are free to send another report */

            appData->isMouseReportSendBusy = false;
            break;

        case USB_DEVICE_HID_EVENT_REPORT_RECEIVED:

            /* Dont care for other event in this demo */
            break;

        case USB_DEVICE_HID_EVENT_SET_IDLE:

             /* Acknowledge the Control Write Transfer */
           USB_DEVICE_ControlStatus(appData->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);

            /* save Idle rate received from Host */
            appData->idleRate = ((USB_DEVICE_HID_EVENT_DATA_SET_IDLE*)eventData)->duration;
            break;

        case USB_DEVICE_HID_EVENT_GET_IDLE:

            /* Host is requesting for Idle rate. Now send the Idle rate */
            USB_DEVICE_ControlSend(appData->deviceHandle, &(appData->idleRate),1);

            /* On successfully receiving Idle rate, the Host would acknowledge back with a
               Zero Length packet. The HID function driver returns an event
               USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT to the application upon
               receiving this Zero Length packet from Host.
               USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT event indicates this control transfer
               event is complete */

            break;

        case USB_DEVICE_HID_EVENT_SET_PROTOCOL:
            /* Host is trying set protocol. Now receive the protocol and save */
            appData->activeProtocol = *(USB_HID_PROTOCOL_CODE *)eventData;

              /* Acknowledge the Control Write Transfer */
            USB_DEVICE_ControlStatus(appData->deviceHandle, USB_DEVICE_CONTROL_STATUS_OK);
            break;

        case  USB_DEVICE_HID_EVENT_GET_PROTOCOL:

            /* Host is requesting for Current Protocol. Now send the Idle rate */
             USB_DEVICE_ControlSend(appData->deviceHandle, &(appData->activeProtocol), 1);

             /* On successfully receiving Idle rate, the Host would acknowledge
               back with a Zero Length packet. The HID function driver returns
               an event USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT to the
               application upon receiving this Zero Length packet from Host.
               USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT event indicates
               this control transfer event is complete */
             break;

        case USB_DEVICE_HID_EVENT_CONTROL_TRANSFER_DATA_SENT:
            break;

        default:
            break;
    }
}

/*******************************************************************************
  Function:
    void APP_USBDeviceEventHandler (USB_DEVICE_EVENT event,
        USB_DEVICE_EVENT_DATA * eventData)

  Summary:
    Event callback generated by USB device layer.

  Description:
    This event handler will handle all device layer events.

  Parameters:
    None.

  Returns:
    None.
*/

void APP_USBDeviceEventHandler(USB_DEVICE_EVENT event, void * eventData, uintptr_t context)
{
    USB_DEVICE_EVENT_DATA_CONFIGURED * configurationValue;
    switch(event)
    {
        case USB_DEVICE_EVENT_SOF:
            /* This event is used for switch debounce. This flag is reset
             * by the switch process routine. */
            appData.sofEventHasOccurred = true;
            appData.setIdleTimer++;
            break;
        case USB_DEVICE_EVENT_RESET:
        case USB_DEVICE_EVENT_DECONFIGURED:

            /* Device got deconfigured */

            appData.isConfigured = false;
            appData.isMouseReportSendBusy = false;
            appData.state = APP_STATE_WAIT_FOR_CONFIGURATION;
            appData.emulateMouse = true;
            LED1_Off();

            break;

        case USB_DEVICE_EVENT_CONFIGURED:

            /* Device is configured */
            configurationValue = (USB_DEVICE_EVENT_DATA_CONFIGURED *)eventData;
            if(configurationValue->configurationValue == 1)
            {
                appData.isConfigured = true;

                LED1_On();

                /* Register the Application HID Event Handler. */

                USB_DEVICE_HID_EventHandlerSet(appData.hidInstance,
                        APP_USBDeviceHIDEventHandler, (uintptr_t)&appData);
            }
            break;

        case USB_DEVICE_EVENT_POWER_DETECTED:

            /* VBUS was detected. We can attach the device */
            USB_DEVICE_Attach(appData.deviceHandle);
            break;

        case USB_DEVICE_EVENT_POWER_REMOVED:

            /* VBUS is not available any more. Detach the device. */
            USB_DEVICE_Detach(appData.deviceHandle);
            LED1_Off();
            break;

        case USB_DEVICE_EVENT_SUSPENDED:
            
            break;

        case USB_DEVICE_EVENT_RESUMED:
        case USB_DEVICE_EVENT_ERROR:
        default:
            break;

    }
}


// *****************************************************************************
// *****************************************************************************
// Section: Application Local Functions
// *****************************************************************************
// *****************************************************************************

/********************************************************
 * Application switch press routine
 ********************************************************/

void APP_ProcessSwitchPress(void)
{
    /* This function checks if the switch is pressed and then
     * debounces the switch press*/
    if(SWITCH_STATE_PRESSED == (SWITCH_Get()))
    {
        if(appData.ignoreSwitchPress)
        {
            /* This means the key press is in progress */
            if(appData.sofEventHasOccurred)
            {
                /* A timer event has occurred. Update the debounce timer */
                appData.switchDebounceTimer ++;
                appData.sofEventHasOccurred = false;
                if(appData.switchDebounceTimer == APP_USB_SWITCH_DEBOUNCE_COUNT)
                {
                    /* Indicate that we have valid switch press. The switch is
                     * pressed flag will be cleared by the application tasks
                     * routine. We should be ready for the next key press.*/
                    appData.isSwitchPressed = true;
                    appData.switchDebounceTimer = 0;
                    appData.ignoreSwitchPress = false;
                }
            }
        }
        else
        {
            /* We have a fresh key press */
            appData.ignoreSwitchPress = true;
            appData.switchDebounceTimer = 0;
        }
    }
    else
    {
        /* No key press. Reset all the indicators. */
        appData.ignoreSwitchPress = false;
        appData.switchDebounceTimer = 0;
        appData.sofEventHasOccurred = false;
    }
}


// *****************************************************************************
// *****************************************************************************
// Section: Application Initialization and State Machine Functions
// *****************************************************************************
// *****************************************************************************

/*******************************************************************************
  Function:
    void APP_Initialize ( void )

  Remarks:
    See prototype in app.h.
 */

void APP_Initialize ( void )
{
    /* Place the App state machine in its initial state. */
    appData.state = APP_STATE_INIT;

    appData.deviceHandle  = USB_DEVICE_HANDLE_INVALID;
    appData.isConfigured = false;
    appData.emulateMouse = true;
    appData.hidInstance = 0;
    appData.isMouseReportSendBusy = false;
    appData.isSwitchPressed = false;
    appData.ignoreSwitchPress = false;
}


/******************************************************************************
  Function:
    void APP_Tasks ( void )

  Remarks:
    See prototype in app.h.
 */
int8_t dir_table[8] ={-4,-4,-4, 0, 4, 4, 4, 0};

void APP_Tasks ( void )
{
    static int8_t   vector = 0;
    static uint8_t  movement_length = 0;
    static bool     sent_dont_move = false;


    /* Check the application's current state. */
    switch ( appData.state )
    {
        /* Application's initial state. */
        case APP_STATE_INIT:
        {
		    /* Open the device layer */
            appData.deviceHandle = USB_DEVICE_Open( USB_DEVICE_INDEX_0,
                    DRV_IO_INTENT_READWRITE );

            if(appData.deviceHandle != USB_DEVICE_HANDLE_INVALID)
            {
                /* Register a callback with device layer to get event notification (for end point 0) */
                USB_DEVICE_EventHandlerSet(appData.deviceHandle,
                        APP_USBDeviceEventHandler, 0);

                appData.state = APP_STATE_WAIT_FOR_CONFIGURATION;
            }
            else
            {
                /* The Device Layer is not ready to be opened. We should try
                 * again later. */
            }
            break;
        }

        case APP_STATE_WAIT_FOR_CONFIGURATION:

            /* Check if the device is configured. The
             * isConfigured flag is updated in the
             * Device Event Handler */

            if(appData.isConfigured)
            {
                appData.state = APP_STATE_MOUSE_EMULATE;
            }
            break;

        case APP_STATE_MOUSE_EMULATE:

            APP_ProcessSwitchPress();

            /* The following logic rotates the mouse icon when
             * a switch is pressed */

            if(appData.isSwitchPressed)
            {
                /* Toggle the mouse emulation with each switch press */
                appData.emulateMouse ^= 1;
                appData.isSwitchPressed = false;
            }

            if(appData.emulateMouse)
            {
                sent_dont_move = false;

                if(movement_length > 50)
                {
                    appData.mouseButton[0] = MOUSE_BUTTON_STATE_RELEASED;
                    appData.mouseButton[1] = MOUSE_BUTTON_STATE_RELEASED;
                    appData.xCoordinate =(int8_t)dir_table[vector & 0x07] ;
                    appData.yCoordinate =(int8_t)dir_table[(vector+2) & 0x07];
                    vector ++;
                    movement_length = 0;
                }
            }
            else
            {
                appData.mouseButton[0] = MOUSE_BUTTON_STATE_RELEASED;
                appData.mouseButton[1] = MOUSE_BUTTON_STATE_RELEASED;
                appData.xCoordinate = 0;
                appData.yCoordinate = 0;
            }

            if(!appData.isMouseReportSendBusy)
            {
                if(((sent_dont_move == false) && (!appData.emulateMouse)) || (appData.emulateMouse))
                {

                    /* This means we can send the mouse report. The
                       isMouseReportBusy flag is updated in the HID Event Handler. */

                    appData.isMouseReportSendBusy = true;

                    /* Create the mouse report */

                    MOUSE_ReportCreate(appData.xCoordinate, appData.yCoordinate,
                            appData.mouseButton, &mouseReport);

                    if(memcmp((const void *)&mouseReportPrevious, (const void *)&mouseReport,
                            (size_t)sizeof(mouseReport)) == 0)
                    {
                        /* Reports are same as previous report. However mouse reports
                         * can be same as previous report as the co-ordinate positions are relative.
                         * In that case it needs to be send */
                        if((appData.xCoordinate == 0) && (appData.yCoordinate == 0))
                        {
                            /* If the coordinate positions are 0, that means there
                             * is no relative change */
                            if(appData.idleRate == 0)
                            {
                                appData.isMouseReportSendBusy = false;
                            }
                            else
                            {
                                /* Check the idle rate here. If idle rate time elapsed
                                 * then the data will be sent. Idle rate resolution is
                                 * 4 msec as per HID specification; possible range is
                                 * between 4msec >= idlerate <= 1020 msec.
                                 */
                                if(appData.setIdleTimer * APP_USB_CONVERT_TO_MILLISECOND
                                        >= appData.idleRate * 4)
                                {
                                    /* Send REPORT as idle time has elapsed */
                                    appData.isMouseReportSendBusy = true;
                                }
                                else
                                {
                                    /* Do not send REPORT as idle time has not elapsed */
                                    appData.isMouseReportSendBusy = false;
                                }
                            }
                        }

                    }
                    if(appData.isMouseReportSendBusy == true)
                    {
                        /* Copy the report sent to previous */
                        memcpy((void *)&mouseReportPrevious, (const void *)&mouseReport,
                                (size_t)sizeof(mouseReport));

                        /* Send the mouse report. */
                        USB_DEVICE_HID_ReportSend(appData.hidInstance,
                            &appData.reportTransferHandle, (uint8_t*)&mouseReport,
                            sizeof(MOUSE_REPORT));
                        appData.setIdleTimer = 0;
                    }
                    movement_length ++;
                    sent_dont_move = true;
                }
            }

            break;

        case APP_STATE_ERROR:

            break;

        /* The default state should never be executed. */
        default:
        {
            /* TODO: Handle error in application's state machine. */
            break;
        }
    }
}


/*******************************************************************************
 End of File
 */

