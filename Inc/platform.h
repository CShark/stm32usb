#ifndef __PLATFORM_H_
#define __PLATFORM_H_

/* #define STM32G441xx */
#define STM32F042x6

#ifdef STM32G441xx
#include "stm32g4xx.h"
#elif defined(STM32F042x6)
#include "stm32f0xx.h"
#endif

#include <stddef.h>

unsigned int sys_jiffies();
unsigned int sys_now();
void delay_ms(unsigned int ms);

void Systick_Init();

#endif
