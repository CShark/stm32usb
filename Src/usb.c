#include "usb.h"
#include "stm32g441xx.h"
#include "usb_config.h"

#define __USB_MEM __attribute__((section(".usbbuf")))
#define __USBBUF_BEGIN 0x40006000
#define __MEM2USB(X) (((int)X - __USBBUF_BEGIN))
#define __USB2MEM(X) (((int)X + __USBBUF_BEGIN))
#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))

typedef struct {
    unsigned short ADDR_TX;
    unsigned short COUNT_TX;
    unsigned short ADDR_RX;
    unsigned short COUNT_RX;
} USB_BTABLE_ENTRY;

typedef struct {
    unsigned short Length;
    unsigned short BytesSent;
    unsigned char *Buffer;
} USB_TRANSFER_STATE;

typedef struct {
    USB_SETUP_PACKET Setup;
    USB_TRANSFER_STATE Transfer;
    USB_TRANSFER_STATE Receive;
} USB_CONTROL_STATE;

typedef struct {
    char *Buffer;
    char Size;
    void (*CompleteCallback)(char ep, short length);
} USB_BufferConfig;

__ALIGNED(8)
__USB_MEM
__IO static USB_BTABLE_ENTRY BTable[8] = {0};

__ALIGNED(2)
__USB_MEM
__IO static char EP0_Buf[2][64] = {0};

static USB_BufferConfig Buffers[16] = {0};

static USB_CONTROL_STATE ControlState;
static USB_TRANSFER_STATE Transfers[7] = {0};
static char ActiveConfiguration = 0x00;
static char DeviceState = 0x00; // 0 - Default, 1 - Address, 2 - Configured
static char EndpointState[USB_NumEndpoints] = {0};

static char ControlDataBuffer[USB_MaxControlData] = {0};

/// @brief Copy data from / to USB-SRAM
static void USB_CopyMemory(volatile short *source, volatile short *target, short length);
/// @brief Clear the USB-SRAM to 0
static void USB_ClearSRAM();
/// @brief Set an EPn-Register
/// @param ep Pointer to the EPnR to edit
/// @param value The values to set for this register
/// @param mask A mask of bits which should be changed
static void USB_SetEP(short *ep, short value, short mask);
/// @brief Called to handle EP0-messages
static void USB_HandleControl();
/// @brief Called to handle Setup-Packets on EP0
static void USB_HandleSetup(USB_SETUP_PACKET *setup);
/// @brief Prepare a transfer on an endpoint
/// @param transfer A pointer to the transfer metadata
/// @param ep The endpoint to send from
/// @param txBuffer The TX-Buffer to copy the transmission to
/// @param txBufferCount The Register that should contain the number of bytes to send
/// @param txBufferSize The size of the TX-Buffer
static void USB_PrepareTransfer(USB_TRANSFER_STATE *transfer, short *ep, char *txBuffer, short *txBufferCount, short txBufferSize);

void delay_ms(unsigned int ms);

void USB_Init() {
    // Initialize the NVIC
    NVIC_SetPriority(USB_LP_IRQn, 8);
    NVIC_EnableIRQ(USB_LP_IRQn);
    NVIC_SetPriority(USB_HP_IRQn, 8);
    NVIC_EnableIRQ(USB_HP_IRQn);

    ControlState.Receive.Buffer = ControlDataBuffer;

    // Enable USB macrocell
    USB->CNTR &= ~USB_CNTR_PDWN;

    // Wait 1μs until clock is stable
    delay_ms(1);

    // Enable all interrupts & the internal pullup to put 1.5K on D+ for FullSpeed USB
    USB->CNTR |= USB_CNTR_RESETM | USB_CNTR_CTRM | USB_CNTR_WKUPM | USB_CNTR_SUSPM;
    USB->BCDR |= USB_BCDR_DPPU;

    // Clear the USB Reset (D+ & D- low) to start enumeration
    USB->CNTR &= ~USB_CNTR_FRES;
}

