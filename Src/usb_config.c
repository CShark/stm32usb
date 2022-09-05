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

USB_DESCRIPTOR_DEVICE *USB_GetDeviceDescriptor() {
    return &DeviceDescriptor;
}