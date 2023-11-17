#ifndef __HID_DEVICE_H
#define __HID_DEVICE_H

#include "usb.h"
#include "usb_config.h"


#define HID_CONFIG_GETDESCRIPTOR 0x06
#define HID_CONFIG_SETDESCRIPTOR 0x07
#define HID_CLASS_GETREPORT 0x01
#define HID_CLASS_GETIDLE 0x02
#define HID_CLASS_GETPROTOCOL 0x03
#define HID_CLASS_SETREPORT 0x09
#define HID_CLASS_SETIDLE 0x0A
#define HID_CLASS_SETPROTOCOL 0x0B


void HID_HandlePacket(short length);
char HID_SetupPacket(USB_SETUP_PACKET *setup, char *data, short length);

#endif