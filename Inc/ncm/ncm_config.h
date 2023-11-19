#ifndef __NCM_CONFIG_H__
#define __NCM_CONFIG_H__

#include "usb.h"
#include "platform.h"

void NCM_Init();
void NCM_Loop();
USB_Implementation NCM_GetImplementation();

#endif