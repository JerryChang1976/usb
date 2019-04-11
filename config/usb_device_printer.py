"""*****************************************************************************
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
*****************************************************************************"""
currentQSizeRead  = 1
currentQSizeWrite = 1
printerInterfacesNumber = 1
printerDescriptorSize = 16
printerEndpointsPic32 = 1
printerEndpointsSAM = 2
indexFunction = None
configValue = None 
startInterfaceNumber = None 
numberOfInterfaces = None 
epNumberBulkOut = None
printerEndpointNumber = None


def onAttachmentConnected(source, target):
	global printerInterfacesNumber
	global printerDescriptorSize
	global configValue
	global startInterfaceNumber
	global numberOfInterfaces
	global epNumberBulkOut
	global printerEndpointsPic32
	global printerEndpointsSAM
	
	dependencyID = source["id"]
	ownerComponent = source["component"]
	
	# Read number of functions from USB Device Layer 
	nFunctions = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_FUNCTIONS_NUMBER")

	if nFunctions != None: 
		#Log.writeDebugMessage ("USB Device Printer Function Driver: Attachment connected")
		
		# Update Number of Functions in USB Device, Increment the value by One. 
		Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_FUNCTIONS_NUMBER")
		Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_FUNCTIONS_NUMBER", nFunctions + 1 , 2)
	
		configDescriptorSize = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE")
		if configDescriptorSize != None: 
			Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE")
			Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE", configDescriptorSize + printerDescriptorSize , 2)
	
		# Update Total Interfaces number 
		nInterfaces = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_INTERFACES_NUMBER")
		if nInterfaces != None: 
			Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_INTERFACES_NUMBER")
			Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_INTERFACES_NUMBER", nInterfaces + printerInterfacesNumber , 2)
			startInterfaceNumber.setValue(nInterfaces, 1)
			
	# Update Total Endpoints used 
		nEndpoints = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER")
		if nEndpoints != None:

			epNumberBulkOut.setValue(nEndpoints + 1, 1)
			if any(x in Variables.get("__PROCESSOR") for x in ["PIC32MZ"]):
				Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER")
				Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER", nEndpoints + printerEndpointsPic32 , 2)
			else:
				Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER")
				Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER", nEndpoints + printerEndpointsSAM , 2)
			

def onAttachmentDisconnected(source, target):

	print ("Printer Function Driver: Detached")
	global printerInterfacesNumber
	global printerDescriptorSize
	global configValue
	global startInterfaceNumber
	global numberOfInterfaces
	global epNumberBulkOut
	global printerEndpointsPic32
	global printerEndpointsSAM
	dependencyID = source["id"]
	ownerComponent = source["component"]
	
	nFunctions = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_FUNCTIONS_NUMBER")
	if nFunctions != None: 
		nFunctions = nFunctions - 1
		Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_FUNCTIONS_NUMBER")
		Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_FUNCTIONS_NUMBER", nFunctions , 2)
	
	endpointNumber = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER")
	if endpointNumber != None:
		if any(x in Variables.get("__PROCESSOR") for x in ["PIC32MZ"]):
			Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER")
			Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER", endpointNumber -  printerEndpointsPic32 , 2)
		else:
			Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER")
			Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER", endpointNumber -  printerEndpointsSAM , 2)
	
	# Update Total Interfaces number
	interfaceNumber = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_INTERFACES_NUMBER")
	if interfaceNumber != None: 
		Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_INTERFACES_NUMBER")
		Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_INTERFACES_NUMBER", interfaceNumber - 2, 2)

	# Update Total configuration descriptor size 	
	configDescriptorSize = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE")
	if configDescriptorSize != None: 
		Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE")
		Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE", configDescriptorSize - printerDescriptorSize , 2)
		
	
def destroyComponent(component):
	print ("Printer Function Driver: Destroyed")
			
		
	
def usbDevicePrinterBufferQueueSize(usbSymbolSource, event):
	global currentQSizeRead
	global currentQSizeWrite
	queueDepthCombined = Database.getSymbolValue("usb_device_printer", "CONFIG_USB_DEVICE_PRINTER_QUEUE_DEPTH_COMBINED")
	if (event["id"] == "CONFIG_USB_DEVICE_FUNCTION_READ_Q_SIZE"):
		queueDepthCombined = queueDepthCombined - currentQSizeRead + event["value"]
		currentQSizeRead = event["value"]
	if (event["id"] == "CONFIG_USB_DEVICE_FUNCTION_WRITE_Q_SIZE"):
		queueDepthCombined = queueDepthCombined - currentQSizeWrite  + event["value"]
		currentQSizeWrite = event["value"]
	Database.clearSymbolValue("usb_device_printer", "CONFIG_USB_DEVICE_PRINTER_QUEUE_DEPTH_COMBINED")
	Database.setSymbolValue("usb_device_printer", "CONFIG_USB_DEVICE_PRINTER_QUEUE_DEPTH_COMBINED", queueDepthCombined, 2)
	
