/*******************************************************************************
   USB Host Port Driver Core Routines

   Company:
    Microchip Technology Inc.

   File Name:
    drv_usb_uhp.c

   Summary:
    USB Host port Driver Implementation 


   Description:
    The USB device driver provides a simple interface to manage the USB
    modules on Microchip microcontrollers.  This file Implements the core
    interface routines for the USB driver.

    While building the driver from source, ALWAYS use this file in the build.
*******************************************************************************/

//DOM-IGNORE-BEGIN
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
//DOM-IGNORE-END

// *****************************************************************************
// *****************************************************************************
// Section: Include Files
// *****************************************************************************
// *****************************************************************************

#include "definitions.h"

#include "driver/usb/drv_usb.h"

#include "driver/usb/uhp/src/drv_usb_uhp_local.h"
#include "drv_usb_uhp_ohci_host.h"
#include "drv_usb_uhp_ehci_host.h"

extern __ALIGNED(4096) uint8_t USBBufferAligned[USB_HOST_TRANSFERS_NUMBER*64]; /* 4K page aligned, see Table 3-17. qTD Buffer Pointer(s) (DWords 3-7) */

/************************************
 * Prototype
 ***********************************/
void DRV_USB_UHP_HOST_StartOfFrameControl(DRV_HANDLE client, bool control);
USB_SPEED DRV_USB_UHP_HOST_DeviceCurrentSpeedGet(DRV_HANDLE client);
void DRV_USB_UHP_Deinitialize(const SYS_MODULE_INDEX object);
void UHPHS_Handler(void);
bool DRV_USB_UHP_HOST_Resume(DRV_HANDLE handle);
bool DRV_USB_UHP_HOST_Suspend(DRV_HANDLE handle);

/************************************
 * Driver instance object
 ***********************************/

DRV_USB_UHP_OBJ gDrvUSBObj[DRV_USB_UHP_INSTANCES_NUMBER];

/*****************************************
* Pool of pipe objects that is used by
* all driver instances.
*****************************************/
DRV_USB_UHP_HOST_PIPE_OBJ gDrvUSBHostPipeObj[DRV_USB_UHP_PIPES_NUMBER];

// *****************************************************************************
/* Function:
    SYS_MODULE_OBJ DRV_USB_UHP_Initialize
    (
       const SYS_MODULE_INDEX index,
       const SYS_MODULE_INIT * const init
    )

   Summary:
    Dynamic implementation of DRV_USB_UHP_Initialize system interface function.

   Description:
    This is the dynamic implementation of DRV_USB_UHP_Initialize system interface
    function. Function performs the following task:
    - Initializes the necessary USB module as per the instance init data
    - Updates internal data structure for the particular USB instance
    - Returns the USB instance value as a handle to the system

   Remarks:
    See drv_uhp.h for usage information.
 */
