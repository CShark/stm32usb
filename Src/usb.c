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
    unsigned char RequestType;
    unsigned char Request;
    union {
        unsigned short Value;
        struct {
            unsigned char DescriptorIndex;
            unsigned char DescriptorType;
        };
    };
    unsigned short Index;
    unsigned short Length;
} USB_SETUP_PACKET;

typedef struct {
    unsigned short Length;
    unsigned short BytesSent;
    unsigned char *Buffer;
} USB_TRANSFER_STATE;

typedef struct {
    USB_SETUP_PACKET Setup;
    USB_TRANSFER_STATE Transfer;
} USB_CONTROL_STATE;

__ALIGNED(8)
__USB_MEM
__IO static USB_BTABLE_ENTRY BTable[8] = {0};

__ALIGNED(2)
__USB_MEM
__IO static char EP0_Buf[2][64] = {0};

static USB_CONTROL_STATE ControlState;
static char ActiveConfiguration = 0x00;
static char DeviceState = 0x00; // 0 - Default, 1 - Address, 2 - Configured
static char EndpointState[USB_NumEndpoints] = {0};

static void USB_CopyMemory(volatile short *source, volatile short *target, short length);
static void USB_ClearSRAM();
static void USB_SetEP(short *ep, short value, short mask);
static void USB_HandleControl();
static void USB_HandleSetup(USB_SETUP_PACKET *setup);
static void USB_PrepareTransfer(USB_TRANSFER_STATE *transfer, short *ep, char *txBuffer, short *txBufferCount, short txBufferSize);

void USB_Init() {
    // Initialize the NVIC
    NVIC_SetPriority(USB_LP_IRQn, 0);
    NVIC_EnableIRQ(USB_LP_IRQn);

    // Enable USB macrocell
    USB->CNTR &= ~USB_CNTR_PDWN;

    // Wait 1μs until clock is stable
    SysTick->LOAD = 100;
    SysTick->VAL = 0;
    SysTick->CTRL = 1;
    while ((SysTick->CTRL & SysTick_CTRL_COUNTFLAG_Msk) == 0) {
    }
    SysTick->CTRL = 0;

    // Enable all interrupts & the internal pullup to put 1.5K on D+ for FullSpeed USB
    USB->CNTR |= USB_CNTR_RESETM | USB_CNTR_CTRM | USB_CNTR_WKUPM | USB_CNTR_SUSPM;
    USB->BCDR |= USB_BCDR_DPPU;

    // Clear the USB Reset (D+ & D- low) to start enumeration
    USB->CNTR &= ~USB_CNTR_FRES;
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

        // Prepare for a setup packet (RX = Valid, TX = NAK)
        USB_SetEP(&USB->EP0R, USB_EP_CONTROL | USB_EP_RX_VALID | USB_EP_TX_NAK, USB_EP_TYPE_MASK | USB_EP_RX_VALID | USB_EP_TX_VALID);

        // Enable USB functionality and set address to 0
        DeviceState = 0;
        USB->DADDR = USB_DADDR_EF;
    } else if ((USB->ISTR & USB_ISTR_CTR) != 0) {
        if ((USB->ISTR & USB_ISTR_EP_ID) == 0) {
            USB_HandleControl();
        }
    } else if((USB->ISTR & USB_ISTR_SUSP) != 0){
    	USB->ISTR = ~USB_ISTR_SUSP;
        USB_SuspendDevice();
        
        // On Suspend, the device should enter low power mode and turn off the USB-Peripheral
        USB->CNTR |= USB_CNTR_FSUSP;

        // If the device still needs power from the USB Host
        USB->CNTR |= USB_CNTR_LPMODE;        
    } else if((USB->ISTR & USB_CLR_WKUP) != 0) {
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
        target[length - 1] = source[length - 1];
    }
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
            USB_SETUP_PACKET *setup = EP0_Buf[0];
            USB_HandleSetup(setup);
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
    USB_CopyMemory(setup, &ControlState.Setup, sizeof(USB_SETUP_PACKET));
    ControlState.Transfer.Length = 0;

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
            }
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
                EP0_Buf[1][0] = 0x00;
                EP0_Buf[1][1] = 0x00;
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
        case 0x11: // Set Interface
            if (DeviceState == 2 && setup->Index < USB_NumInterfaces && EP0_Buf[0][0] == 0x00) {
                BTable[0].COUNT_TX = 0;
                USB_SetEP(&USB->EP0R, USB_EP_TX_VALID, USB_EP_TX_VALID);
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
                    EP0_Buf[1][0] = EndpointState[setup->Index];
                    EP0_Buf[1][1] = 0x00;
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
        case 0x12: // Sync Frame
            USB_SetEP(&USB->EP0R, USB_EP_TX_STALL, USB_EP_TX_VALID);
            break;
        }
    }
}

static void USB_PrepareTransfer(USB_TRANSFER_STATE *transfer, short *ep, char *txBuffer, short *txBufferCount, short txBufferSize) {
    *txBufferCount = MIN(txBufferSize, transfer->Length - transfer->BytesSent);
    if (*txBufferCount > 0) {
        USB_CopyMemory(transfer->Buffer + transfer->BytesSent, txBuffer, *txBufferCount);
        transfer->BytesSent += *txBufferCount;
        USB_SetEP(ep, USB_EP_TX_VALID, USB_EP_TX_VALID);
    } else {
        USB_SetEP(ep, USB_EP_TX_NAK, USB_EP_TX_VALID);
    }
}
