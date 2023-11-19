#include "cdc/cdc_config.h"
#include "cdc/cdc_device.h"

// Example definition for a Virtual COM Port
static const USB_DESCRIPTOR_DEVICE DeviceDescriptor = {
    .Length = 18,
    .Type = 0x01,
    .USBVersion = 0x0200,
    .DeviceClass = 0x02,
    .DeviceSubClass = 0x00,
    .DeviceProtocol = 0x00,
    .MaxPacketSize = 64,
    .VendorID = 0xDEAD,  // 0x0483,
    .ProductID = 0xBEEF, // 0x5740,
    .DeviceVersion = 0x0100,
    .strManufacturer = 1,
    .strProduct = 2,
    .strSerialNumber = 0,
    .Configurations = 1};

static const USB_DESCRIPTOR_CONFIG ConfigDescriptor = {
    .Length = 9,
    .Type = 0x02,
    .TotalLength = 62,
    .Interfaces = 2,
    .ConfigurationID = 1,
    .strConfiguration = 0,
    .Attributes = (1 << 7),
    .MaxPower = 50};

static const USB_DESCRIPTOR_INTERFACE CDCManagementInterface = {
    .Length = 9,
    .Type = 0x04,
    .InterfaceID = 0,
    .AlternateID = 0,
    .Endpoints = 1,
    .Class = 0x02,
    .SubClass = 0x02,
    .Protocol = 0x01,
    .strInterface = 0};

static const USB_DESC_FUNC_HEADER CDCFuncHeader = {
    .Length = 5,
    .Type = 0x24,
    .SubType = 0x00,
    .CDCVersion = 0x0110};

static const USB_DESC_FUNC_ACM CDCFuncACM = {
    .Length = 4,
    .Type = 0x24,
    .SubType = 0x02,
    .Capabilities = (1 << 1)};

static const USB_DESC_FUNC_CallManagement CDCFuncCall = {
    .Length = 5,
    .Type = 0x24,
    .SubType = 0x01,
    .Capabilities = 0x00,
    .DataInterface = 1};

static const USB_DESC_FUNC_UNION1 CDCFuncUnion = {
    .Length = 5,
    .Type = 0x24,
    .SubType = 0x06,
    .ControlInterface = 0,
    .SubInterface0 = 1};

static const USB_DESCRIPTOR_ENDPOINT CDCNotificationEndpoint = {
    .Length = 7,
    .Type = 0x05,
    .Address = (1 << 7) | 1,
    .Attributes = 0x03,
    .MaxPacketSize = 8,
    .Interval = 0x10};

static const USB_DESCRIPTOR_INTERFACE CDCDataInterface = {
    .Length = 9,
    .Type = 0x04,
    .InterfaceID = 1,
    .AlternateID = 0,
    .Endpoints = 2,
    .Class = 0x0A,
    .SubClass = 0x00,
    .Protocol = 0x00,
    .strInterface = 0};

static const USB_DESCRIPTOR_ENDPOINT CDCDataEndpoints[2] = {
    {.Length = 7,
     .Type = 0x05,
     .Address = (1 << 7) | 2,
     .Attributes = 0x02,
     .MaxPacketSize = 64,
     .Interval = 0x00},
    {.Length = 7,
     .Type = 0x05,
     .Address = 2,
     .Attributes = 0x02,
     .MaxPacketSize = 64,
     .Interval = 0x00}};

// Buffer holding the complete descriptor (except the device one) in the correct order
static char ConfigurationBuffer[62] = {0};

static const USB_CONFIG_EP EndpointConfigs[2] = {
    {.EP = 1,
     .RxBufferSize = 0,
     .TxBufferSize = 8,
     .Type = USB_EP_INTERRUPT},
    {.EP = 2,
     .RxBufferSize = 64,
     .TxBufferSize = 64,
     .RxCallback = CDC_HandlePacket,
     .Type = USB_EP_BULK}};

static char *GetConfigDescriptor(short *length) {
    if (ConfigurationBuffer[0] == 0) {
        USB_BuildDescriptor(ConfigurationBuffer, sizeof(ConfigurationBuffer), 9,
                            (void *[]){
                                &ConfigDescriptor,
                                &CDCManagementInterface,
                                &CDCFuncHeader,
                                &CDCFuncACM,
                                &CDCFuncUnion,
                                &CDCNotificationEndpoint,
                                &CDCDataInterface,
                                &CDCDataEndpoints[0],
                                &CDCDataEndpoints[1]});
    }

    *length = sizeof(ConfigurationBuffer);
    return ConfigurationBuffer;
}

static char *GetString(char index, short lcid, short *length) {
    if (index == 1) {
        *length = 10;
        return u"asdf";
    } else if (index == 2) {
        *length = 28;
        return u"My Controller";
    }

    return 0;
}

static char SetupPacket_Handler(USB_SETUP_PACKET *setup, const unsigned char *data, short length) {
    return CDC_SetupPacket(setup, data, length);
}

USB_Implementation CDC_GetImplementation() {
    USB_Implementation impl = {0};
    unsigned short len = USB_BuildDescriptor(ConfigurationBuffer, sizeof(ConfigurationBuffer), 9,
                                             (void *[]){
                                                 &ConfigDescriptor,
                                                 &CDCManagementInterface,
                                                 &CDCFuncHeader,
                                                 &CDCFuncACM,
                                                 &CDCFuncUnion,
                                                 &CDCNotificationEndpoint,
                                                 &CDCDataInterface,
                                                 &CDCDataEndpoints[0],
                                                 &CDCDataEndpoints[1]});

    impl.DeviceDescriptor = &DeviceDescriptor;
    impl.ConfigDescriptor = ConfigurationBuffer;
    impl.ConfigDescriptorLength = len;
    impl.Endpoints = EndpointConfigs;
    impl.NumEndpoints = 2;
    impl.NumInterfaces = 2;

    impl.GetString = &GetString;
    impl.SetupPacket_Handler = &SetupPacket_Handler;

    return impl;
}