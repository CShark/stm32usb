#include "usb_config.h"
#include "cdc_device.h"
#include "hid_device.h"
#include "stm32g441xx.h"

// Example definition for a Virtual COM Port
static const USB_DESCRIPTOR_DEVICE DeviceDescriptor = {
    .Length = 18,
    .Type = 0x01,
    .USBVersion = 0x0200,
    .DeviceClass = 0x00,
    .DeviceSubClass = 0x00,
    .DeviceProtocol = 0x00,
    .MaxPacketSize = 64,
    .VendorID = 0x16C0,
    .ProductID = 0x088B,
    .DeviceVersion = 0x0100,
    .strManufacturer = 1,
    .strProduct = 2,
    .strSerialNumber = 3,
    .Configurations = 1};

static const USB_DESCRIPTOR_CONFIG ConfigDescriptor = {
    .Length = 9,
    .Type = 0x02,
    .TotalLength = 41,
    .Interfaces = 1,
    .ConfigurationID = 1,
    .strConfiguration = 0,
    .Attributes = (1 << 7),
    .MaxPower = 50};

static const USB_DESCRIPTOR_INTERFACE HIDInterface = {
    .Length = 9,
    .Type = 0x04,
    .InterfaceID = 0,
    .AlternateID = 0,
    .Endpoints = 2,
    .Class = 0x03,
    .SubClass = 0x00,
    .Protocol = 0x00,
    .strInterface = 4};

static const USB_DESC_FUNC_HID HIDDescriptor = {
    .Length = 9,
    .Type = 0x21,
    .HidVersion = 0x0110,
    .CountryCode = 0x00,
    .Descriptors = 1,
    .Desc0Type = 0x22,
    .Desc0Length = 36};

static const USB_DESCRIPTOR_ENDPOINT HIDEndpoints[2] = {
    {.Length = 7,
     .Type = 0x05,
     .Address = (1 << 7) | 1,
     .Attributes = 0x03,
     .MaxPacketSize = 33,
     .Interval = 1},
    {.Length = 7,
     .Type = 0x05,
     .Address = 1,
     .Attributes = 0x03,
     .MaxPacketSize = 33,
     .Interval = 1}};

// Buffer holding the complete descriptor (except the device one) in the correct order
static char ConfigurationBuffer[41] = {0};

static char HIDConfigurationBuffer[] = {
    // Usage page
    0b00000110,
    0xa0, 0xFF,
    // Usage
    0b00001001,
    0xa5,
    // Collection Application
    0b10100001,
    0x01,
    // Usage
    0b00001001,
    0xa6,
    0b00001001,
    0xa7,
    // Logical Min/Max
    0b00010101,
    0x00,
    0b00100101,
    0xFF,
    // Size & Count
    0b01110101,
    8,
    0b10010101,
    33,
    // Output
    0b10000010,
    0b00100010,
    0b00000001,
    // Usage
    0b00001001,
    0xa9,
    // Logical Min/Max
    0b00010101,
    0x00,
    0b00100101,
    0xFF,
    // Size & Count
    0b01110101,
    8,
    0b10010101,
    33,
    // Output
    0b10010010,
    0b00100010,
    0b00000001,
    // End Collection
    0b11000000};

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
        AddToDescriptor(&HIDInterface, &offset);
        AddToDescriptor(&HIDDescriptor, &offset);
        AddToDescriptor(&HIDEndpoints[0], &offset);
        AddToDescriptor(&HIDEndpoints[1], &offset);
    }

    *length = sizeof(ConfigurationBuffer);
    return ConfigurationBuffer;
}

char *USB_GetString(char index, short lcid, short *length) {
    // Strings need to be in unicode (thus prefixed with u"...")
    // The length is double the character count + 2 — or use VSCode which will show the number of bytes on hover
    if (index == 1) {
        *length = 20;
        return u"Housemade";
    } else if (index == 2) {
        *length = 36;
        return u"My DMX Controller";
    } else if(index == 3) {
        *length = 22;
        return u"01234-6786";
    }else if(index == 4) {
        *length = 28;
        return u"DMX Interface";
    }

    return 0;
}

char *USB_GetOSDescriptor(short *length) {
    return 0;
}

void USB_ConfigureEndpoints() {
    // Configure all endpoints and route their reception to the functions that need them
    USB_CONFIG_EP DataEP = {
        .EP = 1,
        .RxBufferSize = 34,
        .TxBufferSize = 34,
        .RxCallback = HID_HandlePacket,
        .Type = USB_EP_INTERRUPT};

    USB_SetEPConfig(DataEP);
}

char USB_HandleClassSetup(USB_SETUP_PACKET *setup, char *data, short length) {
    // Route the setup packets based on the Interface / Class Index
    if (setup->Request == HID_CONFIG_GETDESCRIPTOR) {
        USB_Transmit(0, HIDConfigurationBuffer, sizeof(HIDConfigurationBuffer));
        return USB_OK;
    } else {
        return HID_SetupPacket(setup, data, length);
    }
}

__weak void USB_SuspendDevice() {
}

__weak void USB_WakeupDevice() {
}
