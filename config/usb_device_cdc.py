currentQSizeRead  = 1
currentQSizeWrite = 1
currentQSizeSerialStateNotification = 1
cdcInterfacesNumber = 2
cdcDescriptorSize = 58
indexFunction = None
configValue = None 
startInterfaceNumber = None 
numberOfInterfaces = None 
useIad = None 
epNumberInterrupt = None
epNumberBulkOut = None
epNumberBulkIn = None

def onAttachmentConnected(source, target):
	global cdcInterfacesNumber
	global cdcDescriptorSize
	global configValue
	global startInterfaceNumber
	global numberOfInterfaces
	global useIad
	global epNumberInterrupt
	global epNumberBulkOut
	global epNumberBulkIn
	dependencyID = source["id"]
	ownerComponent = source["component"]
	
	# Read number of functions from USB Device Layer 
	nFunctions = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_FUNCTIONS_NUMBER")

	if nFunctions != None: 
		#Log.writeDebugMessage ("USB Device CDC Function Driver: Attachment connected")
		
		# Update Number of Functions in USB Device, Increment the value by One. 
		Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_FUNCTIONS_NUMBER")
		Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_FUNCTIONS_NUMBER", nFunctions + 1 , 2)
	
		# If we have CDC function driver plus any function driver (no matter what Class), we enable IAD. 
		if nFunctions > 0:
			Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_DESCRIPTOR_IAD_ENABLE")
			Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_DESCRIPTOR_IAD_ENABLE", True, 2)
			iadEnableSymbol = ownerComponent.getSymbolByID("CONFIG_USB_DEVICE_FUNCTION_USE_IAD")
			iadEnableSymbol.clearValue()
			iadEnableSymbol.setValue(True, 1)
		
			Database.clearSymbolValue("usb_device_cdc_0", "CONFIG_USB_DEVICE_FUNCTION_USE_IAD")
			Database.setSymbolValue("usb_device_cdc_0", "CONFIG_USB_DEVICE_FUNCTION_USE_IAD", True, 2)
			configDescriptorSize = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE")
			if configDescriptorSize != None:
				Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE")
				Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE", configDescriptorSize + 8 , 2)
		
		configDescriptorSize = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE")
		if configDescriptorSize != None: 
			Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE")
			iadEnableSymbol = ownerComponent.getSymbolByID("CONFIG_USB_DEVICE_FUNCTION_USE_IAD")
			if iadEnableSymbol.getValue() == True:
				descriptorSize =  cdcDescriptorSize + 8
			else:
				descriptorSize =  cdcDescriptorSize
			Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE", configDescriptorSize + descriptorSize , 2)
	
		nInterfaces = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_INTERFACES_NUMBER")
		if nInterfaces != None: 
			Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_INTERFACES_NUMBER")
			Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_INTERFACES_NUMBER", nInterfaces + cdcInterfacesNumber , 2)
			startInterfaceNumber.setValue(nInterfaces, 1)
			
		nEndpoints = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER")
		if nEndpoints != None:
			Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER")
			Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER", nEndpoints + 3 , 2)
			epNumberInterrupt.setValue(nEndpoints + 1, 1)
			epNumberBulkOut.setValue(nEndpoints + 2, 1)
			epNumberBulkIn.setValue(nEndpoints + 3, 1)