void USB_HP_IRQHandler() {
    // Only take care of regular transmissions
    if ((USB->ISTR & USB_ISTR_CTR) != 0) {
        char ep = USB->ISTR & USB_ISTR_EP_ID;

        if (ep > 0 && ep < 8) {
            // On RX, call the registered callback if available
            if ((*(&USB->EP0R + ep * 2) & USB_EP_CTR_RX) != 0) {
                if (Buffers[ep * 2].CompleteCallback != 0) {
                    Buffers[ep * 2].CompleteCallback(ep, BTable[ep].COUNT_RX & 0x01FF);
                }

                USB_SetEP(&USB->EP0R + ep * 2, USB_EP_RX_VALID, USB_EP_CTR_RX | USB_EP_RX_VALID);
            }

            // On TX, check if there is some remaining data to be sent in the pending Transfers
            if ((*(&USB->EP0R + ep * 2) & USB_EP_CTR_TX) != 0) {
                if (Transfers[ep - 1].Length > 0) {
                    if (Transfers[ep - 1].Length > Transfers[ep - 1].BytesSent) {
                        USB_PrepareTransfer(&Transfers[ep - 1], &USB->EP0R + ep * 2, Buffers[ep * 2 + 1].Buffer, &BTable[ep].COUNT_TX, Buffers[ep * 2 + 1].Size);
                    } else if (Transfers[ep - 1].Length == Transfers[ep - 1].BytesSent) {
                        char length = Transfers[ep - 1].Length;
                        Transfers[ep - 1].Length = 0;

                        if (Buffers[ep * 2 + 1].CompleteCallback != 0) {
                            Buffers[ep * 2 + 1].CompleteCallback(ep, length);
                        }

                        // if complete and no new TX, add one empty packet to flush queue, send tx complete signal
                        if (Transfers[ep - 1].Length == 0) {
                            BTable[ep].COUNT_TX = 0;
                            USB_SetEP(&USB->EP0R + ep * 2, USB_EP_TX_VALID, USB_EP_TX_VALID);
                        }
                    }
                }

                USB_SetEP(&USB->EP0R + ep * 2, 0x00, USB_EP_CTR_TX);
            }
        }
    }
}

void USB_LP_IRQHandler() {
    if ((USB->ISTR & USB_ISTR_RESET) != 0) {
        // Clear interrupt
        USB->ISTR = ~USB_ISTR_RESET;

        // Clear SRAM for readability
        USB_ClearSRAM();

        // Prepare BTable
        USB->BTABLE = __MEM2USB(BTable);

        BTable[0].ADDR_RX = __MEM2USB(EP0_Buf[0]);
        BTable[0].ADDR_TX = __MEM2USB(EP0_Buf[1]);
        BTable[0].COUNT_TX = 0;
        BTable[0].COUNT_RX = (1 << 15) | (1 << 10);

        Buffers[0].Buffer = EP0_Buf[0];
        Buffers[0].Size = 64;
        Buffers[1].Buffer = EP0_Buf[1];
        Buffers[1].Size = 64;

        // Prepare for a setup packet (RX = Valid, TX = NAK)
        USB_SetEP(&USB->EP0R, USB_EP_CONTROL | USB_EP_RX_VALID | USB_EP_TX_NAK, USB_EP_TYPE_MASK | USB_EP_RX_VALID | USB_EP_TX_VALID);

        USB_ConfigureEndpoints();

        // Enable USB functionality and set address to 0
        DeviceState = 0;
        USB->DADDR = USB_DADDR_EF;
    } else if ((USB->ISTR & USB_ISTR_CTR) != 0) {
        // Route EP0 to the control handler, everything else to the HP handler
        if ((USB->ISTR & USB_ISTR_EP_ID) == 0) {
            USB_HandleControl();
        } else {
            USB_HP_IRQHandler();
        }
    } else if ((USB->ISTR & USB_ISTR_SUSP) != 0) {
        USB->ISTR = ~USB_ISTR_SUSP;
        USB_SuspendDevice();

        // On Suspend, the device should enter low power mode and turn off the USB-Peripheral
        USB->CNTR |= USB_CNTR_FSUSP;

        // If the device still needs power from the USB Host
        USB->CNTR |= USB_CNTR_LPMODE;
    } else if ((USB->ISTR & USB_CLR_WKUP) != 0) {
        USB->ISTR = ~USB_ISTR_WKUP;

        // Resume peripheral
        USB->CNTR &= ~(USB_CNTR_FSUSP | USB_CNTR_LPMODE);
        USB_WakeupDevice();
    }
}