SYS_MODULE_OBJ DRV_USB_UHP_Initialize
(
    const SYS_MODULE_INDEX       drvIndex,
    const SYS_MODULE_INIT *const init
)
{
    DRV_USB_UHP_OBJ          *drvObj;
    DRV_USB_UHP_INIT           *usbInit;
    SYS_MODULE_OBJ             retVal = SYS_MODULE_OBJ_INVALID;

    SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\033[0m\n\rUSB HOST UHP example");

    if (drvIndex >= DRV_USB_UHP_INSTANCES_NUMBER)
    {
        /* The driver module index specified does not exist in the system */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Invalid Driver Module Index in DRV_USB_UHP_Initialize().");
    }
    else if (gDrvUSBObj[drvIndex].inUse == true)
    {
        /* Cannot initialize an object that is already in use. */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Driver is already initialized in DRV_USB_UHP_Initialize().");
    }
    else
    {
        usbInit = (DRV_USB_UHP_INIT *)init;
        drvObj  = &gDrvUSBObj[drvIndex];

        /* Try creating the global mutex. */
        if (OSAL_RESULT_TRUE != OSAL_MUTEX_Create(&drvObj->mutexID))
        {
            /* Could not create the mutual exclusion */
            SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB UHP: Could not create Mutex in DRV_USB_UHP_Initialize().");
        }
        else
        {
            /* Populate the driver object with the required data */
            drvObj->inUse     = true;
            drvObj->status    = SYS_STATUS_BUSY;
            drvObj->usbIDEHCI = ((uhphs_registers_t*)UHPHS_EHCI_ADDR); /* EHCI */
            drvObj->usbIDOHCI = ((UhpOhci*)UHPHS_OHCI_ADDR); /* OHCI */

            /* Assign the endpoint table */
            drvObj->interruptSource = usbInit->interruptSource;

            /* The root hub information is applicable for host mode operation. */
            drvObj->rootHubInfo.rootHubAvailableCurrent = usbInit->rootHubAvailableCurrent;
            drvObj->rootHubInfo.portIndication          = usbInit->portIndication;
            drvObj->rootHubInfo.portOverCurrentDetect   = usbInit->portOverCurrentDetect;
            drvObj->rootHubInfo.portPowerEnable         = usbInit->portPowerEnable;

            drvObj->isOpened = false;
            drvObj->pEventCallBack = NULL;

            drvObj->sessionInvalidEventSent = false;

            PMC_UCKR_UPLLEN();

            PMC_REGS->PMC_PCR = PMC_PCR_PID(drvObj->interruptSource);
            PMC_REGS->PMC_PCR = PMC_PCR_PID(drvObj->interruptSource) | PMC_PCR_CMD_Msk | PMC_PCR_EN_Msk | PMC_PCR_GCKCSS_UPLL_CLK;

            /* Set the state to indicate that the delay will be started */
            drvObj->state = DRV_USB_UHP_TASK_STATE_WAIT_FOR_CLOCK_USABLE;
            retVal        = (SYS_MODULE_OBJ)drvIndex;
        }
        _DRV_USB_UHP_InterruptSourceEnable(drvObj->interruptSource);
    }
    return(retVal);
} /* end of DRV_USB_UHP_Initialize() */




/* Function:
    void _DRV_USB_UHP_HOST_ResetStateMachine(DRV_USB_UHP_OBJ *hDriver)

   Summary:
    Reset State Machine

   Description:
    Reset State Machine 

   Remarks:
    Refer to .h for usage information.
 */
void _DRV_USB_UHP_HOST_ResetStateMachine(DRV_USB_UHP_OBJ *hDriver)
{
    volatile UhpOhci *usbIDOHCI = hDriver->usbIDOHCI;
    volatile uhphs_registers_t *usbIDEHCI = hDriver->usbIDEHCI;

    /* Check if reset is needed */
    switch (hDriver->resetState)
    {
        case DRV_USB_UHP_HOST_RESET_STATE_NO_RESET:
            /* No reset signaling is request */
            break;

        case DRV_USB_UHP_HOST_RESET_STATE_START:
            /* Trigger USB Reset */
            USB_UHP_ResetEnableEhci(hDriver);
            /* Delay to be added for correct Low Speed switching problem */
            /* Delta T4 */
            if (SYS_TIME_DelayMS( 70 /*ms Delta T4*/, &hDriver->timerHandle) == SYS_TIME_SUCCESS)
            {
                hDriver->resetState = DRV_USB_UHP_HOST_RESET_STATE_START_DELAYED;
                break;
            }
            else
            {
                SYS_DEBUG_MESSAGE(SYS_ERROR_INFO,"\r\nDRV USB_UHP: Handle error ");
            }
            break;

        case DRV_USB_UHP_HOST_RESET_STATE_START_DELAYED:
            if (SYS_TIME_DelayIsComplete(hDriver->timerHandle) == true)
            {
                hDriver->deviceSpeed = USB_SPEED_ERROR;
                /* Now that reset is complete, we can find out the speed of the attached device. */
                /* This field is valid only when the port enable bit is zero and the current connect status bit is set to a one. */
                if ((*((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + hDriver->portNumber) & UHPHS_PORTSC_PED_Msk) == 0 )
                {
                    if ((*((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + hDriver->portNumber) & UHPHS_PORTSC_CCS_Msk) == UHPHS_PORTSC_CCS_Msk )
                    {
                        /* Line Status */
                        if ((*((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + hDriver->portNumber) & UHPHS_PORTSC_LS_Msk) == (0x01<<UHPHS_PORTSC_LS_Pos))
                        {
                            SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\n\rDRV USB_UHP: LS device connected");
                            hDriver->deviceSpeed = USB_SPEED_LOW;
                            gDrvUSBUHPHostInterface.hostIRPSubmit = DRV_USB_UHP_HOST_IRPSubmitOhci;
                            gDrvUSBUHPHostInterface.rootHubInterface.rootHubOperationEnable = DRV_USB_UHP_HOST_ROOT_HUB_OperationEnableOhci;

                            /* Disable all EHCI interrupts */
                            usbIDEHCI->UHPHS_USBINTR = 0;

                            /* PortOwner: Software writes a one to this bit when the attached device is not a high-speed device. A one in
                             * this bit means that a companion host controller owns and controls the port. */
                            *((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + hDriver->portNumber) |= UHPHS_PORTSC_PO_Msk;  /* Port Owner bit to 1 */

                            /* Route all ports to OHCI in config flag
                             * Port routing control logic default-routes each port to an implementation-dependent classic host controller (default value). */
                            usbIDEHCI->UHPHS_CONFIGFLAG &= ~UHPHS_CONFIGFLAG_CF_Msk;

                            if (SYS_TIME_DelayMS( 5/*50*/ /*ms Low Speed */, &hDriver->timerHandle) == SYS_TIME_SUCCESS)
                            {
                                /* HostControllerReset: Issue an OHCI Controller resets */
                                usbIDOHCI->UHP_OHCI_HCCOMMANDSTATUS |= UHP_OHCI_HCCOMMANDSTATUS_HCR_Msk;
                                usbIDOHCI->UHP_OHCI_HCCONTROL = UHP_OHCI_HCCONTROL_HCFS( DRV_USB_UHP_HOST_OHCI_STATE_USBRESET );
                                hDriver->resetState = DRV_USB_UHP_HOST_RESET_STATE_OHCI_RESET_START;
                                break;
                            }
                            else
                            {
                                SYS_DEBUG_MESSAGE(SYS_ERROR_INFO,"\r\nDRV USB_UHP: Handle error ");
                            }
                        }
                    }
                }

                if( hDriver->deviceSpeed == USB_SPEED_ERROR )
                {
                    /* EHCI Set Port Reset: start bus reset sequence */
                    *((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + hDriver->portNumber) |= UHPHS_PORTSC_PR_Msk;
                    SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\n\r DRV USB_UHP:EHCI Chirps begin");

                    /* Delta T5 = TDRST */
                    if (SYS_TIME_DelayMS( DRV_USB_UHP_RESET_DURATION /* Delta T5 */, &hDriver->timerHandle) == SYS_TIME_SUCCESS)
                    {
                        hDriver->resetState = DRV_USB_UHP_HOST_RESET_STATE_WAIT_FOR_COMPLETE;
                    }
                    else
                    {
                        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO,"\r\nDRV USB_UHP: Handle error ");
                    }
                }
            }
            break;

        case DRV_USB_UHP_HOST_RESET_STATE_WAIT_FOR_COMPLETE:
            /* Reset should last 50ms */
            if (SYS_TIME_DelayIsComplete(hDriver->timerHandle) == true)
            {
                if ((*((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + hDriver->portNumber) & UHPHS_PORTSC_PR_Msk) == UHPHS_PORTSC_PR_Msk)
                {
                    /* Clear Port Reset */
                    /* Software writes a zero to this bit to terminate the bus reset sequence. */
                    *((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + hDriver->portNumber) &= ~UHPHS_PORTSC_PR_Msk;
                    /* CHIRPS sequence begin here */
                    hDriver->resetState = DRV_USB_UHP_HOST_RESET_STATE_COMPLETE;
                }
            }
            break;

        case DRV_USB_UHP_HOST_RESET_STATE_COMPLETE:

            /* 4.2.2 Port Routing Control via PortOwner and Disconnect Event */
            if ((*((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + hDriver->portNumber) & UHPHS_PORTSC_PR_Msk) != UHPHS_PORTSC_PR_Msk)
            {
                SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\n\rDRV USB_UHP: EHCI RESET end");

                if ((*((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + hDriver->portNumber) & UHPHS_PORTSC_PED_Msk) == UHPHS_PORTSC_PED_Msk )
                {
                    SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\n\rDRV USB_UHP: HS device connected");
                    gDrvUSBUHPHostInterface.hostIRPSubmit = DRV_USB_UHP_HOST_IRPSubmitEhci;
                    gDrvUSBUHPHostInterface.rootHubInterface.rootHubOperationEnable = DRV_USB_UHP_HOST_ROOT_HUB_OperationEnableEhci;

                    hDriver->deviceSpeed = USB_SPEED_HIGH;
                    hDriver->resetState = DRV_USB_UHP_HOST_RESET_STATE_NO_RESET;
                    /* port reset is complete */
                    hDriver->isResetting = false;
                }
                else
                {
                    SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\n\rDRV USB_UHP: FS device connected");
                    hDriver->deviceSpeed = USB_SPEED_FULL;

                    gDrvUSBUHPHostInterface.hostIRPSubmit = DRV_USB_UHP_HOST_IRPSubmitOhci;
                    gDrvUSBUHPHostInterface.rootHubInterface.rootHubOperationEnable = DRV_USB_UHP_HOST_ROOT_HUB_OperationEnableOhci;

                    /* Disable all EHCI interrupts */
                    usbIDEHCI->UHPHS_USBINTR = 0;

                    /* PortOwner: Software writes a one to this bit when the attached device is not a high-speed device. A one in
                     * this bit means that a companion host controller owns and controls the port. */
                    *((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + hDriver->portNumber) |= UHPHS_PORTSC_PO_Msk;  /* Port Owner bit to 1 */

                    /* Route all ports to OHCI in config flag
                     * Port routing control logic default-routes each port to an implementation-dependent classic host controller (default value). */
                    usbIDEHCI->UHPHS_CONFIGFLAG &= ~UHPHS_CONFIGFLAG_CF_Msk;
                    
                    hDriver->resetState = DRV_USB_UHP_HOST_RESET_STATE_OHCI_WAIT_FOR_COMPLETE;
                    /* HostControllerReset: Issue an OHCI Controller resets */
                    usbIDOHCI->UHP_OHCI_HCCOMMANDSTATUS |= UHP_OHCI_HCCOMMANDSTATUS_HCR_Msk;
                }
            }
            break;

        case DRV_USB_UHP_HOST_RESET_STATE_OHCI_RESET_START:
            /* Normally, the Host Controller Driver must ensure that the Host Controller stays in USBSUSPEND state for at least 5 ms
             * and then exits this state to either the USBRESUME or the USBRESET state. */
            if (SYS_TIME_DelayIsComplete(hDriver->timerHandle) == true)
            {
                usbIDOHCI->UHP_OHCI_HCCONTROL = UHP_OHCI_HCCONTROL_HCFS( DRV_USB_UHP_HOST_OHCI_STATE_USBRESET );

                /* This means the device attached at low speed */
                hDriver->resetState = DRV_USB_UHP_HOST_RESET_STATE_OHCI_WAIT_FOR_COMPLETE;
            }
            break;

        case DRV_USB_UHP_HOST_RESET_STATE_OHCI_WAIT_FOR_COMPLETE:
            /* Check if the reset has completed */
            if ((usbIDOHCI->UHP_OHCI_HCCOMMANDSTATUS&UHP_OHCI_HCCOMMANDSTATUS_HCR_Msk) != UHP_OHCI_HCCOMMANDSTATUS_HCR_Msk)
            {
                _DRV_USB_UHP_HOST_OhciInit(hDriver);
                /* port reset is complete */
                hDriver->isResetting = false;
                hDriver->resetState = DRV_USB_UHP_HOST_RESET_STATE_NO_RESET;
            }
            break;

        default:
            break;
    }
}

// *****************************************************************************
/* Function:
    void _DRV_USB_UHP_HOST_Initialize
    (
        DRV_USB_UHP_OBJ * const pUSBDrvObj,
        const SYS_MODULE_INDEX index
    )

  Summary:
    This function initializes the driver for host mode operation.

  Description:
    Function performs the following tasks:
      - Informs the USB module with SOF threshold in bit times
      - Enables VBUS power and initializes the module in HOST mode
      - Resets the BDT table data structure with init value
      - Configures EP0 register for the specific USB module
      - Enables the USB attach interrupt

  Remarks:
    This is a local function and should not be called directly by the
    application.
*/
void _DRV_USB_UHP_HOST_Initialize
(
    DRV_USB_UHP_OBJ *drvObj,
    SYS_MODULE_INDEX  index
)
{
    /* No device attached */
    drvObj->deviceAttached = false;
    /* Initialize the device handle */
    drvObj->attachedDeviceObjHandle = USB_HOST_DEVICE_OBJ_HANDLE_INVALID;

    drvObj->intXfrQtdComplete = 0;
    drvObj->staticDToggleIn = 0;
    drvObj->staticDToggleOut = 0;
    drvObj->portNumber = 0xFF;

    _DRV_USB_UHP_HOST_EhciInit(drvObj);

    /* Initialize the host specific members in the driver object */
    drvObj->isResetting = false;
    drvObj->usbHostDeviceInfo = USB_HOST_DEVICE_OBJ_HANDLE_INVALID;
    drvObj->operationEnabled = false;
}/* end of _DRV_USB_UHP_HOST_Initialize() */

// ****************************************************************************
/* Function:
    void DRV_USB_UHP_HOST_EndpointToggleClear

    (
        DRV_HANDLE client,
        USB_ENDPOINT endpointAndDirection
    )

  Summary:
    Facilitates in resetting of endpoint data toggle to 0 for Non Control
    endpoints.

  Description:
    Facilitates in resetting of endpoint data toggle to 0 for Non Control
    endpoints.

  Remarks:
    Refer to drv_usb_uhp_host.h for usage information.
*/
void DRV_USB_UHP_HOST_EndpointToggleClear
(
    DRV_HANDLE client,
    USB_ENDPOINT endpointAndDirection
)
{
    /* Start of local variables */
    DRV_USB_UHP_OBJ * hDriver = NULL;
    uint8_t epIter = 0;
    
    /* End of local variables */
    if((client == DRV_HANDLE_INVALID) || (((DRV_USB_UHP_OBJ *)client) == NULL))
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Invalid client handle");
    }
    else
    {
        hDriver = (DRV_USB_UHP_OBJ *)client;
            
        /* Now map the device endpoint to host endpoint. This is required to
         * jump to the appropriate entry in the endpoint table */
        for(epIter = 1; epIter < DRV_USB_UHP_HOST_MAXIMUM_ENDPOINTS_NUMBER; epIter++)
        {
            if(true == hDriver->hostEndpointTable[epIter].endpoint.inUse)
            {
                /* Please not that for a single non control endpoint there cannot
                 * be multiple pipes. Hence there should be only 1 pipe object
                 * that can be linked to this "endpointAndDirection". */
                if((hDriver->hostEndpointTable[epIter].endpoint.pipe)->endpointAndDirection
                        == endpointAndDirection)
                {
                    /* Got the entry in the host endpoint table. We can exit
                     * from this loop now for further processing */
                    if( (hDriver->hostEndpointTable[epIter].endpoint.pipe->endpointAndDirection & 0x80) == 0 )
                    {
                        /* Host to Device: OUT */
                        hDriver->staticDToggleOut = 0;
                    }
                    else
                    {
                        /* IN */
                        hDriver->staticDToggleIn = 0;
                    }
                }
            }
        }
    }
} /* end of DRV_USB_UHP_HOST_EndpointToggleClear() */

/* Function:
    void _DRV_USB_UHP_HOST_AttachDetachStateMachine(DRV_USB_UHP_OBJ *hDriver)

   Summary:
    Initiate the USB reset on the current port number

   Description:
    Initiate the USB reset on the current port number

   Remarks:
    Refer to .h for usage information.
 */
void _DRV_USB_UHP_HOST_AttachDetachStateMachine(DRV_USB_UHP_OBJ *hDriver)
{
    /* In the host mode, we perform attach de-bouncing */
    bool interruptWasEnabled;

    switch (hDriver->attachState)
    {
        case DRV_USB_UHP_HOST_ATTACH_STATE_CHECK_FOR_DEVICE_ATTACH:

            /* If no device is attached, then there is nothing to do
             * If device is attached, then move to next state */
            if (hDriver->deviceAttached)
            {
                hDriver->attachState = DRV_USB_UHP_HOST_ATTACH_STATE_DETECTED_DEBOUNCE;
            }
            break;

        case DRV_USB_UHP_HOST_ATTACH_STATE_DETECTED_DEBOUNCE:

            /* Delta T3: TATTDB > 100ms */
            if (SYS_TIME_DelayMS( 100  /* ms Delta T3 */, &hDriver->timerHandle) == SYS_TIME_SUCCESS)
            {
                hDriver->attachState = DRV_USB_UHP_HOST_ATTACH_STATE_DETECTED;
            }
            else
            {
                SYS_DEBUG_MESSAGE(SYS_ERROR_INFO,"\r\nDRV USB_UHP: Handle error ");
            }
            break;

    case DRV_USB_UHP_HOST_ATTACH_STATE_DETECTED:

            if (SYS_TIME_DelayIsComplete(hDriver->timerHandle) == true)
            {
                /* Disable the driver interrupt as
                 * we do not want this section to be interrupted. */
                interruptWasEnabled = _DRV_USB_UHP_InterruptSourceDisable(hDriver->interruptSource);

                if (hDriver->deviceAttached)
                {
                    /* Yes the device is still attached. Enumerate
                     * this device. usbHostDeviceInfo is the ID of
                     * this root hub. */
                    hDriver->attachedDeviceObjHandle = USB_HOST_DeviceEnumerate(hDriver->usbHostDeviceInfo, 0);
                    hDriver->attachState = DRV_USB_UHP_HOST_ATTACH_STATE_READY;
                }
                else
                {
                    /* The device is not attached any more. This was a false attach
                     */
                    hDriver->attachState = DRV_USB_UHP_HOST_ATTACH_STATE_CHECK_FOR_DEVICE_ATTACH;
                }

                if (interruptWasEnabled)
                {
                    /* Re-enable the interrupt if it was originally enabled. */
                    _DRV_USB_UHP_InterruptSourceEnable(hDriver->interruptSource);
                }
            }
            break;

        case DRV_USB_UHP_HOST_ATTACH_STATE_READY:

            /* De-bouncing is done and device ready. We can check
             * here if the device is detached */
            if (!hDriver->deviceAttached)
            {
                /* Device is not attached */
                hDriver->attachState = DRV_USB_UHP_HOST_ATTACH_STATE_CHECK_FOR_DEVICE_ATTACH;
            }
            break;

        default:
            break;
    }
}

// *****************************************************************************
/* Function:
    void DRV_USB_UHP_HOST_IRPCancel(USB_HOST_IRP * pinputIRP)

  Summary:
    Cancels the specified IRP.
    
  Description:
    This function attempts to cancel the specified IRP. If the IRP is queued and
    its processing has not started, it will be cancelled successfully. If the
    IRP in progress, the ongoing transaction will be allowed to complete. 

  Remarks:
    See .h for usage information.
*/
void DRV_USB_UHP_HOST_IRPCancel(USB_HOST_IRP *inputIRP)
{
    /* This function cancels an IRP */

    USB_HOST_IRP_LOCAL          *irp = (USB_HOST_IRP_LOCAL *)inputIRP;
    DRV_USB_UHP_OBJ           *hDriver;
    DRV_USB_UHP_HOST_PIPE_OBJ *pipe;
    bool interruptWasEnabled = false;

    if (irp->pipe == DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID)
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Invalid pipe");
        return;
    }

    if (irp->status <= USB_HOST_IRP_STATUS_COMPLETED_SHORT)
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: IRP is not pending or in progress");
        return;
    }

    pipe    = (DRV_USB_UHP_HOST_PIPE_OBJ *)irp->pipe;
    hDriver = (DRV_USB_UHP_OBJ *)pipe->hClient;

    if (!hDriver->isInInterruptContext)
    {
        /* OSAL: Get Mutex */
        if (OSAL_MUTEX_Lock(&(hDriver->mutexID), OSAL_WAIT_FOREVER) != OSAL_RESULT_TRUE)
        {
            SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Mutex lock failed");
        }
        interruptWasEnabled = _DRV_USB_UHP_InterruptSourceDisable(hDriver->interruptSource);
    }

    if (irp->previous == NULL)
    {
        /* This means this was the first
         * irp in the queue. Update the pipe
         * queue head directly */

        pipe->irpQueueHead = irp->next;
        if (irp->next != NULL)
        {
            irp->next->previous = NULL;
        }
    }
    else
    {
        /* Remove the IRP from the linked
         * list */
        irp->previous->next = irp->next;

        if (irp->next != NULL)
        {
            /* This applies if this is not the last
             * irp in the linked list */
            irp->next->previous = irp->previous;
        }
    }

    if (irp->status == USB_HOST_IRP_STATUS_IN_PROGRESS)
    {
        /* If the irp is already in progress then
         * we set the temporary state. This will get
         * caught in _DRV_USB_UHP_HOST_ControlXferProcess()
         * and _DRV_USB_UHP_HOST_NonControlIRPProcess()
         * functions. */

        irp->tempState = DRV_USB_UHP_HOST_IRP_STATE_ABORTED;
    }
    else
    {
        irp->status = USB_HOST_IRP_STATUS_ABORTED;
        if (irp->callback != NULL)
        {
            irp->callback((USB_HOST_IRP *)irp);
        }
    }

    if (!hDriver->isInInterruptContext)
    {
        if (interruptWasEnabled)
        {
            _DRV_USB_UHP_InterruptSourceEnable(hDriver->interruptSource);
        }
        /* OSAL: Release Mutex */
        if (OSAL_MUTEX_Unlock(&hDriver->mutexID) != OSAL_RESULT_TRUE)
        {
            SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Mutex unlock failed");
        }
    }
}/* end of DRV_USB_UHP_IRPCancel() */

// *****************************************************************************
/* Function:
     void DRV_USB_UHP_HOST_PipeClose(DRV_USB_UHP_HOST_PIPE_HANDLE pipeHandle)

  Summary:
    Closes an open pipe.
	
  Description:
    This function closes an open pipe. Any IRPs scheduled on the pipe will be
    aborted and IRP callback functions will be called with the status as
    DRV_USB_HOST_IRP_STATE_ABORTED. The pipe handle will become invalid and the
    pipe will not accept IRPs.

  Remarks:
    See .h for usage information.
*/
void DRV_USB_UHP_HOST_PipeClose
(
    DRV_USB_UHP_HOST_PIPE_HANDLE pipeHandle
)
{
    /* This function closes an open pipe */
    bool interruptWasEnabled = false;

    DRV_USB_UHP_OBJ           *hDriver;
    USB_HOST_IRP_LOCAL          *irp;
    DRV_USB_UHP_HOST_PIPE_OBJ *pipe;
    DRV_USB_UHP_HOST_ENDPOINT_OBJ *endpointObj;

    /* Make sure we have a valid pipe */
    if ((pipeHandle == 0) || (pipeHandle == DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID))
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Invalid pipe handle");
        return;
    }

    pipe = (DRV_USB_UHP_HOST_PIPE_OBJ *)pipeHandle;

    /* Make sure tha we are working with a pipe in use */
    if (pipe->inUse != true)
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Pipe is not in use");
        return;
    }

    hDriver = (DRV_USB_UHP_OBJ *)pipe->hClient;

    /* Disable the interrupt */
    if (!hDriver->isInInterruptContext)
    {
        /* OSAL: Get Mutex */
        if (OSAL_MUTEX_Lock(&hDriver->mutexID, OSAL_WAIT_FOREVER) !=
            OSAL_RESULT_TRUE)
        {
            SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Mutex lock failed");
        }
        interruptWasEnabled = _DRV_USB_UHP_InterruptSourceDisable(hDriver->interruptSource);
    }

    /* Non control transfer pipes are not stored as groups.  We deallocate
     * the endpoint object that this pipe used */

    endpointObj = &hDriver->hostEndpointTable[pipe->hostEndpoint];
    endpointObj->endpoint.inUse = false;
    endpointObj->endpoint.pipe  = NULL;

    /* Now we invoke the call back for each IRP in this pipe and say that it is
     * aborted.  If the IRP is in progress, then that IRP will be actually
     * aborted on the next SOF This will allow the USB module to complete any
     * transaction that was in progress. */

    irp = (USB_HOST_IRP_LOCAL *)pipe->irpQueueHead;
    while (irp != NULL)
    {
        irp->pipe = DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID;

        if (irp->status == USB_HOST_IRP_STATUS_IN_PROGRESS)
        {
            /* If the IRP is in progress, then we set the temp IRP state. This
             * will be caught in the DRV_USB_UHP_HOST_NonControlTransferProcess() and
             * _DRV_USB_UHP_HOST_ControlXferProcess() functions */
            irp->status = USB_HOST_IRP_STATUS_ABORTED;
            if (irp->callback != NULL)
            {
                irp->callback((USB_HOST_IRP *)irp);
            }
            irp->tempState = DRV_USB_UHP_HOST_IRP_STATE_ABORTED;
        }
        else
        {
            /* IRP is pending */
            irp->status = USB_HOST_IRP_STATUS_ABORTED;
            if (irp->callback != NULL)
            {
                irp->callback((USB_HOST_IRP *)irp);
            }
        }
        irp = irp->next;
    }

    /* Now we return the pipe back to the driver */
    pipe->inUse = false;

    /* Enable the interrupts */
    if (!hDriver->isInInterruptContext)
    {
        if (interruptWasEnabled)
        {
            _DRV_USB_UHP_InterruptSourceEnable(hDriver->interruptSource);
        }
        /* OSAL: Return Mutex */
        if (OSAL_MUTEX_Unlock(&hDriver->mutexID) != OSAL_RESULT_TRUE)
        {
            SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Mutex unlock failed");
        }
    }
}/* end of DRV_USB_UHP_HOST_PipeClose() */


// *****************************************************************************
/* Function:
    DRV_USB_UHP_HOST_PIPE_HANDLE DRV_USB_UHP_HOST_PipeSetup
    (
        DRV_HANDLE handle,
        uint8_t deviceAddress,
        USB_ENDPOINT endpointAndDirection,
        uint8_t hubAddress,
        uint8_t hubPort,
        USB_TRANSFER_TYPE pipeType,
        uint8_t bInterval,
        uint16_t wMaxPacketSize,
        USB_SPEED speed
    )

  Summary:
    Open a pipe with the specified attributes.
	
  Description:
    This function opens a communication pipe between the Host and the device
    endpoint. The transfer type and other attributes are specified through the
    function parameters. The driver does not check for available bus bandwidth,
    which should be done by the application (the USB Host Layer in this case)

  Remarks:
    See drv_xxx.h for usage information.
*/
DRV_USB_UHP_HOST_PIPE_HANDLE DRV_USB_UHP_HOST_PipeSetup
(
    DRV_HANDLE        client,
    uint8_t           deviceAddress,
    USB_ENDPOINT      endpointAndDirection,
    uint8_t           hubAddress,
    uint8_t           hubPort,
    USB_TRANSFER_TYPE pipeType,
    uint8_t           bInterval,
    uint16_t          wMaxPacketSize,
    USB_SPEED         speed
)
{
    int pipeIter;
    DRV_USB_UHP_OBJ           *hDriver;
    DRV_USB_UHP_HOST_PIPE_OBJ *pipe;
    bool epFound;

    if (client == DRV_HANDLE_INVALID)
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Invalid client handle");
        return DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID;
    }

    if ((speed == USB_SPEED_LOW) || (speed == USB_SPEED_FULL) || (speed == USB_SPEED_HIGH))
    {
        if (pipeType != USB_TRANSFER_TYPE_CONTROL)
        {
            if (wMaxPacketSize < 8)
            {
                wMaxPacketSize = 8;
            }
        }
    }
    else
    {
        return DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID;
    }

    if ((wMaxPacketSize < 8) || (wMaxPacketSize > 4096))
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Invalid pipe endpoint size");
        return DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID;
    }

    hDriver = (DRV_USB_UHP_OBJ *)client;

    /* OSAL: Mutex Lock */
    if (OSAL_MUTEX_Lock(&hDriver->mutexID, OSAL_WAIT_FOREVER) !=
        OSAL_RESULT_TRUE)
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Mutex lock failed");
        /* return USB_ERROR_OSAL_FUNCTION; */
        return DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID;
    }

    if (pipeType == USB_TRANSFER_TYPE_CONTROL)
    {
        /* Set pipeIter to zero to indicate that we must use pipe 0
         * for control transfers. We also add the control transfer pipe
         * to the control transfer group. */
        pipeIter = 0;

        if (wMaxPacketSize < 8)
        {
            return DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID;
        }

        /* Enable Pipe */
        epFound = true;
        pipe    = &gDrvUSBHostPipeObj[pipeIter];
    }
    else
    {
        /* Pipe allocation for non-control transfer */
        for (pipeIter = 1; pipeIter < DRV_USB_UHP_PIPES_NUMBER; pipeIter++)
        {
            if (false == gDrvUSBHostPipeObj[pipeIter].inUse)
            {
                /* This means we have found a free pipe object */
                epFound = true;
                pipe    = &gDrvUSBHostPipeObj[pipeIter];
                hDriver->hostEndpointTable[pipeIter].endpoint.inUse = true;
                hDriver->hostEndpointTable[pipeIter].endpoint.pipe  = pipe;
                break;
            }
        }
    }

    if (!epFound)
    {
        /* This means we could not find a spare endpoint for this
         * non control transfer. */

        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Could not allocate endpoint");
        /* OSAL: Mutex Unlock */
        if (OSAL_MUTEX_Unlock(&hDriver->mutexID) != OSAL_RESULT_TRUE)
        {
            SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Mutex unlock failed");
        }
        return DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID;
    }

    /* Setup the pipe object */
    pipe->inUse                = true;
    pipe->deviceAddress        = deviceAddress;
    pipe->irpQueueHead         = NULL;
    pipe->bInterval            = bInterval;
    pipe->speed                = speed;
    pipe->hubAddress           = hubAddress;
    pipe->hubPort              = hubPort;
    pipe->pipeType             = pipeType;
    pipe->hClient              = client;
    pipe->endpointSize         = wMaxPacketSize;
    pipe->intervalCounter      = bInterval;
    pipe->hostEndpoint         = pipeIter;
    pipe->endpointAndDirection = endpointAndDirection;

    /* OSAL: Release Mutex */
    if (OSAL_MUTEX_Unlock(&hDriver->mutexID) != OSAL_RESULT_TRUE)
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP Mutex unlock failed");
    }
    return((DRV_USB_UHP_HOST_PIPE_HANDLE)pipe);
}/* end of DRV_USB_UHP_HOST_PipeSetup() */


// *****************************************************************************
/* Function:
    void DRV_USB_UHP_HOST_StartOfFrameControl(DRV_HANDLE client, bool control)

  Summary:
    SOF
	
  Description:
    Management of SOF: nothing to do

  Remarks:
    See drv_xxx.h for usage information.
*/
void DRV_USB_UHP_HOST_StartOfFrameControl(DRV_HANDLE client, bool control)
{
    /* At the point this function does not do any thing.
     * The Start of frame signaling in this HCD is controlled
     * automatically by the module. */
}/* end of DRV_USB_UHP_HOST_StartOfFrameControl() */

// *****************************************************************************
/* Function:
    USB_SPEED DRV_USB_UHP_HOST_DeviceCurrentSpeedGet(DRV_HANDLE client)

  Summary:
    Current speed
	
  Description:
    Get current usb speed of the connected device

  Remarks:
    See drv_xxx.h for usage information.
*/
USB_SPEED DRV_USB_UHP_HOST_DeviceCurrentSpeedGet(DRV_HANDLE client)
{
    /* This function returns the current device speed */
    DRV_USB_UHP_OBJ *hDriver;

    if ((client == DRV_HANDLE_INVALID) || (((DRV_USB_UHP_OBJ *)client) == NULL))
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Invalid client");
    }

    hDriver = (DRV_USB_UHP_OBJ *)client;
    return(hDriver->deviceSpeed);
}/* end of DRV_USB_UHP_HOST_DeviceCurrentSpeedGet() */

