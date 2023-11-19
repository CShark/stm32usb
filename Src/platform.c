#include "platform.h"

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

void Systick_Init() {
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