def onAttachmentDisconnected(source, target):

	print ("CDC Function Driver: Detached")
	global cdcInterfacesNumber
	global cdcDescriptorSize
	global configValue
	global startInterfaceNumber
	global numberOfInterfaces
	global useIad
	global epNumberInterrupt
	global epNumberBulkOut
	global epNumberBulkIn
	dependencyID = source["id"]
	ownerComponent = source["component"]
	
	nFunctions = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_FUNCTIONS_NUMBER")
	if nFunctions != None: 
		nFunctions = nFunctions - 1
		Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_FUNCTIONS_NUMBER")
		Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_FUNCTIONS_NUMBER", nFunctions , 2)
	
	endpointNumber = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER")
	if endpointNumber != None: 
		Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER")
		Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_ENDPOINTS_NUMBER", endpointNumber -  3 , 2)
	
	interfaceNumber = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_INTERFACES_NUMBER")
	if interfaceNumber != None: 
		Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_INTERFACES_NUMBER")
		Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_INTERFACES_NUMBER", interfaceNumber - 2, 2)
		
	nCDCInstances = Database.getSymbolValue("usb_device_cdc", "CONFIG_USB_DEVICE_CDC_INSTANCES")
	if nCDCInstances != None:
		nCDCInstances = nCDCInstances - 1
		Database.clearSymbolValue("usb_device_cdc", "CONFIG_USB_DEVICE_CDC_INSTANCES")
		Database.setSymbolValue("usb_device_cdc", "CONFIG_USB_DEVICE_CDC_INSTANCES", nCDCInstances, 2)
		if nCDCInstances == 1 and nFunctions != None and nFunctions == 1:
			Database.clearSymbolValue("usb_device_cdc_0", "CONFIG_USB_DEVICE_FUNCTION_USE_IAD")
			Database.setSymbolValue("usb_device_cdc_0", "CONFIG_USB_DEVICE_FUNCTION_USE_IAD", False, 2)
			
			configDescriptorSize = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE")
			if configDescriptorSize != None:
				Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE")
				Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE", configDescriptorSize - 8 , 2)
	
	configDescriptorSize = Database.getSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE")
	if configDescriptorSize != None: 
		Database.clearSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE")
		if useIad.getValue() == True:
			descriptorSize =  cdcDescriptorSize + 8
		else:
			descriptorSize =  cdcDescriptorSize
		Database.setSymbolValue("usb_device", "CONFIG_USB_DEVICE_CONFIG_DESCRPTR_SIZE", configDescriptorSize - descriptorSize , 2)
		
	
def destroyComponent(component):
	print ("CDC Function Driver: Destroyed")
			
		
	
def usbDeviceCdcBufferQueueSize(usbSymbolSource, event):
	global currentQSizeRead
	global currentQSizeWrite
	global currentQSizeSerialStateNotification
	queueDepthCombined = Database.getSymbolValue("usb_device_cdc", "CONFIG_USB_DEVICE_CDC_QUEUE_DEPTH_COMBINED")
	if (event["id"] == "CONFIG_USB_DEVICE_FUNCTION_READ_Q_SIZE"):
		queueDepthCombined = queueDepthCombined - currentQSizeRead + event["value"]
		currentQSizeRead = event["value"]
	if (event["id"] == "CONFIG_USB_DEVICE_FUNCTION_WRITE_Q_SIZE"):
		queueDepthCombined = queueDepthCombined - currentQSizeWrite  + event["value"]
		currentQSizeWrite = event["value"]
	if (event["id"] == "CONFIG_USB_DEVICE_FUNCTION_SERIAL_NOTIFIACATION_Q_SIZE"):
		queueDepthCombined = queueDepthCombined - currentQSizeSerialStateNotification + event["value"]
		currentQSizeSerialStateNotification = event["value"]
	Database.clearSymbolValue("usb_device_cdc", "CONFIG_USB_DEVICE_CDC_QUEUE_DEPTH_COMBINED")
	Database.setSymbolValue("usb_device_cdc", "CONFIG_USB_DEVICE_CDC_QUEUE_DEPTH_COMBINED", queueDepthCombined, 2)
	