/* **************************************************************************** */
/* Function:
    bool DRV_USB_UHP_HOST_EventDisable
    (
        DRV_HANDLE handle
    );

   Summary:
    Disables host mode events.

   Description:
    This function disables the host mode events. This function is called by the
    Host Layer when it wants to execute code atomically.

   Remarks:
    Refer to .h for usage information.
 */
bool DRV_USB_UHP_HOST_EventsDisable
(
    DRV_HANDLE handle
)
{
    DRV_USB_UHP_OBJ *pUSBDrvObj;
    bool              result = false;

    if ((DRV_HANDLE_INVALID != handle) && (0 != handle))
    {
        pUSBDrvObj = (DRV_USB_UHP_OBJ *)(handle);
        result     = _DRV_USB_UHP_InterruptSourceDisable(pUSBDrvObj->interruptSource);
    }
    return(result);
}

/* **************************************************************************** */
/* Function:
    void DRV_USB_UHP_HOST_EventsEnable
    (
        DRV_HANDLE handle
        bool eventRestoreContext
    );

   Summary:
    Restores the events to the specified the original value.

   Description:
    This function will restore the enable disable state of the events.
    eventRestoreContext should be equal to the value returned by the
    DRV_USB_UHP_HOST_EventsDisable() function.

   Remarks:
    Refer to .h for usage information.
 */
