#ifndef __NCM_DEVICE_H
#define __NCM_DEVICE_H

#include "usb.h"

#define NCM_SET_NTB_INPUT_SIZE 0x86
#define NCM_GET_NTB_INPUT_SIZE 0x85
#define NCM_GET_NTB_PARAMETERS 0x80

#define NCM_NETWORK_CONNECTION 0x00
#define NCM_NETWORK_SPEEDCHANGE 0x2A

#pragma pack(1)
// NCM10 Table 6-3
typedef struct {
    unsigned short Length;
    unsigned short NtbFormatsSupported;
    unsigned int NtbInMaxSize;
    unsigned short NdpInDivisor;
    unsigned short NdpInPayloadRemainder;
    unsigned short NdpInAlignment;
    unsigned short _reserved;
    unsigned int NtbOutMaxSize;
    unsigned short NdpOutDivisior;
    unsigned short NdpOutPayloadRemainder;
    unsigned short NdpOutAlignment;
    unsigned short NtbOutMaxDatagrams;
} USB_NTB_PARAMS;

typedef struct {
    unsigned int NtbInMaxSize;
    unsigned short NtbInMaxDatagrams;
    unsigned short _reserved;
} USB_NTB_INPUT_SIZE;

typedef struct {
    USB_SETUP_PACKET SetupPacket;
    unsigned int DlBitRate;
    unsigned int UlBitRate;
} USB_NCM_SPEEDDATA;

typedef struct {
    unsigned char Signature[4];
    unsigned short HeaderLength;
    unsigned short Sequence;
    unsigned short BlockLength;
    unsigned short NdpOffset;
} NCM_NTB_HEADER_16;

typedef struct {
    unsigned char Signature[4];
    unsigned short Length;
    unsigned short NextNdpOffset;
} NCM_NTB_POINTER_16;

typedef struct {
    unsigned short DatagramOffset;
    unsigned short DatagramLength;
} NCM_NTB_DATAPOINTER_16;
#pragma pack()

typedef struct NCM_CtrlTxInfo NCM_CtrlTxInfo;
typedef struct NCM_BufferInfo NCM_BufferInfo;

struct NCM_CtrlTxInfo {
    char *buffer;
    unsigned short length;
    NCM_CtrlTxInfo *next;
};

typedef enum {
    NCM_BUF_UNUSED,
    NCM_BUF_READY,
    NCM_BUF_LOCKED,
} NCM_BufferState;

struct NCM_BufferInfo {
    char *buffer;
    NCM_BufferState status;
    unsigned short length;
    unsigned short offset;
    NCM_BufferInfo *next;
};

typedef struct {
    NCM_BufferInfo *buffer;
    unsigned char datagramCount;
    unsigned short offset;
    unsigned short sequence;
    NCM_NTB_DATAPOINTER_16 datagrams[10];
} NCM_TX_BufferInfo;

typedef struct {
    NCM_BufferInfo *buffer;
    NCM_NTB_POINTER_16 *ndp;
    NCM_NTB_DATAPOINTER_16 *datagramm;
} NCM_RX_BufferInfo;

char NCM_SetupPacket(USB_SETUP_PACKET *setup, char *data, short length);
void NCM_HandlePacket(char ep, short length);
void NCM_Reset(char interface, char alternateId);
void NCM_ControlTransmit(char ep, short length);

void NCM_LinkUp();
void NCM_LinkDown();

char *NCM_GetNextRxDatagramBuffer(short *length);
char *NCM_GetNextTxDatagramBuffer(short length);
void NCM_FlushTx();
void NCM_BufferTransmitted(char ep, short length);

#endif