def instantiateComponent(usbDeviceCdcComponent, index):
	global cdcDescriptorSize
	global cdcInterfacesNumber
	global cdcDescriptorSize
	global configValue
	global startInterfaceNumber
	global numberOfInterfaces
	global useIad
	global currentQSizeRead
	global currentQSizeWrite
	global currentQSizeSerialStateNotification
	global epNumberInterrupt
	global epNumberBulkOut
	global epNumberBulkIn
	
	res = Database.activateComponents(["usb_device"])
	
	# Index of this function 
	indexFunction = usbDeviceCdcComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_INDEX", None)
	indexFunction.setVisible(False)
	indexFunction.setMin(0)
	indexFunction.setMax(16)
	indexFunction.setDefaultValue(index)
	
	# Config name: Configuration number
	configValue = usbDeviceCdcComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_CONFIG_VALUE", None)
	configValue.setLabel("Configuration Value")
	configValue.setVisible(False)
	configValue.setMin(1)
	configValue.setMax(16)
	configValue.setDefaultValue(1)
	configValue.setReadOnly(True)
	
	# Adding Start Interface number 
	startInterfaceNumber = usbDeviceCdcComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_INTERFACE_NUMBER", None)
	startInterfaceNumber.setLabel("Start Interface Number")
	startInterfaceNumber.setVisible(True)
	startInterfaceNumber.setMin(0)
	startInterfaceNumber.setDefaultValue(0)
	startInterfaceNumber.setReadOnly(True)
	
	# Adding Number of Interfaces
	numberOfInterfaces = usbDeviceCdcComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_NUMBER_OF_INTERFACES", None)
	numberOfInterfaces.setLabel("Number of Interfaces")
	numberOfInterfaces.setVisible(True)
	numberOfInterfaces.setMin(1)
	numberOfInterfaces.setMax(16)
	numberOfInterfaces.setDefaultValue(2)
	numberOfInterfaces.setReadOnly(True)
	
	
	# Use IAD
	useIad = usbDeviceCdcComponent.createBooleanSymbol("CONFIG_USB_DEVICE_FUNCTION_USE_IAD", None)
	useIad.setLabel("Use Interface Association Descriptor")
	useIad.setVisible(True)
	useIad.setDefaultValue(False)
	useIad.setUseSingleDynamicValue(True)
	
	# CDC Function driver Read Queue Size 
	queueSizeRead = usbDeviceCdcComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_READ_Q_SIZE", None)
	queueSizeRead.setLabel("CDC Read Queue Size")
	queueSizeRead.setVisible(True)
	queueSizeRead.setMin(1)
	queueSizeRead.setMax(32767)
	queueSizeRead.setDefaultValue(1)
	currentQSizeRead = queueSizeRead.getValue()

	
	# CDC Function driver Write Queue Size 
	queueSizeWrite = usbDeviceCdcComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_WRITE_Q_SIZE", None)
	queueSizeWrite.setLabel("CDC Write Queue Size")
	queueSizeWrite.setVisible(True)
	queueSizeWrite.setMin(1)
	queueSizeWrite.setMax(32767)
	queueSizeWrite.setDefaultValue(1)	
	currentQSizeWrite = queueSizeWrite.getValue()
	
	
	# CDC Function driver Serial state notification Queue Size  
	queueSizeSerialStateNotification = usbDeviceCdcComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_SERIAL_NOTIFIACATION_Q_SIZE", None)
	queueSizeSerialStateNotification.setLabel("CDC Serial Notification Queue Size")
	queueSizeSerialStateNotification.setVisible(True)
	queueSizeSerialStateNotification.setMin(1)
	queueSizeSerialStateNotification.setMax(32767)
	queueSizeSerialStateNotification.setDefaultValue(1)	
	currentQSizeSerialStateNotification = queueSizeSerialStateNotification.getValue()
	
	# CDC Function driver Notification Endpoint Number  
	epNumberInterrupt = usbDeviceCdcComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_INT_ENDPOINT_NUMBER", None)
	epNumberInterrupt.setLabel("Interrupt Endpoint Number")
	epNumberInterrupt.setVisible(True)
	epNumberInterrupt.setMin(1)
	epNumberInterrupt.setMax(10)
	epNumberInterrupt.setDefaultValue(1)

	# CDC Function driver Data OUT Endpoint Number   
	epNumberBulkOut = usbDeviceCdcComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_BULK_OUT_ENDPOINT_NUMBER", None)
	epNumberBulkOut.setLabel("Bulk OUT Endpoint Number")
	epNumberBulkOut.setVisible(True)
	epNumberBulkOut.setMin(1)
	epNumberBulkOut.setMax(10)
	epNumberBulkOut.setDefaultValue(2)
	
	# CDC Function driver Data IN Endpoint Number   
	epNumberBulkIn = usbDeviceCdcComponent.createIntegerSymbol("CONFIG_USB_DEVICE_FUNCTION_BULK_IN_ENDPOINT_NUMBER", None)
	epNumberBulkIn.setLabel("Bulk IN Endpoint Number")
	epNumberBulkIn.setVisible(True)
	epNumberBulkIn.setMin(1)
	epNumberBulkIn.setMax(10)
	epNumberBulkIn.setDefaultValue(3)
	
	usbDeviceCdcBufPool = usbDeviceCdcComponent.createBooleanSymbol("CONFIG_USB_DEVICE_CDC_BUFFER_POOL", None)
	usbDeviceCdcBufPool.setLabel("**** Buffer Pool Update ****")
	usbDeviceCdcBufPool.setDependencies(usbDeviceCdcBufferQueueSize, ["CONFIG_USB_DEVICE_FUNCTION_READ_Q_SIZE", "CONFIG_USB_DEVICE_FUNCTION_WRITE_Q_SIZE", "CONFIG_USB_DEVICE_FUNCTION_SERIAL_NOTIFIACATION_Q_SIZE"])
	usbDeviceCdcBufPool.setVisible(False)
	
	
	

	############################################################################
	#### Dependency ####
	############################################################################
	# USB DEVICE CDC Common Dependency
	
	Log.writeDebugMessage ("Dependency Started")
	
	numInstances  = Database.getSymbolValue("usb_device_cdc", "CONFIG_USB_DEVICE_CDC_INSTANCES")
	if (numInstances == None):
		numInstances = 0
		
	queueDepthCombined = Database.getSymbolValue("usb_device_cdc", "CONFIG_USB_DEVICE_CDC_QUEUE_DEPTH_COMBINED")
	if (queueDepthCombined == None):
		queueDepthCombined = 0

	Database.clearSymbolValue("usb_device_cdc", "CONFIG_USB_DEVICE_CDC_INSTANCES")
	Database.setSymbolValue("usb_device_cdc", "CONFIG_USB_DEVICE_CDC_INSTANCES", (index+1), 2)
	Database.clearSymbolValue("usb_device_cdc", "CONFIG_USB_DEVICE_CDC_QUEUE_DEPTH_COMBINED")
	Database.setSymbolValue("usb_device_cdc", "CONFIG_USB_DEVICE_CDC_QUEUE_DEPTH_COMBINED", queueDepthCombined + currentQSizeRead + currentQSizeWrite + currentQSizeSerialStateNotification, 2)
	
	
	#############################################################
	# Function Init Entry for CDC
	#############################################################
	usbDeviceCdcFunInitFile = usbDeviceCdcComponent.createFileSymbol(None, None)
	usbDeviceCdcFunInitFile.setType("STRING")
	usbDeviceCdcFunInitFile.setOutputName("usb_device.LIST_USB_DEVICE_FUNCTION_INIT_ENTRY")
	usbDeviceCdcFunInitFile.setSourcePath("templates/device/cdc/system_init_c_device_data_cdc_function_init.ftl")
	usbDeviceCdcFunInitFile.setMarkup(True)
	
	
	#############################################################
	# Function Registration table for CDC
	#############################################################
	usbDeviceCdcFunRegTableFile = usbDeviceCdcComponent.createFileSymbol(None, None)
	usbDeviceCdcFunRegTableFile.setType("STRING")
	usbDeviceCdcFunRegTableFile.setOutputName("usb_device.LIST_USB_DEVICE_FUNCTION_ENTRY")
	usbDeviceCdcFunRegTableFile.setSourcePath("templates/device/cdc/system_init_c_device_data_cdc_function.ftl")
	usbDeviceCdcFunRegTableFile.setMarkup(True)
	
	#############################################################
	# HS Descriptors for CDC Function 
	#############################################################
	usbDeviceCdcDescriptorHsFile = usbDeviceCdcComponent.createFileSymbol(None, None)
	usbDeviceCdcDescriptorHsFile.setType("STRING")
	usbDeviceCdcDescriptorHsFile.setOutputName("usb_device.LIST_USB_DEVICE_FUNCTION_DESCRIPTOR_HS_ENTRY")
	usbDeviceCdcDescriptorHsFile.setSourcePath("templates/device/cdc/system_init_c_device_data_cdc_function_descrptr_hs.ftl")
	usbDeviceCdcDescriptorHsFile.setMarkup(True)
	
	#############################################################
	# FS Descriptors for CDC Function 
	#############################################################
	usbDeviceCdcDescriptorFsFile = usbDeviceCdcComponent.createFileSymbol(None, None)
	usbDeviceCdcDescriptorFsFile.setType("STRING")
	usbDeviceCdcDescriptorFsFile.setOutputName("usb_device.LIST_USB_DEVICE_FUNCTION_DESCRIPTOR_FS_ENTRY")
	usbDeviceCdcDescriptorFsFile.setSourcePath("templates/device/cdc/system_init_c_device_data_cdc_function_descrptr_fs.ftl")
	usbDeviceCdcDescriptorFsFile.setMarkup(True)
	
	
	#############################################################
	# Class code Entry for CDC Function 
	#############################################################
	usbDeviceCdcDescriptorClassCodeFile = usbDeviceCdcComponent.createFileSymbol(None, None)
	usbDeviceCdcDescriptorClassCodeFile.setType("STRING")
	usbDeviceCdcDescriptorClassCodeFile.setOutputName("usb_device.LIST_USB_DEVICE_DESCRIPTOR_CLASS_CODE_ENTRY")
	usbDeviceCdcDescriptorClassCodeFile.setSourcePath("templates/device/cdc/system_init_c_device_data_cdc_function_class_codes.ftl")
	usbDeviceCdcDescriptorClassCodeFile.setMarkup(True)
	
	################################################
	# USB CDC Function driver Files 
	################################################
	usbDeviceCdcHeaderFile = usbDeviceCdcComponent.createFileSymbol(None, None)
	addFileName('usb_device_cdc.h', usbDeviceCdcComponent, usbDeviceCdcHeaderFile, "", "/usb/", True, None)
	
	usbCdcHeaderFile = usbDeviceCdcComponent.createFileSymbol(None, None)
	addFileName('usb_cdc.h', usbDeviceCdcComponent, usbCdcHeaderFile, "", "/usb/", True, None)
	
	usbDeviceCdcSourceFile = usbDeviceCdcComponent.createFileSymbol(None, None)
	addFileName('usb_device_cdc.c', usbDeviceCdcComponent, usbDeviceCdcSourceFile, "src/", "/usb/src", True, None)
	
	usbDeviceCdcAcmSourceFile = usbDeviceCdcComponent.createFileSymbol(None, None)
	addFileName('usb_device_cdc_acm.c', usbDeviceCdcComponent, usbDeviceCdcAcmSourceFile, "src/", "/usb/src", True, None)
	
	usbDeviceCdcLocalHeaderFile = usbDeviceCdcComponent.createFileSymbol(None, None)
	addFileName('usb_device_cdc_local.h', usbDeviceCdcComponent, usbDeviceCdcLocalHeaderFile, "src/", "/usb/src", True, None)
	
	
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