void DRV_USB_UHP_HOST_EventsEnable
(
    DRV_HANDLE handle,
    bool       eventContext
)
{
    DRV_USB_UHP_OBJ *pUSBDrvObj;

    if ((DRV_HANDLE_INVALID != handle) && (0 != handle))
    {
        pUSBDrvObj = (DRV_USB_UHP_OBJ *)(handle);
        if (false == eventContext)
        {
            _DRV_USB_UHP_InterruptSourceDisable(pUSBDrvObj->interruptSource);
        }
        else
        {
            _DRV_USB_UHP_InterruptSourceEnable(pUSBDrvObj->interruptSource);
        }
    }
}


/* **************************************************************************** */
/* Function:
    bool DRV_USB_UHP_HOST_ROOT_HUB_OperationIsEnabled(DRV_HANDLE hClient)

   Summary:
    Root hub enable

   Description:
    return true if the HUB is operational and enabled.

   Remarks:
    Refer to .h for usage information.
 */
bool DRV_USB_UHP_HOST_ROOT_HUB_OperationIsEnabled(DRV_HANDLE hClient)
{
    DRV_USB_UHP_OBJ *hDriver;

    if ((hClient == DRV_HANDLE_INVALID) || (((DRV_USB_UHP_OBJ *)hClient) == NULL))
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Invalid client");
    }

    hDriver = (DRV_USB_UHP_OBJ *)hClient;
    return(hDriver->operationEnabled);
}/* end of DRV_USB_UHP_ROOT_HUB_OperationIsEnabled() */