static void USB_CopyMemory(volatile short *source, volatile short *target, short length) {
    for (int i = 0; i < length / 2; i++) {
        target[i] = source[i];
    }

    if (length % 2 == 1) {
        ((char *)target)[length - 1] = ((char *)source)[length - 1];
    }
}

static void USB_WriteChars(char byte0, char byte1, volatile short *target) {
    target[0] = byte0 << 0xFF | byte1;
}

static void USB_ClearSRAM() {
    char *buffer = __USBBUF_BEGIN;

    for (int i = 0; i < 1024; i++) {
        buffer[i] = 0;
    }
}

static void USB_SetEP(short *ep, short value, short mask) {
    short toggle = 0b0111000001110000;
    short rc_w0 = 0b1000000010000000;
    short rw = 0b0000011100001111;

    short wr0 = rc_w0 & (~mask | value);
    short wr1 = (mask & toggle) & (*ep ^ value);
    short wr2 = rw & ((*ep & ~mask) | value);

    *ep = wr0 | wr1 | wr2;
}

static void USB_HandleControl() {
    if (USB->EP0R & USB_EP_CTR_RX) {
        // We received a control message
        if (USB->EP0R & USB_EP_SETUP) {
            // On Setup, ditch all running receptions and start anew
            USB_SETUP_PACKET *setup = EP0_Buf[0];
            USB_CopyMemory(setup, &ControlState.Setup, sizeof(USB_SETUP_PACKET));
            ControlState.Transfer.Length = 0;
            ControlState.Receive.Length = 0;

            // If this is an OUT Transfer and we expect data, postpone handling the setup until the data arrives
            if ((setup->RequestType & 0x80) == 0 && setup->Length > 0) {
                ControlState.Receive.Length = setup->Length;
                ControlState.Receive.BytesSent = 0;
            } else {
                USB_HandleSetup(&ControlState.Setup);
            }
        } else {
            // Check if we are expecting data for a setup-packet. If so, read it and call the Setup-Handler once the transfer is complete
            if (ControlState.Receive.Length > 0) {
                if (ControlState.Receive.BytesSent < USB_MaxControlData) {
                    USB_CopyMemory(EP0_Buf[0], ControlState.Receive.Buffer + ControlState.Receive.BytesSent, MIN(USB_MaxControlData - ControlState.Receive.BytesSent, BTable[0].COUNT_RX & 0x1FF));
                    ControlState.Receive.BytesSent += MIN(USB_MaxControlData - ControlState.Receive.BytesSent, BTable[0].COUNT_RX & 0x1FF);
                }

                if (ControlState.Receive.BytesSent >= ControlState.Receive.Length) {
                    USB_HandleSetup(&ControlState.Setup);
                    ControlState.Receive.Length = 0;
                } else if (ControlState.Receive.BytesSent >= USB_MaxControlData) {
                    USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                    ControlState.Receive.Length = 0;
                }
            }
        }

        USB_SetEP(&USB->EP0R, USB_EP_RX_VALID, USB_EP_CTR_RX | USB_EP_RX_VALID);
    }

    if (USB->EP0R & USB_EP_CTR_TX) {
        // We just sent a control message
        if (ControlState.Setup.Request == 0x05) {
            USB->DADDR = USB_DADDR_EF | ControlState.Setup.Value;
        }

        // check for running transfers
        if (ControlState.Transfer.Length > 0) {
            if (ControlState.Transfer.Length > ControlState.Transfer.BytesSent) {
                USB_PrepareTransfer(&ControlState.Transfer, &USB->EP0R, &EP0_Buf[1], &BTable[0].COUNT_TX, 64);
            }
        }

        USB_SetEP(&USB->EP0R, 0x00, USB_EP_CTR_TX);
    }
}

