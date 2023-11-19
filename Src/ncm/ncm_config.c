#include "ncm/ncm_config.h"
#include "lwip/autoip.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "ncm/ncm_device.h"
#include "ncm/ncm_netif.h"

// Example definition for a NCM-Device
static const USB_DESCRIPTOR_DEVICE DeviceDescriptor = {
    .Length = 18,
    .Type = 0x01,
    .USBVersion = 0x0200,
    .DeviceClass = 0x02,
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
    .TotalLength = 86,
    .Interfaces = 2,
    .ConfigurationID = 1,
    .strConfiguration = 0,
    .Attributes = (1 << 7),
    .MaxPower = 50};

static const USB_DESCRIPTOR_INTERFACE NCMInterface = {
    .Length = 9,
    .Type = 0x04,
    .InterfaceID = 0,
    .AlternateID = 0,
    .Endpoints = 1,
    .Class = 0x02,
    .SubClass = 0x0D,
    .Protocol = 0x00,
    .strInterface = 4};

static const USB_DESC_FUNC_HEADER FuncHeader = {
    .Length = 5,
    .Type = CS_INTERFACE,
    .SubType = FUNC_HEADER,
    .CDCVersion = 0x0110};

static const USB_DESC_FUNC_UNION1 FuncUnion = {
    .Length = 5,
    .Type = CS_INTERFACE,
    .SubType = FUNC_UNION,
    .ControlInterface = 0,
    .SubInterface0 = 1};

static const USB_DESC_FUNC_ECM FuncETH = {
    .Length = 13,
    .Type = CS_INTERFACE,
    .SubType = FUNC_ECM,
    .MaxSegmentSize = 1514,
    .strMacAddress = 20};

static const USB_DESC_FUNC_NCM FuncNCM = {
    .Length = 6,
    .Type = CS_INTERFACE,
    .SubType = FUNC_NCM,
    .NcmVersion = 0x0100,
    .NetworkCapabilities = 0b10000};

static const USB_DESCRIPTOR_INTERFACE DataInterfaces[2] = {
    {.Length = 9,
     .Type = 0x04,
     .InterfaceID = 1,
     .AlternateID = 0,
     .Endpoints = 0,
     .Class = 0x0A,
     .SubClass = 0x00,
     .Protocol = 0x01,
     .strInterface = 0},
    {.Length = 9,
     .Type = 0x04,
     .InterfaceID = 1,
     .AlternateID = 1,
     .Endpoints = 2,
     .Class = 0x0A,
     .SubClass = 0x00,
     .Protocol = 0x01,
     .strInterface = 0}};

static const USB_DESCRIPTOR_ENDPOINT Endpoints[3] = {
    {.Length = 7,
     .Type = 0x05,
     .Address = (1 << 7) | 1,
     .Attributes = 0x03,
     .MaxPacketSize = 16,
     .Interval = 50},
    {.Length = 7,
     .Type = 0x05,
     .Address = 2,
     .Attributes = 0x02,
     .MaxPacketSize = 64,
     .Interval = 1},
    {.Length = 7,
     .Type = 0x05,
     .Address = (1 << 7) | 2,
     .Attributes = 0x02,
     .MaxPacketSize = 64,
     .Interval = 0}};

static char ConfigurationBuffer[86] = {0};

static const USB_CONFIG_EP EndpointConfigs[2] = {
    {.EP = 1,
     .RxBufferSize = 0,
     .TxBufferSize = 16,
     .TxCallback = NCM_ControlTransmit,
     .Type = USB_EP_INTERRUPT},
    {.EP = 2,
     .RxBufferSize = 64,
     .TxBufferSize = 64,
     .RxCallback = NCM_HandlePacket,
     .TxCallback = NCM_BufferTransmitted,
     .Type = USB_EP_BULK}};

static char *GetString(char index, short lcid, short *length) {
    // Strings need to be in unicode (thus prefixed with u"...")
    // The length is double the character count + 2 — or use VSCode which will show the number of bytes on hover
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
    } else if (index == 20) {
        *length = 26;
        return u"445BBD24A371";
    }

    return 0;
}

static char HandleClassSetup(USB_SETUP_PACKET *setup, char *data, short length) {
    // Route the setup packets based on the Interface / Class Index
    NCM_SetupPacket(setup, data, length);
}

static void ResetClass(char interface, char alternateId) {
    NCM_Reset(interface, alternateId);
}

USB_Implementation NCM_GetImplementation() {
    USB_Implementation impl = {0};

    unsigned short len = USB_BuildDescriptor(ConfigurationBuffer, sizeof(ConfigurationBuffer), 11,
                                             (void *[]){
                                                 &ConfigDescriptor,
                                                 &NCMInterface,
                                                 &FuncHeader,
                                                 &FuncUnion,
                                                 &FuncETH,
                                                 &FuncNCM,
                                                 &Endpoints[0],
                                                 &DataInterfaces[0],
                                                 &DataInterfaces[1],
                                                 &Endpoints[1],
                                                 &Endpoints[2],
                                             });

    impl.DeviceDescriptor = &DeviceDescriptor;
    impl.ConfigDescriptor = ConfigurationBuffer;
    impl.ConfigDescriptorLength = len;

    impl.Endpoints = EndpointConfigs;
    impl.NumEndpoints = 2;
    impl.NumInterfaces = 2;

    impl.GetString = &GetString;
    impl.ResetInterface_Handler = &ResetClass;
    impl.SetupPacket_Handler = &HandleClassSetup;

    return impl;
}

// NCM features
static struct netif ncm_if;

void NCM_Init() {
    lwip_init();

    netif_add(&ncm_if, IP4_ADDR_ANY, IP4_ADDR_ANY, IP4_ADDR_ANY, NULL, ncm_netif_init, netif_input);
    netif_set_default(&ncm_if);
    netif_set_up(&ncm_if);

    autoip_start(&ncm_if);
}

void NCM_Loop() {
    ncm_netif_poll(&ncm_if);
    sys_check_timeouts();

    if (sys_now() % 500 == 0) {
        NCM_FlushTx();
    }
}