#ifndef __USB_CONFIG_H
#define __USB_CONFIG_H

#ifndef __weak
#define __weak __attribute__((weak))
#endif

typedef struct {
    unsigned char Length;
    unsigned char Type;
    unsigned short USBVersion;
    unsigned char DeviceClass;
    unsigned char DeviceSubClass;
    unsigned char DeviceProtocol;
    unsigned char MaxPacketSize;
    unsigned short VendorID;
    unsigned short ProductID;
    unsigned short DeviceVersion;
    unsigned char strManufacturer;
    unsigned char strProduct;
    unsigned char strSerialNumber;
    unsigned char Configurations;
} USB_DESCRIPTOR_DEVICE;

typedef struct {
    unsigned char Length;
    unsigned char Type;
    unsigned short TotalLength;
    unsigned char Interfaces;
    unsigned char ConfigurationID;
    unsigned char strConfiguration;
    unsigned char Attributes;
    unsigned char MaxPower;
} USB_DESCRIPTOR_CONFIG;

typedef struct {
    unsigned char Length;
    unsigned char Type;
    unsigned char InterfaceID;
    unsigned char AlternateID;
    unsigned char Endpoints;
    unsigned char Class;
    unsigned char SubClass;
    unsigned char Protocol;
    unsigned char strInterface;
} USB_DESCRIPTOR_INTERFACE;

typedef struct {
    unsigned char Length;
    unsigned char Type;
    unsigned char Address;
    unsigned char Attributes;
    unsigned short MaxPacketSize;
    unsigned char Interval;
} USB_DESCRIPTOR_ENDPOINT;

#define USB_SelfPowered 0
#define USB_NumInterfaces 1
#define USB_NumEndpoints 2 // Config + 1 Bidirectional

USB_DESCRIPTOR_DEVICE *USB_GetDeviceDescriptor();
char *USB_GetConfigDescriptor(short *length);

void USB_SuspendDevice();
void USB_WakeupDevice();

#endif