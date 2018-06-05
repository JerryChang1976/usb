<#--
/*******************************************************************************
  USB Device Freemarker Template File

  Company:
    Microchip Technology Inc.

  File Name:
    system_init_c_device_data_audio_function_init.ftl

  Summary:
    USB Device Freemarker Template File

  Description:
    This file contains configurations necessary to run the system.  It
    implements the "SYS_Initialize" function, configuration bits, and allocates
    any necessary global system resources, such as the systemObjects structure
    that contains the object handles to all the MPLAB Harmony module objects in
    the system.
*******************************************************************************/

/*******************************************************************************
Copyright (c) 2014 released Microchip Technology Inc.  All rights reserved.

Microchip licenses to you the right to use, modify, copy and distribute
Software only when embedded on a Microchip microcontroller or digital signal
controller that is integrated into your product or third party product
(pursuant to the sublicense terms in the accompanying license agreement).

You should refer to the license agreement accompanying this Software for
additional information regarding your rights and obligations.

SOFTWARE AND DOCUMENTATION ARE PROVIDED AS IS  WITHOUT  WARRANTY  OF  ANY  KIND,
EITHER EXPRESS  OR  IMPLIED,  INCLUDING  WITHOUT  LIMITATION,  ANY  WARRANTY  OF
MERCHANTABILITY, TITLE, NON-INFRINGEMENT AND FITNESS FOR A  PARTICULAR  PURPOSE.
IN NO EVENT SHALL MICROCHIP OR  ITS  LICENSORS  BE  LIABLE  OR  OBLIGATED  UNDER
CONTRACT, NEGLIGENCE, STRICT LIABILITY, CONTRIBUTION,  BREACH  OF  WARRANTY,  OR
OTHER LEGAL  EQUITABLE  THEORY  ANY  DIRECT  OR  INDIRECT  DAMAGES  OR  EXPENSES
INCLUDING BUT NOT LIMITED TO ANY  INCIDENTAL,  SPECIAL,  INDIRECT,  PUNITIVE  OR
CONSEQUENTIAL DAMAGES, LOST  PROFITS  OR  LOST  DATA,  COST  OF  PROCUREMENT  OF
SUBSTITUTE  GOODS,  TECHNOLOGY,  SERVICES,  OR  ANY  CLAIMS  BY  THIRD   PARTIES
(INCLUDING BUT NOT LIMITED TO ANY DEFENSE  THEREOF),  OR  OTHER  SIMILAR  COSTS.
*******************************************************************************/
-->
<#if CONFIG_USB_DEVICE_FUNCTION_AUDIO_VERSION == "Audio v1">
const USB_DEVICE_AUDIO_INIT audioInit${CONFIG_USB_DEVICE_FUNCTION_INDEX} =
{
	.queueSizeRead = ${CONFIG_USB_DEVICE_FUNCTION_READ_Q_SIZE},
	.queueSizeWrite = ${CONFIG_USB_DEVICE_FUNCTION_WRITE_Q_SIZE}
};
<#elseif CONFIG_USB_DEVICE_FUNCTION_AUDIO_VERSION == "Audio v2">
const USB_DEVICE_AUDIO_V2_INIT audioInit${CONFIG_USB_DEVICE_FUNCTION_INDEX} =
{
	.queueSizeRead = ${CONFIG_USB_DEVICE_FUNCTION_READ_Q_SIZE},
	.queueSizeWrite = ${CONFIG_USB_DEVICE_FUNCTION_WRITE_Q_SIZE}
};
</#if>
<#--
/*******************************************************************************
 End of File
*/
-->