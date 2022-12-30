#include "main.h"
#include "lwip/autoip.h"
#include "lwip/init.h"
#include "lwip/netif.h"
#include "lwip/timeouts.h"
#include "ncm_device.h"
#include "ncm_netif.h"
#include "usb.h"

static struct netif ncm_if;

static void InitClock();
static void InitSystick();

#define AHBClockSpeed 72 * 1000 * 1000

static volatile uint32_t globalTime_ms = 0;

void SysTick_Handler() {
    globalTime_ms++;
}

unsigned int sys_jiffies() {
    return globalTime_ms;
}

unsigned int sys_now() {
    return globalTime_ms;
}

void delay_ms(unsigned int ms) {
    uint32_t time = globalTime_ms;

    while (globalTime_ms - time < ms) {
    }
}

void *memcpy(void *destination, const void *source, size_t num) {
    if ((uintptr_t)destination % sizeof(long) == 0 &&
        (uintptr_t)source % sizeof(long) == 0 &&
        num % sizeof(long) == 0) {

        long *lsrc = (long *)source;
        long *ldst = (long *)destination;

        for (int i = 0; i < num / sizeof(long); i++) {
            ldst[i] = lsrc[i];
        }
    } else {
        char *csrc = (char *)source;
        char *cdst = (char *)destination;

        for (int i = 0; i < num; i++) {
            cdst[i] = csrc[i];
        }
    }
}

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void) {
    InitClock();
    InitSystick();
    USB_Init();
    lwip_init();

    netif_add(&ncm_if, IP4_ADDR_ANY, IP4_ADDR_ANY, IP4_ADDR_ANY, NULL, ncm_netif_init, netif_input);
    netif_set_default(&ncm_if);
    netif_set_up(&ncm_if);

    autoip_start(&ncm_if);

    while (1) {
        ncm_netif_poll(&ncm_if);
        sys_check_timeouts();

        if (globalTime_ms % 500 == 0) {
            NCM_FlushTx();
        }
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

static void InitSystick() {
    unsigned int loadVal = SystemCoreClock / 1000 / 8;

    if (loadVal == 0) {
        loadVal = SystemCoreClock / 1000;
        SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk;
    } else {
        SysTick->CTRL &= ~SysTick_CTRL_CLKSOURCE_Msk;
    }

    SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;
    SysTick->LOAD = loadVal;
    SysTick->VAL = 0;

    NVIC_SetPriority(SysTick_IRQn, 0);
    NVIC_EnableIRQ(SysTick_IRQn);

    SysTick->CTRL |= 1;
}