static void USB_HandleSetup(USB_SETUP_PACKET *setup) {
    if ((setup->RequestType & 0x60) != 0) {// || (setup->RequestType & 0x1F) != 0) {
        // Class and interface setup packets are redirected to the class specific implementation
        char ret = USB_HandleClassSetup(setup, ControlState.Receive.Buffer, ControlState.Receive.Length);

        if ((setup->RequestType & 0x80) == 0) {
            if (ret == USB_OK) {
                BTable[0].COUNT_TX = 0;
                USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
            } else if (ret == USB_BUSY) {
                USB_SetEP(&USB->EP0R, USB_EP_TX_NAK, USB_EP_TX_VALID);
            } else {
                USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
            }
        }
    } else if ((setup->RequestType & 0x60) == 0) {
        if ((setup->RequestType & 0x0F) == 0) { // Device Requests
            switch (setup->Request) {
            case 0x00: // Get Status
                EP0_Buf[1][0] = USB_SelfPowered;
                EP0_Buf[1][1] = 0x00;
                BTable[0].COUNT_TX = 2;
                USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                break;
            case 0x01: // Clear Feature
                USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                break;
            case 0x03: // Set Feature
                BTable[0].COUNT_TX = 0;
                USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                break;
            case 0x05: // Set Address
                if (DeviceState == 0 || DeviceState == 1) {
                    BTable[0].COUNT_TX = 0;
                    USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                    DeviceState = 1;
                } else {
                    USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                }
                break;
            case 0x06: // Get Descriptor
                switch (setup->DescriptorType) {
                case 0x01: { // Device Descriptor
                    USB_DESCRIPTOR_DEVICE *descriptor = USB_GetDeviceDescriptor();
                    USB_CopyMemory(descriptor, EP0_Buf[1], sizeof(USB_DESCRIPTOR_DEVICE));
                    BTable[0].COUNT_TX = sizeof(USB_DESCRIPTOR_DEVICE);

                    USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                } break;
                case 0x02: { // Configuration Descriptor
                    short length;
                    char *descriptor = USB_GetConfigDescriptor(&length);
                    ControlState.Transfer.Buffer = descriptor;
                    ControlState.Transfer.BytesSent = 0;
                    ControlState.Transfer.Length = MIN(length, setup->Length);

                    USB_PrepareTransfer(&ControlState.Transfer, &USB->EP0R, &EP0_Buf[1], &BTable[0].COUNT_TX, 64);
                } break;
                case 0x03:                             // String Descriptor
                    if (setup->DescriptorIndex == 0) { // Get supported Languages
                        USB_DESCRIPTOR_STRINGS data = {
                            .Length = 4,
                            .Type = 0x03,
                        };
                        // Don't use prepare transfer, as the variable will become invalid once we leave this block
                        USB_CopyMemory(&data, EP0_Buf[1], 2);
                        *((short *)(&EP0_Buf[1][2])) = 0x0409;

                        BTable[0].COUNT_TX = 4;
                        USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                    } else {
                        short length = 0;
                        char *data = 0;

                        if (setup->DescriptorIndex == 0xEE) { // Microsoft OS Descriptor
                            data = USB_GetOSDescriptor(&length);
                        } else {
                            data = USB_GetString(setup->DescriptorIndex, setup->Index, &length);
                        }

                        short txLength = length + 2;
                        txLength = MIN(length, setup->Length);

                        USB_DESCRIPTOR_STRINGS header = {
                            .Length = length,
                            .Type = 0x03};

                        USB_CopyMemory(data, EP0_Buf[1] + 2, txLength - 2);

                        USB_CopyMemory(&header, EP0_Buf[1], 2);

                        BTable->COUNT_TX = txLength;
                        USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                    }
                    break;
                }
                break;
            case 0x07: // Set Descriptor
                // Allows the Host to alter the descriptor. Not supported
                USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                break;
            case 0x08: // Get Configuration
                if (DeviceState == 1 || DeviceState == 2) {
                    if (DeviceState == 1) {
                        ActiveConfiguration = 0;
                    }

                    EP0_Buf[1][0] = ActiveConfiguration;
                    BTable[0].COUNT_TX = 1;
                    USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                } else {
                    USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                }
                break;
            case 0x09: // Set Configuration
                if (DeviceState == 1 || DeviceState == 2) {
                    BTable[0].COUNT_TX = 0;
                    switch (setup->Value & 0xFF) {
                    case 0:
                        DeviceState = 1;
                        ActiveConfiguration = 0;
                        USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                        break;
                    case 1:
                        DeviceState = 2;
                        ActiveConfiguration = 1;
                        USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                        break;
                    default:
                        USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                        break;
                    }

                    if (DeviceState == 2) {
                        USB_SetEP(&USB->EP1R, 0x00, USB_EP_DTOG_RX | USB_EP_DTOG_TX);
                        USB_SetEP(&USB->EP2R, 0x00, USB_EP_DTOG_RX | USB_EP_DTOG_TX);
                        USB_SetEP(&USB->EP3R, 0x00, USB_EP_DTOG_RX | USB_EP_DTOG_TX);
                        USB_SetEP(&USB->EP4R, 0x00, USB_EP_DTOG_RX | USB_EP_DTOG_TX);
                        USB_SetEP(&USB->EP5R, 0x00, USB_EP_DTOG_RX | USB_EP_DTOG_TX);
                        USB_SetEP(&USB->EP6R, 0x00, USB_EP_DTOG_RX | USB_EP_DTOG_TX);
                        USB_SetEP(&USB->EP7R, 0x00, USB_EP_DTOG_RX | USB_EP_DTOG_TX);
                    }
                } else {
                    USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                }
                break;
            }
        } else if ((setup->RequestType & 0x0F) == 0x01) { // Interface requests
            switch (setup->Request) {
            case 0x00: // Get Status
                if (DeviceState == 2) {
                    USB_WriteChars(0x00, 0x00, EP0_Buf[1]);
                    BTable[0].COUNT_TX = 2;
                    USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                } else {
                    USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                }
                break;
            case 0x01: // Clear Feature
                USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                break;
            case 0x03: // Set Feature
                USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                break;
            case 0x0A: // Get Interface
                if (DeviceState == 2 && setup->Index < USB_NumInterfaces) {
                    EP0_Buf[1][0] = 0x00;
                    BTable[0].COUNT_TX = 1;
                    USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                } else {
                    USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                }
                break;
            case 0x0B: // Set Interface
                if (DeviceState == 2 && setup->Index < USB_NumInterfaces) {
                    BTable[0].COUNT_TX = 0;
                    USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                    USB_ResetClass(setup->Index, setup->Value);
                } else {
                    USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                }
                break;
            }
        } else if ((setup->RequestType & 0x0F) == 0x02) { // Endpoint requests
            switch (setup->Request) {
            case 0x00: // Get Status
                if ((DeviceState == 2 || (DeviceState == 1 && setup->Index == 0x00)) && setup->Index < USB_NumEndpoints) {
                    if (setup->Value == 0x00) {
                        USB_WriteChars(EndpointState[setup->Index], 0x00, EP0_Buf[1]);
                        BTable[0].COUNT_TX = 2;
                        USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                    } else {
                        USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                    }
                } else {
                    USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                }
                break;
            case 0x01: // Clear Feature
                if ((DeviceState == 2 || (DeviceState == 1 && setup->Index == 0x00)) && setup->Index < USB_NumEndpoints) {
                    if (setup->Value == 0x00) {
                        EndpointState[setup->Index] = 0;
                        BTable[0].COUNT_TX = 0;
                        USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                    } else {
                        USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                    }
                } else {
                    USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                }
                break;
            case 0x03: // Set Feature
                if ((DeviceState == 2 || (DeviceState == 1 && setup->Index == 0x00)) && setup->Index < USB_NumEndpoints) {
                    if (setup->Value == 0x00) {
                        EndpointState[setup->Index] = 1;
                        BTable[0].COUNT_TX = 0;
                        USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
                    } else {
                        USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                    }
                } else {
                    USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                }
                break;
            case 0x0C: // Sync Frame
                USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
                break;
            }
        }
    }
}

