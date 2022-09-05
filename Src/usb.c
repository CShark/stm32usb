#include "usb.h"
#include "stm32g441xx.h"

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

}