/* **************************************************************************** */

/* Function:
    void DRV_USB_UHP_HOST_ROOT_HUB_Initialize
    (
        DRV_HANDLE handle,
        USB_HOST_DEVICE_OBJ_HANDLE usbHostDeviceInfo,
    )

   Summary:
    This function instantiates the root hub driver.

   Description:
    This function initializes the root hub driver. It is called by the host
    layer at the time of processing root hub devices. The host layer assigns a
    USB_HOST_DEVICE_OBJ_HANDLE reference to this root hub driver. This
    identifies the relationship between the root hub and the host layer.

   Remarks:
    None.
 */
void DRV_USB_UHP_HOST_ROOT_HUB_Initialize
(
    DRV_HANDLE                 handle,
    USB_HOST_DEVICE_OBJ_HANDLE usbHostDeviceInfo
)
{
    DRV_USB_UHP_OBJ *pUSBDrvObj = (DRV_USB_UHP_OBJ *)handle;

    if (DRV_HANDLE_INVALID == handle)
    {
        /* Driver handle is not valid */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
    }
    else if (!(pUSBDrvObj->isOpened))
    {
        /* Driver has not been opened. Handle could be stale */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
    }
    else
    {
        pUSBDrvObj->usbHostDeviceInfo = usbHostDeviceInfo;
    }
}

/* **************************************************************************** */

/* Function:
    uint8_t DRV_USB_UHP_ROOT_HUB_PortNumbersGet(DRV_HANDLE handle);

   Summary:
    Returns the number of ports this root hub contains.

   Description:
    This function returns the number of ports that this root hub contains.

   Remarks:
    None.
 */
uint8_t DRV_USB_UHP_HOST_ROOT_HUB_PortNumbersGet(DRV_HANDLE handle)
{
    DRV_USB_UHP_OBJ *pUSBDrvObj = (DRV_USB_UHP_OBJ *)handle;
    uint8_t           result      = 0;

    if (DRV_HANDLE_INVALID == handle)
    {
        /* Driver handle is not valid */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
    }
    else if (!(pUSBDrvObj->isOpened))
    {
        /* Driver has not been opened. Handle could be stale */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
    }
    else
    {
        result = 1;
    }

    return(result);
}

/* **************************************************************************** */

/* Function:
    uint32_t DRV_USB_UHP_ROOT_HUB_MaximumCurrentGet(DRV_HANDLE);

   Summary:
    Returns the maximum amount of current that this root hub can provide on the
    bus.

   Description:
    This function returns the maximum amount of current that this root hubn can
    provide on the bus.

   Remarks:
    Refer to .h for usage information.
 */
uint32_t DRV_USB_UHP_HOST_ROOT_HUB_MaximumCurrentGet(DRV_HANDLE handle)
{
    DRV_USB_UHP_OBJ *pUSBDrvObj = (DRV_USB_UHP_OBJ *)handle;
    uint32_t          result      = 0;

    if (DRV_HANDLE_INVALID == handle)
    {
        /* Driver handle is not valid */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
    }
    else if (!(pUSBDrvObj->isOpened))
    {
        /* Driver has not been opened. Handle could be stale */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
    }
    else
    {
        result = pUSBDrvObj->rootHubInfo.rootHubAvailableCurrent;
    }

    return(result);
}

/* **************************************************************************** */

/* Function:
    USB_SPEED DRV_USB_UHP_ROOT_HUB_BusSpeedGet(DRV_HANDLE handle);

   Summary:
    Returns the speed at which the bus to which this root hub is connected is
    operating.

   Description:
    This function returns the speed at which the bus to which this root hub is
    connected is operating.

   Remarks:
    None.
 */
USB_SPEED DRV_USB_UHP_HOST_ROOT_HUB_BusSpeedGet(DRV_HANDLE handle)
{
    DRV_USB_UHP_OBJ *pUSBDrvObj = (DRV_USB_UHP_OBJ *)handle;
    USB_SPEED         speed       = USB_SPEED_ERROR;
    DRV_USB_UHP_OBJ *hDriver;

    if (DRV_HANDLE_INVALID == handle)
    {
        /* Driver handle is not valid */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
    }
    else if (!(pUSBDrvObj->isOpened))
    {
        /* Driver has not been opened. Handle could be stale */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
    }
    else
    {
        /* Return the bus speed. This is speed at which the root hub is
         * operating. */
        hDriver = (DRV_USB_UHP_OBJ *)handle;
        speed = hDriver->deviceSpeed;
    }

    return(speed);
}

/* **************************************************************************** */

/* Function:
    void DRV_USB_UHP_ROOT_HUB_PortResume(DRV_HANDLE handle, uint8_t port );

   Summary:
    Resumes the specified root hub port.

   Description:
    This function resumes the root hub. The resume duration is defined by
    DRV_USB_UHP_ROOT_HUB_RESUME_DURATION. The status of the resume signalling can
    be checked using the DRV_USB_UHP_ROOT_HUB_PortResumeIsComplete() function.

   Remarks:
    The root hub on this particular hardware only contains one port - port 0.
 */
USB_ERROR DRV_USB_UHP_HOST_ROOT_HUB_PortResume(DRV_HANDLE handle, uint8_t port)
{
    /* The functionality is yet to be implemented. */
    return(USB_ERROR_NONE);
}

/* **************************************************************************** */

/* Function:
    void DRV_USB_UHP_ROOT_HUB_PortSuspend(DRV_HANDLE handle, uint8_t port );

   Summary:
    Suspends the specified root hub port.

   Description:
    This function suspends the root hub port.

   Remarks:
    The root hub on this particular hardware only contains one port - port 0.
 */
USB_ERROR DRV_USB_UHP_HOST_ROOT_HUB_PortSuspend(DRV_HANDLE handle, uint8_t port)
{
    /* The functionality is yet to be implemented. */
    return(USB_ERROR_NONE);
}

/* **************************************************************************** */

/* Function:
    void DRV_USB_UHP_ROOT_HUB_PortResetIsComplete
    (
        DRV_HANDLE handle,
        uint8_t port
    );

   Summary:
    Returns true if the root hub has completed the port reset operation.

   Description:
    This function returns true if the port reset operation has completed. It
    should be called after the DRV_USB_HOST_ROOT_HUB_PortReset() function to
    check if the reset operation has completed.

   Remarks:
    Refer to .h for usage information.
 */
bool DRV_USB_UHP_HOST_ROOT_HUB_PortResetIsComplete
(
    DRV_HANDLE handle,
    uint8_t    port
)
{
    DRV_USB_UHP_OBJ *pUSBDrvObj = (DRV_USB_UHP_OBJ *)handle;
    bool              result      = true;

    if (DRV_HANDLE_INVALID == handle)
    {
        /* Driver handle is not valid */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
    }
    else if (!(pUSBDrvObj->isOpened))
    {
        /* Driver has not been opened. Handle could be stale */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
    }
    else
    {
        _DRV_USB_UHP_HOST_ResetStateMachine((DRV_USB_UHP_OBJ *)handle);
        /* Return false if the driver is still resetting*/
        result = (pUSBDrvObj->isResetting) ? false : true;
    }

    return(result);
}

/* **************************************************************************** */

/* Function:
    void DRV_USB_UHP_ROOT_HUB_PortReset(DRV_HANDLE handle, uint8_t port );

   Summary:
    Resets the specified root hub port.

   Description:
    This function resets the root hub port. The reset duration is defined by
    DRV_USB_UHP_ROOT_HUB_RESET_DURATION. The status of the reset signalling can be
    checked using the DRV_USB_UHP_ROOT_HUB_PortResetIsComplete() function.

   Remarks:
    Refer to .h for usage information.
 */
