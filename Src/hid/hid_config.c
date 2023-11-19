#include "hid/hid_config.h"
#include "hid/hid_device.h"

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

static const USB_CONFIG_EP EndpointConfigs[1] = {
    {.EP = 1,
     .RxBufferSize = 34,
     .TxBufferSize = 34,
     .RxCallback = HID_HandlePacket,
     .Type = USB_EP_INTERRUPT}};

static char *GetString(char index, short lcid, short *length) {
    if (index == 1) {
        *length = 20;
        return u"Housemade";
    } else if (index == 2) {
        *length = 36;
        return u"My DMX Controller";
    } else if (index == 3) {
        *length = 22;
        return u"01234-6786";
    } else if (index == 4) {
        *length = 28;
        return u"DMX Interface";
    }

    return 0;
}

USB_Implementation HID_GetImplementation() {
    USB_Implementation impl = {0};
    
    unsigned short len = USB_BuildDescriptor(ConfigurationBuffer, sizeof(ConfigurationBuffer), 5,
                                             (void *[]){
                                                 &ConfigDescriptor,
                                                 &HIDInterface,
                                                 &HIDDescriptor,
                                                 &HIDEndpoints[0],
                                                 &HIDEndpoints[1]});

    
    impl.DeviceDescriptor = &DeviceDescriptor;
    impl.ConfigDescriptor = ConfigurationBuffer;
    impl.ConfigDescriptorLength = len;

    impl.Endpoints = EndpointConfigs;
    impl.NumEndpoints = 1;
    impl.NumInterfaces = 1;

    impl.GetString = &GetString;
    return impl;
}