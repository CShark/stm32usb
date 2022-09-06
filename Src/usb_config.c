#include "usb_config.h"

static USB_DESCRIPTOR_DEVICE DeviceDescriptor = {
    .Length = 18,
    .Type = 0x01,
    .USBVersion = 0x0200,
    .DeviceClass = 0x00,
    .DeviceSubClass = 0x00,
    .DeviceProtocol = 0x00,
    .MaxPacketSize = 64,
    .VendorID = 0x0483,
    .ProductID = 0x5740,
    .DeviceVersion = 0x0001,
    .strManufacturer = 0,
    .strProduct = 0,
    .strSerialNumber = 0,
    .Configurations = 1};

static const USB_DESCRIPTOR_CONFIG ConfigDescriptor = {
    .Length = 9,
    .Type = 0x02,
    .TotalLength = 32,
    .Interfaces = 1,
    .ConfigurationID = 1,
    .strConfiguration = 0,
    .Attributes = (1 << 7),
    .MaxPower = 50};

static const USB_DESCRIPTOR_INTERFACE InterfaceDescriptors[] = {
    {.Length = 9,
     .Type = 0x04,
     .InterfaceID = 0,
     .AlternateID = 0,
     .Endpoints = 2,
     .Class = 0x0A,
     .SubClass = 0x00,
     .Protocol = 0x00,
     .strInterface = 0}};

static const USB_DESCRIPTOR_ENDPOINT EndpointDescriptors[] = {
    {.Length = 7,
     .Type = 0x05,
     .Address = (1 << 7) | 0x01,
     .Attributes = 0x03,
     .MaxPacketSize = 64,
     .Interval = 0xFF},
    {.Length = 7,
     .Type = 0x05,
     .Address = 0x01,
     .Attributes = 0x03,
     .MaxPacketSize = 64,
     .Interval = 0xFF}};

static char ConfigurationBuffer[32] = {0};

static void AddToDescriptor(char *data, short length, short *offset);

USB_DESCRIPTOR_DEVICE *USB_GetDeviceDescriptor() {
    return &DeviceDescriptor;
}

static void AddToDescriptor(char *data, short length, short *offset) {
    for (int i = 0; i < length; i++) {
        ConfigurationBuffer[i + *offset] = data[i];
    }

    *offset += length;
}

char *USB_GetConfigDescriptor(short *length) {
    if(ConfigurationBuffer[0] == 0) {
        short offset = 0;
        AddToDescriptor(&ConfigDescriptor, ConfigDescriptor.Length, &offset);
        AddToDescriptor(&InterfaceDescriptors[0], InterfaceDescriptors[0].Length, &offset);
        AddToDescriptor(&EndpointDescriptors[0], EndpointDescriptors[0].Length, &offset);
        AddToDescriptor(&EndpointDescriptors[1], EndpointDescriptors[1].Length, &offset);
    }

    *length = sizeof(ConfigurationBuffer);
    return ConfigurationBuffer;
}