USB_ERROR DRV_USB_UHP_HOST_ROOT_HUB_PortReset(DRV_HANDLE handle, uint8_t port)
{
    DRV_USB_UHP_OBJ *pUSBDrvObj = (DRV_USB_UHP_OBJ *)handle;
    USB_ERROR         result      = USB_ERROR_NONE;

    if (DRV_HANDLE_INVALID == handle)
    {
        /* Driver handle is not valid */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
        result = USB_ERROR_PARAMETER_INVALID;
    }
    else if (!(pUSBDrvObj->isOpened))
    {
        /* Driver has not been opened. Handle could be stale */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
        result = USB_ERROR_PARAMETER_INVALID;
    }
    else if (pUSBDrvObj->isResetting)
    {
        /* This means a reset is already in progress. Lets not do anything. */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Reset already in progress");
    }
    else
    {
        /* Start the reset signal. Set the driver flag to indicate the reset
         * signal is in progress. Start generating the reset signal.
         */      

        pUSBDrvObj->isResetting = true;
        pUSBDrvObj->resetState  = DRV_USB_UHP_HOST_RESET_STATE_START;
    }
    return(result);
}

/* **************************************************************************** */

/* Function:
    USB_SPEED DRV_USB_UHP_HOST_ROOT_HUB_PortSpeedGet
    (
        DRV_HANDLE handle,
        uint8_t port
    );

   Summary:
    Returns the speed of at which the port is operating.

   Description:
    This function returns the speed at which the port is operating.

   Remarks:
    Refer to drv_usb_uhp_ehci.h for usage information.
 */
USB_SPEED DRV_USB_UHP_HOST_ROOT_HUB_PortSpeedGet(DRV_HANDLE handle, uint8_t port)
{
    DRV_USB_UHP_OBJ *pUSBDrvObj = (DRV_USB_UHP_OBJ *)handle;
    USB_SPEED         speed       = USB_SPEED_ERROR;

    if (DRV_HANDLE_INVALID == handle)
    {
        /* Driver handle is not valid */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
    }
    else if (!(pUSBDrvObj->isOpened))
    {
        /* Driver has not been opened. Handle could be stale */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nDRV USB_UHP: Bad Client or client closed");
    }
    else
    {
        /* The driver will not check if a device is connected. It is assumed
         * that the client has issued a port reset before calling this function
         */
        speed = pUSBDrvObj->deviceSpeed;
    }
    return(speed);
}


