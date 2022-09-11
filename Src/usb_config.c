#include "usb_config.h"
#include "cdc_device.h"
#include "stm32g441xx.h"

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

/// @brief A Helper to add a descriptor to the configuration buffer
/// @param data The raw descriptor data
/// @param offset The offset in the configuration buffer
static void AddToDescriptor(char *data, short *offset);

USB_DESCRIPTOR_DEVICE *USB_GetDeviceDescriptor() {
    return &DeviceDescriptor;
}

static void AddToDescriptor(char *data, short *offset) {
    short length = data[0];

    for (int i = 0; i < length; i++) {
        ConfigurationBuffer[i + *offset] = data[i];
    }

    *offset += length;
}

char *USB_GetConfigDescriptor(short *length) {
    if (ConfigurationBuffer[0] == 0) {
        short offset = 0;
        AddToDescriptor(&ConfigDescriptor, &offset);
        AddToDescriptor(&CDCManagementInterface, &offset);
        AddToDescriptor(&CDCFuncHeader, &offset);
        AddToDescriptor(&CDCFuncACM, &offset);
        AddToDescriptor(&CDCFuncUnion, &offset);
        AddToDescriptor(&CDCNotificationEndpoint, &offset);
        AddToDescriptor(&CDCDataInterface, &offset);
        AddToDescriptor(&CDCDataEndpoints[0], &offset);
        AddToDescriptor(&CDCDataEndpoints[1], &offset);
    }

    *length = sizeof(ConfigurationBuffer);
    return ConfigurationBuffer;
}

char *USB_GetString(char index, short lcid, short *length) {
    // Strings need to be in unicode (thus prefixed with u"...")
    // The length is double the character count + 2 — or use VSCode which will show the number of bytes on hover
    if (index == 1) {
        *length = 10;
        return u"asdf";
    } else if (index == 2) {
        *length = 28;
        return u"My Controller";
    }

    return 0;
}

char *USB_GetOSDescriptor(short *length) {
    return 0;
}

void USB_ConfigureEndpoints() {
    // Configure all endpoints and route their reception to the functions that need them
    USB_CONFIG_EP Notification = {
        .EP = 1,
        .RxBufferSize = 0,
        .TxBufferSize = 8,
        .Type = USB_EP_INTERRUPT};

    USB_CONFIG_EP DataEP = {
        .EP = 2,
        .RxBufferSize = 64,
        .TxBufferSize = 64,
        .RxCallback = CDC_HandlePacket,
        .Type = USB_EP_BULK};

    USB_SetEPConfig(Notification);
    USB_SetEPConfig(DataEP);
}

char USB_HandleClassSetup(USB_SETUP_PACKET *setup, char *data, short length) {
    // Route the setup packets based on the Interface / Class Index
    return CDC_SetupPacket(setup, data, length);
}

__weak void USB_SuspendDevice() {
}

__weak void USB_WakeupDevice() {
}
