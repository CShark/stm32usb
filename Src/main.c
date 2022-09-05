#include "main.h"

static void InitClock();

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
    InitClock();

    while (1) {
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
}