static void USB_PrepareTransfer(USB_TRANSFER_STATE *transfer, short *ep, char *txBuffer, short *txBufferCount, short txBufferSize) {
    // Check if there is still data to transmit and if so transmit the next chunk of data
    *txBufferCount = MIN(txBufferSize, transfer->Length - transfer->BytesSent);
    if (*txBufferCount > 0) {
        USB_CopyMemory(transfer->Buffer + transfer->BytesSent, txBuffer, *txBufferCount);
        transfer->BytesSent += *txBufferCount;
        USB_SetEP(ep, USB_EP_TX_VALID, USB_EP_TX_VALID);
    } else {
        USB_SetEP(ep, USB_EP_TX_NAK, USB_EP_TX_VALID);
    }
}

void USB_Transmit(char ep, char *buffer, short length) {
    // Prepare the transfer metadata and initiate the chunked transfer
    if (ep == 0) {
        ControlState.Transfer.Buffer = buffer;
        ControlState.Transfer.BytesSent = 0;
        ControlState.Transfer.Length = length;
        USB_PrepareTransfer(&ControlState.Transfer, &USB->EP0R, EP0_Buf[1], &BTable[0].COUNT_TX, 64);
    } else if (ep < 8) {
        Transfers[ep - 1].Buffer = buffer;
        Transfers[ep - 1].Length = length;
        Transfers[ep - 1].BytesSent = 0;
        USB_PrepareTransfer(&Transfers[ep - 1], (&USB->EP0R) + ep * 2, Buffers[ep * 2 + 1].Buffer, &BTable[ep].COUNT_TX, Buffers[ep * 2 + 1].Size);
    }
}

