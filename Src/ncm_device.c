#include "ncm_device.h"

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

static char buffers[6][2048] = {};

static NCM_BufferInfo txDef[3] = {
    {.buffer = buffers[0],
     .next = &txDef[1]},
    {.buffer = buffers[1],
     .next = &txDef[2]},
    {.buffer = buffers[2],
     .next = &txDef[0]}};

static NCM_BufferInfo rxDef[3] = {
    {.buffer = buffers[3],
     .next = &rxDef[1]},
    {.buffer = buffers[4],
     .next = &rxDef[2]},
    {.buffer = buffers[5],
     .next = &rxDef[0]}};

static NCM_BufferInfo *tx = &txDef[0];
static NCM_BufferInfo *rx = &rxDef[0];
static NCM_TX_BufferInfo activeTxBuffer = {
    .buffer = &txDef[0]};
static NCM_RX_BufferInfo activeRxBuffer = {
    .buffer = &rxDef[0],
    .datagramm = 0,
    .ndp = 0};

static USB_NTB_INPUT_SIZE ntbInputSize = {
    .NtbInMaxDatagrams = 0,
    .NtbInMaxSize = 2048,
};

static const USB_NTB_PARAMS ntb_params = {
    .Length = 0x1C,
    .NtbFormatsSupported = 0b01,
    .NtbInMaxSize = 2048,
    .NdpInDivisor = 1,
    .NdpInPayloadRemainder = 0,
    .NdpInAlignment = 4,

    .NtbOutMaxSize = 2048,
    .NdpOutDivisior = 1,
    .NdpOutPayloadRemainder = 0,
    .NdpOutAlignment = 4,

    .NtbOutMaxDatagrams = 10};

static void NCM_TransmitNextBuffer();

char NCM_SetupPacket(USB_SETUP_PACKET *setup, char *data, short length) {
    switch (setup->Request) {
    case NCM_GET_NTB_INPUT_SIZE: {
        if (length == 4) {
            USB_Transmit(0, &ntbInputSize, 4);
        } else if (length == 8) {
            USB_Transmit(0, &ntbInputSize, 8);
        } else {
            return USB_ERR;
        }
        return USB_OK;
        break;
    }
    case NCM_SET_NTB_INPUT_SIZE: {
        USB_NTB_INPUT_SIZE *info = data;

        if (length >= 4) {
            ntbInputSize.NtbInMaxSize = info->NtbInMaxSize;
        }

        if (length == 8) {
            ntbInputSize.NtbInMaxDatagrams = info->NtbInMaxDatagrams;
        }
        return USB_OK;
        break;
    }
    case NCM_GET_NTB_PARAMETERS: {
        USB_Transmit(0, &ntb_params, sizeof(USB_NTB_PARAMS));
        return USB_OK;
        break;
    }
    }

    return USB_ERR;
}

void NCM_HandlePacket(char ep, short length) {
    if (ep == 2) {
        while (rx->status == NCM_BUF_LOCKED) {
            rx = rx->next;
        }

        rx->status = NCM_BUF_UNUSED;
        short received = 2048 - rx->offset;
        USB_Fetch(ep, rx->buffer + rx->offset, &received);

        NCM_NTB_HEADER_16 *header = rx->buffer + rx->offset;

        if (rx->offset == 0) {
            if (header->Signature[0] != 'N' || header->Signature[1] != 'C' || header->Signature[2] != 'M' || header->Signature[3] != 'H') {
                return;
            }

            rx->length = header->BlockLength;
        } else {
            // Try Detect broken packages
            if (header->Signature[0] == 'N' && header->Signature[1] == 'C' && header->Signature[2] == 'M' && header->Signature[3] == 'H') {
                rx->offset = 0;
                NCM_HandlePacket(ep, length);
                return;
            }
        }

        rx->offset += received;

        if (rx->length == 0) {
            if (received < 64) {
                NCM_NTB_HEADER_16 *header = rx->buffer;
                header->BlockLength = rx->offset;
                rx->length = rx->offset;

                rx->status = NCM_BUF_READY;
                rx->offset = 0;
                rx = rx->next;
            }
        } else if (rx->offset >= rx->length) {
            rx->status = NCM_BUF_READY;
            rx->offset = 0;
            rx = rx->next;
        }
    }
}

