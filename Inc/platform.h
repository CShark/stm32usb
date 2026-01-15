#ifndef __PLATFORM_H_
#define __PLATFORM_H_

#include "stm32.h"
#include <stddef.h>

unsigned int sys_jiffies();
unsigned int sys_now();
void delay_ms(unsigned int ms);

void Systick_Init();

#endif
