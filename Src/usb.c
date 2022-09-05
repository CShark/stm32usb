#include "usb.h"
#include "stm32g441xx.h"

#define __USB_MEM __attribute__((section(".usbbuf")))
#define __USBBUF_BEGIN 0x40006000
#define __MEM2USB(X) (((int)X - __USBBUF_BEGIN))
#define __USB2MEM(X) (((int)X + __USBBUF_BEGIN))

typedef struct {
    unsigned short ADDR_TX;
    unsigned short COUNT_TX;
    unsigned short ADDR_RX;
    unsigned short COUNT_RX;
} USB_BTABLE_ENTRY;

__ALIGNED(8)
__USB_MEM
__IO static USB_BTABLE_ENTRY BTable[8] = {0};

__ALIGNED(2)
__USB_MEM
__IO static char EP0_Buf[2][64] = {0};

static void USB_ClearSRAM();
static void USB_SetEP(short *ep, short value, short mask);

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
    USB->CNTR |= USB_CNTR_RESETM | USB_CNTR_CTRM | USB_CNTR_WKUPM | USB_CNTR_SUSPM | USB_CNTR_ESOFM | USB_CNTR_ERRM | USB_CNTR_L1REQM | USB_CNTR_SOFM;
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
        USB->DADDR = USB_DADDR_EF;
    } else if ((USB->ISTR & USB_ISTR_CTR) != 0) {
        __BKPT();
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