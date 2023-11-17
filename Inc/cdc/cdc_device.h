#ifndef __CDC_DEVICE_H
#define __CDC_DEVICE_H

#include "usb.h"

// Setup packets that are specific to CDC Devices
#define CDC_CONFIG_SETLINECODING 0x20
#define CDC_CONFIG_GETLINECODING 0x21
#define CDC_CONFIG_CONTROLLINESTATE 0x22

char CDC_SetupPacket(USB_SETUP_PACKET *setup, char* data, short length);
void CDC_HandlePacket(short length);

#endif