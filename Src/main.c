#include "main.h"
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
    USB_Implementation ncm = NCM_GetImplementation();
    NCM_Init();
    USB_Init(ncm);

    while (1) {
        NCM_Loop();
    }
}

static void InitClock() {
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

    SystemCoreClockUpdate();
}