char USB_IsTransmitPending(char ep) {
    if (ep == 0) {
        return ControlState.Transfer.Length > 0;
    } else {
        return Transfers[ep - 1].Length > 0;
    }
}

void USB_Fetch(char ep, char *buffer, short *length) {
    // Read data from the RX Buffer
    if (ep >= 0 && ep < 8) {
        short rxcount = BTable[ep].COUNT_RX & 0x1FF;
        *length = MIN(rxcount, *length);

        USB_CopyMemory(Buffers[ep * 2].Buffer, buffer, *length);
    }
}

static void USB_DistributeBuffers() {
    // This function will organize the USB-SRAM and assign RX- and TX-Buffers
    int addr = __USBBUF_BEGIN + sizeof(BTable);
    for (int i = 0; i < 16; i++) {
        if (Buffers[i].Size > 0) {
            Buffers[i].Buffer = addr;
            addr += Buffers[i].Size;

            if (addr & 0x01)
                addr++;
        } else {
            Buffers[i].Buffer = 0x00;
        }
    }
}

void USB_SetEPConfig(USB_CONFIG_EP config) {
    if (config.EP > 0 && config.EP < 8) {
        unsigned char rxSize = config.RxBufferSize;
        unsigned char txSize = config.TxBufferSize;

        if (rxSize & 0x01)
            rxSize++;
        if (txSize & 0x01)
            txSize++;

        Buffers[config.EP * 2].Size = config.RxBufferSize;
        Buffers[config.EP * 2 + 1].Size = config.TxBufferSize;
        Buffers[config.EP * 2].CompleteCallback = config.RxCallback;
        Buffers[config.EP * 2 + 1].CompleteCallback = config.TxCallback;
        USB_DistributeBuffers();

        if (rxSize > 0) {
            BTable[config.EP].ADDR_RX = __MEM2USB(Buffers[config.EP * 2].Buffer);
        }
        if (txSize > 0) {
            BTable[config.EP].ADDR_TX = __MEM2USB(Buffers[config.EP * 2 + 1].Buffer);
        }

        BTable[config.EP].COUNT_TX = 0;
        if (rxSize < 64) {
            BTable[config.EP].COUNT_RX = (rxSize / 2) << 10;
        } else {
            BTable[config.EP].COUNT_RX = (1 << 15) | (((rxSize / 32) - 1) << 10);
        }

        // only allow to set ep type & kind
        short epConfig = config.Type & 0x0700;
        epConfig |= USB_EP_TX_NAK;
        epConfig |= config.EP;
        if (rxSize > 0) {
            epConfig |= USB_EP_RX_VALID;
        }

        USB_SetEP((&USB->EP0R) + 2 * config.EP, epConfig, USB_EP_DTOG_RX | USB_EP_RX_VALID | USB_EP_TYPE_MASK | USB_EP_KIND | USB_EP_DTOG_TX | USB_EP_TX_VALID | 0x000F);
    }
}
