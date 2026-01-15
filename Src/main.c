#include "usb.h"

#include "cdc/cdc_config.h"
#include "hid/hid_config.h"
#include "ncm/ncm_config.h"

static void InitClock();

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
    InitClock();
    Systick_Init();

    USB_Implementation cdc = CDC_GetImplementation();
    USB_Implementation hid = HID_GetImplementation();
    /* too large for stm32f0xx implementation
    USB_Implementation ncm = NCM_GetImplementation();
    NCM_Init();
    USB_Init(ncm);
    */
    USB_Init(cdc);

    while (1) {
    	/* too large for stm32f0xx implementation
        NCM_Loop();
        */
    }
}

static void InitClock() {

#if defined(STM32G441xx) || defined(STM32G474xx)

    RCC->CR |= RCC_CR_HSEON;

    // Configure PLL (R=143.75, Q=47.92)
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY) {
    }
    RCC->PLLCFGR |= RCC_PLLCFGR_PLLSRC_HSE | RCC_PLLCFGR_PLLM_0 | (23 << RCC_PLLCFGR_PLLN_Pos) | RCC_PLLCFGR_PLLQ_1;
    RCC->PLLCFGR |= RCC_PLLCFGR_PLLREN | RCC_PLLCFGR_PLLQEN;
    RCC->CR |= RCC_CR_PLLON;

    // Select PLL as main clock, AHB/2
    RCC->CFGR |= RCC_CFGR_HPRE_3 | RCC_CFGR_SW_PLL;

    // Select USB Clock as PLLQ
    RCC->CCIPR = RCC_CCIPR_CLK48SEL_1;

    // Enable IO Clock for USB & Port
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    RCC->APB1ENR1 |= RCC_APB1ENR1_USBEN;

    while ((RCC->CFGR & RCC_CFGR_SWS_Msk) != RCC_CFGR_SWS_PLL) {
    }

#elif defined(STM32F042x6)
    // enable HSI48
    RCC->CR2 |= RCC_CR2_HSI48ON;
    while (!(RCC->CR2 & RCC_CR2_HSI48RDY)) {};

    // enable CRS for USB-sync
    RCC->APB1ENR |= RCC_APB1ENR_CRSEN;
    // reset CRS config
    CRS->CR = 0;
    CRS->CFGR = 0;
    // automatic trimming
    CRS->CFGR |= CRS_CFGR_SYNCSRC_1;    // USB SOF
    CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;

    // select HSI48 as USB clock source
    RCC->CFGR3 &= ~RCC_CFGR3_USBSW;
    RCC->CFGR3 |= RCC_CFGR3_USBSW_HSI48;

    // enable GPIOA and USB clocks
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_USBEN;

#else
#error "Unsupported MCU"
#endif

    SystemCoreClockUpdate();
}