// *****************************************************************************
/* Function:
    void DRV_USB_UHP_HOST_ControlTransferProcess
    (
        DRV_USB_UHP_OBJ * hDriver
    )

  Summary:
    Control Transfer Process.
	
  Description:
    This function is called every time there is an endpoint 0 interrupt.
    This means that a stage of the current control IRP has been completed.
    This function is called from an interrupt context

  Remarks:
   
*/
static void DRV_USB_UHP_HOST_ControlTransferProcess(DRV_USB_UHP_OBJ *hDriver)
{
    USB_HOST_IRP_LOCAL *irp;
    DRV_USB_UHP_HOST_PIPE_OBJ *pipe, *iterPipe;
    DRV_USB_UHP_HOST_TRANSFER_GROUP *transferGroup;
    bool endIRP = false;
    bool foundIRP = false;
    uint8_t *pResult;
    uint32_t i;

    transferGroup = &hDriver->controlTransferGroup;

    /* First check if the IRP was aborted */
    irp  = transferGroup->currentIRP;
    pipe = transferGroup->currentPipe;

    /* If current IRP is null, or current pipe is null then we have unknown
     * failure. We just quit.  Nothing we can do.*/
    if ((irp == NULL)
     || (pipe == NULL)
     || (pipe == (DRV_USB_UHP_HOST_PIPE_OBJ *)DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID))
    {
        /* This means the pipe was closed. We don't do anything */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\033[31m\n\rEHCI IRQ : USB error interrupt\033[0m");
        return;
    }

    /* We should check the current state of the IRP and then proceed accordingly */
    /* If here means, we have a valid IRP and pipe.  Check the status register.
     * The IRP could have been aborted. This would be known in the temp state.
     */
    if ( hDriver->intXfrQtdComplete == 0xFF )
    {
        endIRP = true;
        irp->status = USB_HOST_IRP_STATUS_ERROR_STALL;
        irp->tempState = DRV_USB_UHP_HOST_IRP_STATE_PROCESSING;
        hDriver->intXfrQtdComplete = 0;
        hDriver->controlTransferGroup.currentIRP = NULL;
    }
    else if (irp->tempState == DRV_USB_UHP_HOST_IRP_STATE_ABORTED)
    {
        /* This means the application has aborted this IRP.*/
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\n\rIRP state aborted");
        endIRP = true;
        irp->status = USB_HOST_IRP_STATUS_ABORTED;
    }
    else if (irp->tempState == DRV_USB_UHP_HOST_IRP_STATE_PROCESSING)
    {
        if (hDriver->intXfrQtdComplete == 1)
        {
            hDriver->intXfrQtdComplete = 0;
            irp->status = USB_HOST_IRP_STATUS_COMPLETED;
            endIRP = true;
            if(hDriver->deviceSpeed == USB_SPEED_HIGH)
            {
                _DRV_USB_UHP_HOST_DisableControlList_EHCI(hDriver);
            }
            else
            {
                _DRV_USB_UHP_HOST_DisableControlList_OHCI(hDriver);
            }
        }
    }

    if (endIRP == true)
    {
        if( (irp->flags&0x80) == 0x80 )
        {
            /* Device to Host: IN */
            pResult = irp->data;
            if (irp->completedBytes != 0)
            {
                /* Check the real bytes received */
                if(hDriver->deviceSpeed == USB_SPEED_HIGH)
                {
                    ehci_received_size( &(irp->size) );
                }
                else
                {
                    ohci_received_size( &(irp->size) );
                }
                for (i = 0; i < irp->size; i++)
                {
                    *(uint8_t *)(pResult + i) = USBBufferAligned[i];
                }
            }
        }
        /* This means we need to end the IRP */
        pipe->irpQueueHead = NULL;

        if (irp->callback != NULL)
        {
            /* Invoke the call back*/
            irp->callback((USB_HOST_IRP *)(uint32_t)irp);
        }

        irp = NULL;

        /* Now we need to check if any more IRPs are in this group are pending.
         * We start searching from the current pipe onwards. If we dont find
         * another pipe with an IRP, we should land back on the current pipe and
         * check if we have a IRP to process */

        iterPipe = transferGroup->currentPipe->next;
        for (i = 0; i < transferGroup->nPipes; i++)
        {
            if (iterPipe == NULL)
            {
                /* We have reached the end of the pipe group. Rewind the pipe
                 * iterator to the start of the pipe group. */
            }

            /* This means that we have a valid pipe.  Now check if there is irp
             * to be processed. */

            if (iterPipe->irpQueueHead != NULL)
            {
                foundIRP = true;
                transferGroup->currentPipe = iterPipe;
                transferGroup->currentIRP  = iterPipe->irpQueueHead;
                break;
            }

            iterPipe = iterPipe->next;
        }

        if (foundIRP)
        {
            /* This means we have found another IRP to process. We must load the
             * endpoint. */
            irp         = transferGroup->currentIRP;
            pipe        = transferGroup->currentPipe;
            irp->status = USB_HOST_IRP_STATUS_IN_PROGRESS;
            irp->tempState = DRV_USB_UHP_HOST_IRP_STATE_PROCESSING;
        }
        else
        {
            /* This means we don't have an IRP. Set the current IRP and current
             * pipe to NULL to indicate that we don't have any active IRP */
            transferGroup->currentPipe = NULL;
            transferGroup->currentIRP  = NULL;
        }
    }
}/* end of DRV_USB_UHP_HOST_ControlTransferProcess() */


// *****************************************************************************
/* Function:
    void DRV_USB_UHP_HOST_NonControlTransferProcess
    (
        DRV_USB_UHP_OBJ * hDriver
        uint8_t hostPipe
   )

  Summary:
    Non Control Transfer Process.
	
  Description:
    This function processes non-zero endpoint transfers which
    could be any of bulk, interrupt and isochronous transfers

  Remarks:
*/
static void DRV_USB_UHP_HOST_NonControlTransferProcess
(
    DRV_USB_UHP_OBJ * hDriver,
    uint8_t endpoint
)
{
    /* This function processes non-zero endpoint transfers which
     * could be any of bulk, interrupt and isochronous transfers */

    DRV_USB_UHP_HOST_ENDPOINT_OBJ * endpointTable;
    USB_HOST_IRP_LOCAL                *irp;
    DRV_USB_UHP_HOST_PIPE_OBJ       *pipe;
    bool     endIRP = false;
    bool endIRPOut = false;

    endpointTable = &(hDriver->hostEndpointTable[endpoint]);
    pipe = endpointTable->endpoint.pipe; 

    if((endpointTable->endpoint.inUse == false)
    || (pipe == NULL)
    || (pipe == (DRV_USB_UHP_HOST_PIPE_OBJ *)DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID)
    || (pipe->irpQueueHead == NULL))
    {
        /* This means the pipe was closed. We don't do anything */
        return;
    }

    irp = pipe->irpQueueHead;
   
    /* We should check the current state of the IRP and then proceed accordingly */
    if ( hDriver->intXfrQtdComplete == 0xFF )
    {
        endIRP = true;
        irp->status = USB_HOST_IRP_STATUS_ERROR_STALL;

        irp->tempState = DRV_USB_UHP_HOST_IRP_STATE_PROCESSING;
        hDriver->intXfrQtdComplete = 0;
        hDriver->controlTransferGroup.currentIRP = NULL;
    }
    else if ( irp->tempState == DRV_USB_UHP_HOST_IRP_STATE_ABORTED )
    {
        /* This means the application has aborted this IRP.*/
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\n\rIRP state aborted");
        endIRP      = true;
        irp->status = USB_HOST_IRP_STATUS_ABORTED;
    }
    else if ( irp->tempState == DRV_USB_UHP_HOST_IRP_STATE_PROCESSING )
    {
        if (hDriver->intXfrQtdComplete == 1)
        {
            hDriver->intXfrQtdComplete = 0;

            if(irp->completedBytes >= irp->size)
            {
                endIRP = true;
                endIRPOut = true;
                irp->status = USB_HOST_IRP_STATUS_COMPLETED;
            }
            else
            {
                /* This means we have more data to send */
                endIRP = false;
            }

            if(hDriver->deviceSpeed == USB_SPEED_HIGH)
            {
                _DRV_USB_UHP_HOST_DisableControlList_EHCI(hDriver);
            }
            else
            {
                _DRV_USB_UHP_HOST_DisableControlList_OHCI(hDriver);
            }
        }
    }

    if (endIRP)
    {
        DCACHE_INVALIDATE_BY_ADDR((uint32_t *)irp->data, irp->size);

        /* This means we need to end the IRP */
        pipe->irpQueueHead = irp->next;
        if (irp->callback != NULL)
        {
            /* Invoke the call back*/
            irp->callback((USB_HOST_IRP *)(uint32_t)irp);
        }

        irp = pipe->irpQueueHead;

        if((irp != NULL) && (!(irp->status == USB_HOST_IRP_STATUS_IN_PROGRESS)) && (endIRPOut != false) )
        {
            /* We do have another IRP to process. */
            irp->status = USB_HOST_IRP_STATUS_IN_PROGRESS;
        }

        /* A IRP could have been submitted in the callback. If that is the
         * case and the IRP status would indicate that it already in
         * progress. If the IRP in the queue head is not in progress then we
         * should initiate the transaction */
        if((irp != NULL) && (!(irp->status == USB_HOST_IRP_STATUS_IN_PROGRESS)) && (endIRPOut == false) )
        {
            irp->status = USB_HOST_IRP_STATUS_IN_PROGRESS;  
        }
    }
}/* end of DRV_USB_UHP_HOST_NonControlTransferProcess() */


// *****************************************************************************
/* Function:
    void DRV_USB_UHP_HOST_TransferProcess(DRV_USB_UHP_OBJ *hDriver)

   Summary:
    Dynamic implementation of USB HOST Transfer Process system interface function.

   Description:
    This is the dynamic implementation of USB HOST Transfer Process system interface
    function.

   Remarks:
    See drv_uhp.h for usage information.
 */
static void DRV_USB_UHP_HOST_TransferProcess(DRV_USB_UHP_OBJ *hDriver)
{
    /* This function is called every time there is an endpoint 0
     * interrupt. This means that a stage of the current control IRP has been
     * completed. This function is called from an interrupt context */
    USB_HOST_IRP_LOCAL                *irp;
    DRV_USB_UHP_HOST_PIPE_OBJ       *pipe;
    DRV_USB_UHP_HOST_TRANSFER_GROUP *transferGroup;

    transferGroup = &hDriver->controlTransferGroup;

    /* First check if the IRP was aborted */
    irp  = transferGroup->currentIRP;
    pipe = transferGroup->currentPipe;

    /* If current IRP is null, or current pipe is null then we have unknown
     * failure. We just quit.  Nothing we can do.*/
    if ((irp == NULL)
     || (pipe == NULL)
     || (pipe == (DRV_USB_UHP_HOST_PIPE_OBJ *)DRV_USB_UHP_HOST_PIPE_HANDLE_INVALID))
    {
        /* This means the pipe was closed. We don't do anything */
    }
    else
    {
        if(hDriver->hostPipeInUse == 0)
        {
            DRV_USB_UHP_HOST_ControlTransferProcess(hDriver);
        }       
        else
        {
            DRV_USB_UHP_HOST_NonControlTransferProcess(hDriver, hDriver->hostPipeInUse);
        }
    }
}


// *****************************************************************************
/* Function:
    void DRV_USB_UHP_Tasks(SYS_MODULE_OBJ object)

   Summary:
    Dynamic implementation of DRV_USB_UHP_Tasks system interface function.

   Description:
    This is the dynamic implementation of DRV_USB_UHP_Tasks system interface
    function.

   Remarks:
    See drv_uhp.h for usage information.
 */
void DRV_USB_UHP_Tasks(SYS_MODULE_OBJ object)
{
    DRV_USB_UHP_OBJ *hDriver;

    hDriver = &gDrvUSBObj[object];

    if (hDriver->status <= SYS_STATUS_UNINITIALIZED)
    {
        /* Driver is not initialized */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\n\rDriver is not initialized");
    }
    else
    {
        /* Check the tasks state and maintain */
        switch (hDriver->state)
        {
            case DRV_USB_UHP_TASK_STATE_WAIT_FOR_CLOCK_USABLE:

                /* Wait for PLLA,UPLL stabilization LOCK bit in PMC_SR     */
                if(IS_LOCKU_ENABLE())
                {
                    /* The operation mode can be initialized */
                    hDriver->state = DRV_USB_UHP_TASK_STATE_INITIALIZE_OPERATION_MODE;
                }
                break;

            case DRV_USB_UHP_TASK_STATE_INITIALIZE_OPERATION_MODE:

                /* Host mode specific driver initialization */
                _DRV_USB_UHP_HOST_INIT(hDriver, object);

                /* Clear and enable the interrupts */
                _DRV_USB_UHP_InterruptSourceClear(hDriver->interruptSource);
                _DRV_USB_UHP_InterruptSourceEnable(hDriver->interruptSource);

                /* Indicate that the object is ready
                * and change the state to running */
                hDriver->status = SYS_STATUS_READY;
                hDriver->state  = DRV_USB_UHP_TASK_STATE_RUNNING;
                break;

            case DRV_USB_UHP_TASK_STATE_RUNNING:

                /* The module is in a running state. In the polling mode the
                 * driver ISR tasks are called here. We also
                 * check for the VBUS level and generate events if a client
                 * event handler is registered. */

                /* Host mode specific polled
                 * task routines can be called here */
                _DRV_USB_UHP_HOST_AttachDetachStateMachine(hDriver);

                /* Polled mode driver tasks routines are really the same as the
                 * the ISR task routines called in the driver task routine */
                _DRV_USB_UHP_Tasks_ISR(object);
                break;

            default:
                break;
        }
    }
}/* end of DRV_USB_UHP_Tasks() */

// *****************************************************************************
/* Function:
    void DRV_USB_UHP_Deinitialize( const SYS_MODULE_OBJ object )

   Summary:
    Dynamic implementation of DRV_USB_UHP_Deinitialize system interface function.

   Description:
    This is the dynamic implementation of DRV_USB_UHP_Deinitialize
    system interface function.

   Remarks:
    See drv_uhp.h for usage information.
 */
void DRV_USB_UHP_Deinitialize
(
    const SYS_MODULE_INDEX object
)
{
    DRV_USB_UHP_OBJ *drvObj;

    if (object == (SYS_MODULE_INDEX)SYS_MODULE_OBJ_INVALID)
    {
        /* Invalid object */
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB_UHP Driver: Invalid object in DRV_USB_UHP_Deinitialize()");
    }
    else
    {
        if (object >= DRV_USB_UHP_INSTANCES_NUMBER)
        {
            SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB_UHP Driver: Invalid object in DRV_USB_UHP_Deinitialize()");
        }
        else
        {
            if (gDrvUSBObj[object].inUse == false)
            {
                /* Cannot de-initialize an object that is not already in use. */
                SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB_UHP Driver: Driver not initialized in DRV_USB_UHP_Deinitialize()");
            }
            else
            {
                drvObj = &gDrvUSBObj[object];

                /* Populate the driver object with the required data */

                drvObj->inUse  = false;
                drvObj->status = SYS_STATUS_UNINITIALIZED;

                /* Clear and disable the interrupts. Assigning to a value has
                 * been implemented to remove compiler warning in polling mode.
                 */

                _DRV_USB_UHP_InterruptSourceDisable(drvObj->interruptSource);
                SYS_INT_SourceStatusClear(drvObj->interruptSource);

                drvObj->isOpened       = false;
                drvObj->pEventCallBack = NULL;

                /* Delete the mutex */
                if (OSAL_MUTEX_Delete(&drvObj->mutexID) != OSAL_RESULT_TRUE)
                {
                    SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB_UHP Driver: Could not delete mutex in DRV_USB_UHP_Deinitialize()");
                }
            }
        }
    }
} /* end of DRV_USB_UHP_Deinitialize() */

// *****************************************************************************
/* Function:
    SYS_STATUS DRV_USB_UHP_Status( const SYS_MODULE_OBJ object )

   Summary:
    Dynamic implementation of DRV_USB_UHP_Status system interface function.

   Description:
    This is the dynamic implementation of DRV_USB_UHP_Status system interface
    function.

   Remarks:
    See drv_uhp.h for usage information.
 */
SYS_STATUS DRV_USB_UHP_Status(SYS_MODULE_OBJ object)
{
    SYS_STATUS retVal = gDrvUSBObj[object].status;

    if (object == SYS_MODULE_OBJ_INVALID)
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB_UHP Driver: System Module Object is invalid in DRV_USB_UHP_Status().");
        retVal = SYS_STATUS_ERROR;
    }

    /* Return the status of the driver object */
    return(retVal);
}/* end of DRV_USB_UHP_Status() */


// *****************************************************************************
/* Function:
    SYS_STATUS DRV_USB_UHP_Open( const SYS_MODULE_INDEX iDriver,
                                   const DRV_IO_INTENT    ioIntent )

   Summary:
    Dynamic implementation of DRV_USB_UHP_Open system interface function.

   Description:
    This is the dynamic implementation of DRV_USB_UHP_Open system interface
    function.

   Remarks:
    See drv_uhp.h for usage information.
 */
DRV_HANDLE DRV_USB_UHP_Open
(
    const SYS_MODULE_INDEX iDriver,
    const DRV_IO_INTENT    ioIntent
)
{
    DRV_USB_UHP_OBJ *drvObj;
    DRV_HANDLE        retVal = DRV_HANDLE_INVALID;

    /* The iDriver value should be valid. It should be less the number of driver
     * object instances.  */

    if (iDriver >= DRV_USB_UHP_INSTANCES_NUMBER)
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB USB_UHP Driver: Bad Driver Index in DRV_USB_USB_UHP_Open().");
    }
    else
    {
        drvObj = &gDrvUSBObj[iDriver];

        if (drvObj->status == SYS_STATUS_READY)
        {
            if (ioIntent != (DRV_IO_INTENT_EXCLUSIVE|DRV_IO_INTENT_NONBLOCKING |DRV_IO_INTENT_READWRITE))
            {
                /* The driver only supports this mode */
                SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB USB_UHP Driver: Unsupported IO Intent in DRV_USB_USB_UHP_Open().");
            }
            else
            {
                if (drvObj->isOpened)
                {
                    /* Driver supports exclusive open only */
                    SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB USB_UHP Driver: Driver can be opened only once. Multiple calls to DRV_USB_USB_UHP_Open().");
                }
                else
                {
                    /* Clear prior value */
                    drvObj->pEventCallBack = NULL;

                    /* Store the handle in the driver object client table and update
                     * the number of clients*/
                    drvObj->isOpened = true;
                    retVal           = ((DRV_HANDLE)drvObj);
                    SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB USB_UHP Driver: Driver opened successfully in DRV_USB_UHP_Open().");
                }
            }
        }
    }
    /* Return the client object */
    return(retVal);
}/* end of DRV_USB_UHP_Open() */

