#ifndef __USB_CONFIG_H
#define __USB_CONFIG_H

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

// ECM120 Table 3
typedef struct {
    unsigned char Length;
    unsigned char Type;
    unsigned char SubType;
    unsigned char strMacAddress;
    unsigned int EthernetStatistics;
    unsigned short MaxSegmentSize;
    unsigned short NumberMcFilters;
    unsigned char NumberPowerFilters;
} USB_DESC_FUNC_ECM;

// NCM10 Table 5-2
typedef struct {
    unsigned char Length;
    unsigned char Type;
    unsigned char SubType;
    unsigned short NcmVersion;
    unsigned char NetworkCapabilities;
} USB_DESC_FUNC_NCM;

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

#define USB_SelfPowered 0
#define USB_NumEndpoints 8
#define USB_MaxControlData 64

#endif