char *NCM_GetNextRxDatagramBuffer(short *length) {
    if (activeRxBuffer.ndp != 0 && (activeRxBuffer.datagramm == 0 || activeRxBuffer.datagramm->DatagramLength == 0 || activeRxBuffer.datagramm->DatagramOffset == 0)) {
        if (activeRxBuffer.ndp->NextNdpOffset == 0) {
            activeRxBuffer.ndp = 0;
            activeRxBuffer.datagramm = 0;
            activeRxBuffer.buffer->status = NCM_BUF_UNUSED;
        } else {
            activeRxBuffer.ndp = activeRxBuffer.buffer->buffer + activeRxBuffer.ndp->NextNdpOffset;
            activeRxBuffer.datagramm = activeRxBuffer.ndp + 1;
        }
    }

    if (activeRxBuffer.ndp == 0) {
        NCM_BufferInfo *temp = activeRxBuffer.buffer;
        do {
            activeRxBuffer.buffer = activeRxBuffer.buffer->next;
        } while (activeRxBuffer.buffer->status != NCM_BUF_READY && temp != activeRxBuffer.buffer);

        if (activeRxBuffer.buffer->status == NCM_BUF_READY) {
            activeRxBuffer.buffer->status = NCM_BUF_LOCKED;

            NCM_NTB_HEADER_16 *header = activeRxBuffer.buffer->buffer;
            activeRxBuffer.ndp = (char *)header + header->NdpOffset;
            activeRxBuffer.datagramm = activeRxBuffer.ndp + 1;
        }
    }

    if (activeRxBuffer.ndp) {
        NCM_NTB_HEADER_16 *header = activeRxBuffer.buffer->buffer;

        if (activeRxBuffer.ndp > activeRxBuffer.buffer->buffer + header->BlockLength) {
            // Broken ndp reference
            activeRxBuffer.ndp = 0;
            activeRxBuffer.datagramm = 0;
            *length = 0;
            return 0;
        }

        if (activeRxBuffer.ndp->Signature[0] != 'N' || activeRxBuffer.ndp->Signature[1] != 'C' || activeRxBuffer.ndp->Signature[2] != 'M') {
            // Broken link, skip this ndp
            activeRxBuffer.ndp = 0;
            activeRxBuffer.datagramm = 0;

            *length = 0;
            return 0;
        }

        NCM_NTB_DATAPOINTER_16 *datagramm = activeRxBuffer.datagramm;
        activeRxBuffer.datagramm += 1;

        if (datagramm->DatagramLength + datagramm->DatagramOffset > header->BlockLength) {
            // broken datagram
            activeRxBuffer.datagramm = 0;
            *length = 0;
            return 0;
        }

        *length = datagramm->DatagramLength;
        return activeRxBuffer.buffer->buffer + datagramm->DatagramOffset;
    } else {
        *length = 0;
        return 0;
    }
}

char *NCM_GetNextTxDatagramBuffer(short length) {
    // check size constraints
    char maxDatagrams = MIN(10, ntbInputSize.NtbInMaxDatagrams);
    if (maxDatagrams == 0) {
        maxDatagrams = 10;
    }

    short maxLength = ntbInputSize.NtbInMaxSize;

    if (activeTxBuffer.offset == 0) {
        activeTxBuffer.offset = sizeof(NCM_NTB_HEADER_16);
    }

    if (activeTxBuffer.offset + length + sizeof(NCM_NTB_POINTER_16) + (activeTxBuffer.datagramCount + 2) * sizeof(NCM_NTB_DATAPOINTER_16) > maxLength ||
        activeTxBuffer.datagramCount + 1 > maxDatagrams) {
        NCM_FlushTx();
    }

    // Record Datagram
    activeTxBuffer.datagrams[activeTxBuffer.datagramCount].DatagramLength = length;
    activeTxBuffer.datagrams[activeTxBuffer.datagramCount].DatagramOffset = activeTxBuffer.offset;
    activeTxBuffer.offset += length;

    return activeTxBuffer.buffer->buffer + activeTxBuffer.datagrams[activeTxBuffer.datagramCount++].DatagramOffset;
}