// *****************************************************************************
/* Function:
    void DRV_USB_USB_UHP_Close( DRV_HANDLE client )

   Summary:
    Dynamic implementation of DRV_USB_USB_UHP_Close client interface function.

   Description:
    This is the dynamic implementation of DRV_USB_USB_UHP_Close client interface
    function.

   Remarks:
    See drv_uhp.h for usage information.
 */
void DRV_USB_UHP_Close(DRV_HANDLE client)
{
    DRV_USB_UHP_OBJ *hDriver;

    if (client == DRV_HANDLE_INVALID)
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB USB_UHP Driver: Bad Client Handle in DRV_USB_UHP_Close().");
    }
    else
    {
        hDriver = (DRV_USB_UHP_OBJ *)client;

        if (!(hDriver->isOpened))
        {
            SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB USB_UHP Driver: Invalid client handle in DRV_USB_UHP_Close().");
        }
        else
        {
            /* Give back the client */
            hDriver->isOpened       = false;
            hDriver->pEventCallBack = NULL;
        }
    }
}/* end of DRV_USB_USB_UHP_Close() */

// *****************************************************************************
/* Function:
    void UHPHS_Handler( void )

   Summary:
    Dynamic implementation of UHPHS_Handler client interface function.

   Description:
    This is the dynamic implementation of UHPHS_Handler client interface
    function.

   Remarks:
    See drv_uhp.h for usage information.
 */
void UHPHS_Handler(void)
{
    DRV_USB_UHP_Tasks_ISR(sysObj.drvUSBObject);
}

// *****************************************************************************
/* Function:
    DRV_HANDLE DRV_USB_UHP_Tasks_ISR( SYS_MODULE_OBJ object )

   Summary:
    Dynamic implementation of DRV_USB_UHP_Tasks_ISR system interface function.

   Description:
    This is the dynamic implementation of DRV_USB_UHP_Tasks_ISR system interface
    function.

   Remarks:
    See drv_uhp.h for usage information.
 */
void DRV_USB_UHP_Tasks_ISR(SYS_MODULE_OBJ object)
{
    DRV_USB_UHP_OBJ *hDriver;

    hDriver = &gDrvUSBObj[object];

    hDriver->isInInterruptContext = true;

    _DRV_USB_UHP_HOST_Tasks_ISR_EHCI(hDriver);
    _DRV_USB_UHP_HOST_Tasks_ISR_OHCI(hDriver);

    DRV_USB_UHP_HOST_TransferProcess(hDriver);

    SYS_INT_SourceStatusClear(hDriver->interruptSource);
    hDriver->isInInterruptContext = false;
}/* end of DRV_USB_UHP_Tasks_ISR() */


// *****************************************************************************
/* Function:
    bool DRV_USB_UHP_HOST_Resume(DRV_HANDLE handle)

   Summary:
    Dynamic implementation of DRV_USB_UHP_HOST_Resume
    client interface function.

   Description:
    This is the dynamic implementation of DRV_USB_UHP_HOST_Resume client
    interface function. Function resumes a suspended BUS.

   Remarks:
    See drv_uhp.h for usage information.
 */
bool DRV_USB_UHP_HOST_Resume
(
    DRV_HANDLE handle
)
{
    DRV_USB_UHP_OBJ          *pusbdrvObj = (DRV_USB_UHP_OBJ *)NULL;
    volatile uhphs_registers_t *usbIDEHCI;
    bool retVal = false;

    /* Check if the handle is valid */
    if ((handle == DRV_HANDLE_INVALID))
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB USB_UHP Driver: Bad Client or client closed in DRV_USB_UHP_HOST_Resume().");
    }
    else
    {
        pusbdrvObj = (DRV_USB_UHP_OBJ *)handle;
        usbIDEHCI      = pusbdrvObj->usbIDEHCI;
        /* Function enables resume signaling */
        *((uint32_t *)&(usbIDEHCI->UHPHS_PORTSC) + (pusbdrvObj->hostEndpointTable[0].endpoint.pipe->hubPort))
              |= UHPHS_PORTSC_FPR_Msk;  /* Force Port Resume */
        retVal = true;
    }

    return retVal;
}/* end of DRV_USB_UHP_HOST_Resume() */

// *****************************************************************************

/* Function:
    bool DRV_USB_UHP_HOST_Suspend(DRV_HANDLE handle)

   Summary:
    Dynamic implementation of DRV_USB_UHP_HOST_Suspend
    client interface function.

   Description:
    This is the dynamic implementation of DRV_USB_UHP_HOST_Suspend client
    interface function. Function suspends USB BUS.

   Remarks:
    See drv_uhp.h for usage information.
 */
bool DRV_USB_UHP_HOST_Suspend
(
    DRV_HANDLE handle
)
{
    DRV_USB_UHP_OBJ          *pusbdrvObj = (DRV_USB_UHP_OBJ *)NULL;
    volatile uhphs_registers_t *usbIDEHCI;
    bool retVal = false;

    /* Check if the handle is valid */
    if ((handle == DRV_HANDLE_INVALID))
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB USB_UHP Driver: Bad Client or client closed in DRV_USB_UHP_HOST_Suspend().");
    }
    else
    {
        pusbdrvObj = (DRV_USB_UHP_OBJ *)handle;
        usbIDEHCI      = pusbdrvObj->usbIDEHCI;
        /* Suspend the bus */
        /* Turn USB HOST OFF */
        usbIDEHCI->UHPHS_USBCMD &= ~UHPHS_USBCMD_RS_Msk; /* Stop = 0 */
        while ((usbIDEHCI->UHPHS_USBCMD & UHPHS_USBCMD_RS_Msk) == UHPHS_USBCMD_RS_Msk)
        {
        }
        retVal = true;
    }
    return(retVal);
}/* end of DRV_USB_UHP_HOST_Suspend() */

// *****************************************************************************

/* Function:
    void DRV_USB_USB_UHP_ClientEventCallBackSet
    (
        DRV_HANDLE   handle,
        uintptr_t    hReferenceData,
        DRV_USB_UHP_EVENT_CALLBACK eventCallBack
    )

   Summary:
    Dynamic implementation of DRV_USB_USB_UHP_ClientEventCallBackSet client interface
    function.

   Description:
    This is the dynamic implementation of DRV_USB_USB_UHP_ClientEventCallBackSet
    client interface function.

   Remarks:
    See drv_uhp.h for usage information.
 */
void DRV_USB_UHP_ClientEventCallBackSet
(
    DRV_HANDLE             client,
    uintptr_t              hReferenceData,
    DRV_USB_EVENT_CALLBACK eventCallBack
)
{
    DRV_USB_UHP_OBJ *pusbDrvObj;

    if (client == DRV_HANDLE_INVALID)
    {
        SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB USB_UHP Driver: Bad Client Handle in DRV_USB_UHP_ClientEventCallBackSet().");
    }
    else
    {
        pusbDrvObj = (DRV_USB_UHP_OBJ *)client;

        if (!pusbDrvObj->isOpened)
        {
            SYS_DEBUG_MESSAGE(SYS_ERROR_INFO, "\r\nUSB USB_UHP Driver: Invalid client handle in DRV_USB_UHP_ClientEventCallBackSet().");
        }
        else
        {
            /* Assign event call back and reference data */
            pusbDrvObj->hClientArg     = hReferenceData;
            pusbDrvObj->pEventCallBack = eventCallBack;
        }
    }
}/* end of DRV_USB_USB_UHP_ClientEventCallBackSet() */
