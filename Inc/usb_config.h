#ifndef __USB_CONFIG_H
#define __USB_CONFIG_H

#include "usb.h"
#include "platform.h"

#ifndef __weak
#define __weak __attribute__((weak))
#endif

#define CS_INTERFACE 0x24
#define CS_ENDPOINT 0x25

#define FUNC_HEADER 0x00
#define FUNC_CALL 0x01
#define FUNC_ACM 0x02
#define FUNC_UNION 0x06
#define FUNC_NCM 0x1A
#define FUNC_ECM 0x0F

#pragma pack(1)
typedef struct {
    unsigned char Length;
    unsigned char Type;
} USB_DESCRIPTOR_STRINGS;

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

typedef struct {
    unsigned char Length;
    unsigned char DescriptorType;
    unsigned char FirstInterface;
    unsigned char InterfaceCount;
    unsigned char Class;
    unsigned char SubClass;
    unsigned char Protocol;
    unsigned char strFunction;
} USB_FUNC_IAD;

// CDC120 Table 15
typedef struct {
    unsigned char Length;
    unsigned char Type;
    unsigned char SubType;
    unsigned short CDCVersion;
} USB_DESC_FUNC_HEADER;

// CDC120 Table 16
typedef struct {
    unsigned char Length;
    unsigned char Type;
    unsigned char SubType;
    unsigned char ControlInterface;
    unsigned char SubInterface0;
} USB_DESC_FUNC_UNION1;

// PSTN120 Table 3
typedef struct {
    unsigned char Length;
    unsigned char Type;
    unsigned char SubType;
    unsigned char Capabilities;
    unsigned char DataInterface;
} USB_DESC_FUNC_CallManagement;

// PSTN120 Table 4
typedef struct {
    unsigned char Length;
    unsigned char Type;
    unsigned char SubType;
    unsigned char Capabilities;
} USB_DESC_FUNC_ACM;

// HID Descriptors, see HID1_11
typedef struct {
    unsigned char Length;
    unsigned char Type;
    unsigned short HidVersion;
    unsigned char CountryCode;
    unsigned char Descriptors;
    unsigned char Desc0Type;
    unsigned short Desc0Length;
} USB_DESC_FUNC_HID;

#pragma pack()

typedef struct {
    USB_DESCRIPTOR_DEVICE *(*GetDeviceDescriptor)();
    char *(*GetConfigDescriptor)(short *);
    char *(*GetString)(char, short, short *);
    char *(*GetOSDescriptor)(short *);
    void (*ConfigureEndpoints)();
} USB_Implementation;

/// @brief Set config implementation for example flexibility
/// @param impl The function calls to use
void USB_SetImplementation(USB_Implementation impl);

/// @brief Get the device descriptor
USB_DESCRIPTOR_DEVICE *USB_GetDeviceDescriptor();
/// @brief Get the complete descriptor
/// @param length A pointer which will contain the length of the descriptor
char *USB_GetConfigDescriptor(short *length);
/// @brief Get a string from the string table (str... entries)
/// @param index The index of the string
/// @param lcid The language code of the string
/// @param length Will contain the length of the string
char *USB_GetString(char index, short lcid, short *length);
/// @brief Get the OS Descriptor (String descriptor at 0xEE)
/// @param length Will contain the length of the descriptor
char *USB_GetOSDescriptor(short *length);
/// @brief Configure all endpoints used by the configuration
void USB_ConfigureEndpoints();

/// @brief Handle a setup packet that is targeted at an interface or class
/// @param setup The setup packet
/// @param data A pointer to data that was sent with the setup
/// @param length The length of the data
char USB_HandleClassSetup(USB_SETUP_PACKET *setup, char *data, short length);

/// @brief Called when the host triggers a SetInterface to choose an alternate id
/// @param interface The interface id that was triggered
/// @param alternateId The new alternate id that was activated
void USB_ResetClass(char interface, char alternateId);

/// @brief Suspend the device and go into low power mode
void USB_SuspendDevice();
/// @brief Reactivate the device
void USB_WakeupDevice();

#endif