void NCM_FlushTx() {
    if (activeTxBuffer.datagramCount > 0 && activeTxBuffer.buffer->status == NCM_BUF_UNUSED) {
        unsigned short offset = (activeTxBuffer.offset + (4 - 1)) & -4;

        NCM_NTB_HEADER_16 *header = activeTxBuffer.buffer->buffer;
        NCM_NTB_POINTER_16 *ndp = activeTxBuffer.buffer->buffer + offset;

        header->NdpOffset = offset;
        header->HeaderLength = sizeof(NCM_NTB_HEADER_16);
        header->Sequence = activeTxBuffer.sequence;
        header->Signature[0] = 'N';
        header->Signature[1] = 'C';
        header->Signature[2] = 'M';
        header->Signature[3] = 'H';

        ndp->NextNdpOffset = 0;
        ndp->Length = sizeof(NCM_NTB_POINTER_16) + sizeof(NCM_NTB_DATAPOINTER_16) * (activeTxBuffer.datagramCount + 1);
        ndp->Signature[0] = 'N';
        ndp->Signature[1] = 'C';
        ndp->Signature[2] = 'M';
        ndp->Signature[3] = '0';

        offset += sizeof(NCM_NTB_POINTER_16);

        for (int i = 0; i < activeTxBuffer.datagramCount; i++) {
            NCM_NTB_DATAPOINTER_16 *datagram = (char *)header + offset;

            (*datagram) = activeTxBuffer.datagrams[i];
            offset += sizeof(NCM_NTB_DATAPOINTER_16);
        }

        NCM_NTB_DATAPOINTER_16 *terminator = (char *)header + offset;
        terminator->DatagramLength = 0;
        terminator->DatagramOffset = 0;
        offset += sizeof(NCM_NTB_DATAPOINTER_16);
        header->BlockLength = offset;
        activeTxBuffer.buffer->length = offset;

        activeTxBuffer.offset = sizeof(NCM_NTB_HEADER_16);
        activeTxBuffer.datagramCount = 0;
        activeTxBuffer.sequence++;
        activeTxBuffer.buffer->status = NCM_BUF_READY;
        activeTxBuffer.buffer->length = header->BlockLength;

        do {
            activeTxBuffer.buffer = activeTxBuffer.buffer->next;
        } while (activeTxBuffer.buffer->status == NCM_BUF_LOCKED);

        NCM_TransmitNextBuffer();
    }
}

void NCM_BufferTransmitted(char ep, short length) {
    NCM_TransmitNextBuffer();
}

static void NCM_TransmitNextBuffer() {
    if (tx->status == NCM_BUF_LOCKED) {
        tx->status = NCM_BUF_UNUSED;
    }

    NCM_BufferInfo *temp = tx;

    do {
        tx = tx->next;
    } while (tx->status != NCM_BUF_READY && tx != temp);

    if (tx->status == NCM_BUF_READY) {
        tx->status = NCM_BUF_LOCKED;

        USB_Transmit(2, tx->buffer, tx->length);
    }
}

// Control Transmissions for LinkUp & LinkDown
static const USB_NCM_SPEEDDATA nwSpeedChange = {
    .SetupPacket.RequestType = 0b10100001,
    .SetupPacket.Request = NCM_NETWORK_SPEEDCHANGE,
    .SetupPacket.Value = 0,
    .SetupPacket.Index = 1,
    .SetupPacket.Length = 8,
    .DlBitRate = 1000 * 1000,
    .UlBitRate = 1000 * 1000};
static const USB_SETUP_PACKET nwConnected = {
    .RequestType = 0b10100001,
    .Request = NCM_NETWORK_CONNECTION,
    .Value = 1,
    .Index = 1,
    .Length = 0};
static const USB_SETUP_PACKET nwDisconnected = {
    .RequestType = 0b10100001,
    .Request = NCM_NETWORK_CONNECTION,
    .Value = 0,
    .Index = 1,
    .Length = 0};

static const NCM_CtrlTxInfo LinkUp[2] = {
    {.buffer = &nwSpeedChange,
     .length = sizeof(nwSpeedChange),
     .next = &LinkUp[1]},
    {.buffer = &nwConnected,
     .length = sizeof(nwConnected),
     .next = 0}};

static const NCM_CtrlTxInfo LinkDown = {
    .buffer = &nwDisconnected,
    .length = sizeof(nwDisconnected),
    .next = 0};

static NCM_CtrlTxInfo *nextTransmission = {0};

static void NCM_ControlTransmission() {
    if (nextTransmission != 0) {
        USB_Transmit(1, nextTransmission->buffer, nextTransmission->length);
        nextTransmission = nextTransmission->next;
    }
}

void NCM_Reset(char interface, char alternateId) {
    if (interface == 1) {
        if (alternateId == 1) {
            NCM_LinkUp();
        } else if (alternateId == 0) {
            // Reset Network
            nextTransmission = 0;
            ntbInputSize.NtbInMaxDatagrams = 0;
            ntbInputSize.NtbInMaxSize = 2048;
        }
    }
}

void NCM_LinkUp() {
    nextTransmission = &LinkUp[0];
    NCM_ControlTransmission();
}

void NCM_LinkDown() {
    nextTransmission = &LinkDown;
    NCM_ControlTransmission();
}

void NCM_ControlTransmit(char ep, short length) {
    NCM_ControlTransmission();
}
