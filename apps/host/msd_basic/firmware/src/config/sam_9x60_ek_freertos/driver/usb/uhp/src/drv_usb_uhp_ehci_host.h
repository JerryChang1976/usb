/*******************************************************************************
  USB Driver EHCI Data Structures

  Company:
    Microchip Technology Inc.

  File Name:
    drv_usb_uhp_ehci_host.h

  Summary:
    USB driver EHCI declarations and definitions

  Description:
    This file contains the USB driver's EHCI declarations and definitions.
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

#ifndef _DRV_USB_UHP_EHCI_H
#define _DRV_USB_UHP_EHCI_H


// *****************************************************************************
// *****************************************************************************
// Section: File includes
// *****************************************************************************
// *****************************************************************************

#ifndef UHP_EHCI_RAM_ADDR
#define UHP_EHCI_RAM_ADDR                            0xA0100000u
#endif

// *****************************************************************************
// *****************************************************************************
// Section: Data Type Definitions
// *****************************************************************************
// *****************************************************************************

extern void _DRV_USB_UHP_HOST_Tasks_ISR_EHCI(DRV_USB_UHP_OBJ *hDriver);

extern void USB_UHP_ResetEnableEhci(DRV_USB_UHP_OBJ *hDriver);
extern void _DRV_USB_UHP_HOST_EhciInit(DRV_USB_UHP_OBJ *drvObj);
extern void _DRV_USB_UHP_HOST_DisableControlList_EHCI(DRV_USB_UHP_OBJ *hDriver);
extern void ehci_received_size( uint32_t * BuffSize );

#endif  // _DRV_USB_UHP_EHCI_H