def instantiateComponent(usbDevicePrinterComponent, index):
	global printerDescriptorSize
	global printerInterfacesNumber
	global configValue
	global startInterfaceNumber
	global numberOfInterfaces
	global currentQSizeRead
	global currentQSizeWrite
	global epNumberBulkOut
	
	res = Database.activateComponents(["usb_device"])
	
	# Index of this function 
	indexFunction = usbDevicePrinterComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_INDEX", None)
	indexFunction.setVisible(False)
	indexFunction.setMin(0)
	indexFunction.setMax(16)
	indexFunction.setDefaultValue(index)
	indexFunction.setReadOnly(True)

	# Config name: Configuration number
	configValue = usbDevicePrinterComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_CONFIG_VALUE", None)
	configValue.setLabel("Configuration Value")
	configValue.setVisible(False)
	configValue.setMin(1)
	configValue.setMax(16)
	configValue.setDefaultValue(1)
	configValue.setReadOnly(True)

	# Adding Start Interface number 
	startInterfaceNumber = usbDevicePrinterComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_INTERFACE_NUMBER", None)
	startInterfaceNumber.setLabel("Start Interface Number")
	startInterfaceNumber.setVisible(True)
	startInterfaceNumber.setMin(0)
	startInterfaceNumber.setDefaultValue(0)
	startInterfaceNumber.setReadOnly(True)

	# Adding Number of Interfaces
	numberOfInterfaces = usbDevicePrinterComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_NUMBER_OF_INTERFACES", None)
	numberOfInterfaces.setLabel("Number of Interfaces")
	numberOfInterfaces.setVisible(True)
	numberOfInterfaces.setMin(1)
	numberOfInterfaces.setMax(16)
	numberOfInterfaces.setDefaultValue(printerInterfacesNumber)
	
	# Printer Function driver Read Queue Size 
	queueSizeRead = usbDevicePrinterComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_READ_Q_SIZE", None)
	queueSizeRead.setLabel("Printer Read Queue Size")
	queueSizeRead.setVisible(True)
	queueSizeRead.setMin(1)
	queueSizeRead.setMax(32767)
	queueSizeRead.setDefaultValue(1)
	currentQSizeRead = queueSizeRead.getValue()

	# Printer Function driver Write Queue Size 
	queueSizeWrite = usbDevicePrinterComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_WRITE_Q_SIZE", None)
	queueSizeWrite.setLabel("Printer Write Queue Size")
	queueSizeWrite.setVisible(True)
	queueSizeWrite.setMin(1)
	queueSizeWrite.setMax(32767)
	queueSizeWrite.setDefaultValue(1)	
	currentQSizeWrite = queueSizeWrite.getValue()

	# Printer Function driver Bulk OUT Endpoint Number   
	epNumberBulkOut = usbDevicePrinterComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_BULK_OUT_ENDPOINT_NUMBER", None)
	epNumberBulkOut.setLabel("Bulk OUT Endpoint Number")
	epNumberBulkOut.setVisible(True)
	epNumberBulkOut.setMin(1)
	epNumberBulkOut.setDefaultValue(1)
	if any(x in Variables.get("__PROCESSOR") for x in ["PIC32MZ"]):
		epNumberBulkOut.setMax(7)
	else:
		epNumberBulkOut.setMax(8)	

	usbDevicePrinterBufPool = usbDevicePrinterComponent.createBooleanSymbol("CONFIG_USB_DEVICE_PRINTER_BUFFER_POOL", None)
	usbDevicePrinterBufPool.setLabel("**** Buffer Pool Update ****")
	usbDevicePrinterBufPool.setDependencies(usbDevicePrinterBufferQueueSize, ["CONFIG_USB_DEVICE_FUNCTION_READ_Q_SIZE", "CONFIG_USB_DEVICE_FUNCTION_WRITE_Q_SIZE"])
	usbDevicePrinterBufPool.setVisible(False)
	
	
	############################################################################
	#### Dependency ####
	############################################################################
	# USB DEVICE Printer Common Dependency
	
	Log.writeDebugMessage ("Dependency Started")
	
	numInstances  = Database.getSymbolValue("usb_device_printer", "CONFIG_USB_DEVICE_PRINTER_INSTANCES")
	if (numInstances == None):
		numInstances = 0
		
	queueDepthCombined = Database.getSymbolValue("usb_device_printer", "CONFIG_USB_DEVICE_PRINTER_QUEUE_DEPTH_COMBINED")
	if (queueDepthCombined == None):
		queueDepthCombined = 0

	Database.clearSymbolValue("usb_device_printer", "CONFIG_USB_DEVICE_PRINTER_INSTANCES")
	Database.setSymbolValue("usb_device_printer", "CONFIG_USB_DEVICE_PRINTER_INSTANCES", (index+1), 2)
	Database.clearSymbolValue("usb_device_printer", "CONFIG_USB_DEVICE_PRINTER_QUEUE_DEPTH_COMBINED")
	Database.setSymbolValue("usb_device_printer", "CONFIG_USB_DEVICE_PRINTER_QUEUE_DEPTH_COMBINED", queueDepthCombined + currentQSizeRead + currentQSizeWrite, 2)
	
	
	#############################################################
	# Function Init Entry for Printer
	#############################################################
	usbDevicePrinterFunInitFile = usbDevicePrinterComponent.createFileSymbol(None, None)
	usbDevicePrinterFunInitFile.setType("STRING")
	usbDevicePrinterFunInitFile.setOutputName("usb_device.LIST_USB_DEVICE_FUNCTION_INIT_ENTRY")
	usbDevicePrinterFunInitFile.setSourcePath("templates/device/printer/system_init_c_device_data_printer_function_init.ftl")
	usbDevicePrinterFunInitFile.setMarkup(True)
	
	
	#############################################################
	# Function Registration table for Printer
	#############################################################
	usbDevicePrinterFunRegTableFile = usbDevicePrinterComponent.createFileSymbol(None, None)
	usbDevicePrinterFunRegTableFile.setType("STRING")
	usbDevicePrinterFunRegTableFile.setOutputName("usb_device.LIST_USB_DEVICE_FUNCTION_ENTRY")
	usbDevicePrinterFunRegTableFile.setSourcePath("templates/device/printer/system_init_c_device_data_printer_function.ftl")
	usbDevicePrinterFunRegTableFile.setMarkup(True)
	
	#############################################################
	# HS Descriptors for Printer Function 
	#############################################################
	usbDevicePrinterDescriptorHsFile = usbDevicePrinterComponent.createFileSymbol(None, None)
	usbDevicePrinterDescriptorHsFile.setType("STRING")
	usbDevicePrinterDescriptorHsFile.setOutputName("usb_device.LIST_USB_DEVICE_FUNCTION_DESCRIPTOR_HS_ENTRY")
	usbDevicePrinterDescriptorHsFile.setSourcePath("templates/device/printer/system_init_c_device_data_printer_function_descrptr_hs.ftl")
	usbDevicePrinterDescriptorHsFile.setMarkup(True)
	
	#############################################################
	# FS Descriptors for Printer Function 
	#############################################################
	usbDevicePrinterDescriptorFsFile = usbDevicePrinterComponent.createFileSymbol(None, None)
	usbDevicePrinterDescriptorFsFile.setType("STRING")
	usbDevicePrinterDescriptorFsFile.setOutputName("usb_device.LIST_USB_DEVICE_FUNCTION_DESCRIPTOR_FS_ENTRY")
	usbDevicePrinterDescriptorFsFile.setSourcePath("templates/device/printer/system_init_c_device_data_printer_function_descrptr_fs.ftl")
	usbDevicePrinterDescriptorFsFile.setMarkup(True)
	
	
	#############################################################
	# Class code Entry for Printer Function 
	#############################################################
	usbDevicePrinterDescriptorClassCodeFile = usbDevicePrinterComponent.createFileSymbol(None, None)
	usbDevicePrinterDescriptorClassCodeFile.setType("STRING")
	usbDevicePrinterDescriptorClassCodeFile.setOutputName("usb_device.LIST_USB_DEVICE_DESCRIPTOR_CLASS_CODE_ENTRY")
	usbDevicePrinterDescriptorClassCodeFile.setSourcePath("templates/device/printer/system_init_c_device_data_printer_function_class_codes.ftl")
	usbDevicePrinterDescriptorClassCodeFile.setMarkup(True)
	
	################################################
	# USB Printer Function driver Files 
	################################################
	usbDevicePrinterHeaderFile = usbDevicePrinterComponent.createFileSymbol(None, None)
	addFileName('usb_device_printer.h', usbDevicePrinterComponent, usbDevicePrinterHeaderFile, "middleware/", "/usb/", True, None)
	
	usbPrinterHeaderFile = usbDevicePrinterComponent.createFileSymbol(None, None)
	addFileName('usb_printer.h', usbDevicePrinterComponent, usbPrinterHeaderFile, "middleware/", "/usb/", True, None)
	
	usbDevicePrinterSourceFile = usbDevicePrinterComponent.createFileSymbol(None, None)
	addFileName('usb_device_printer.c', usbDevicePrinterComponent, usbDevicePrinterSourceFile, "middleware/src/", "/usb/src", True, None)
	
	usbDevicePrinterLocalHeaderFile = usbDevicePrinterComponent.createFileSymbol(None, None)
	addFileName('usb_device_printer_local.h', usbDevicePrinterComponent, usbDevicePrinterLocalHeaderFile, "middleware/src/", "/usb/src", True, None)
	
	
	# all files go into src/
def addFileName(fileName, component, symbol, srcPath, destPath, enabled, callback):
	configName1 = Variables.get("__CONFIGURATION_NAME")
	#filename = component.createFileSymbol(None, None)
	symbol.setProjectPath("config/" + configName1 + destPath)
	symbol.setSourcePath(srcPath + fileName)
	symbol.setOutputName(fileName)
	symbol.setDestPath(destPath)
	if fileName[-2:] == '.h':
		symbol.setType("HEADER")
	else:
		symbol.setType("SOURCE")
	symbol.setEnabled(enabled)
	if callback != None:
		symbol.setDependencies(callback, ["USB_DEVICE_FUNCTION_1_DEVICE_CLASS"])