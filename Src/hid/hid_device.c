#include "hid/hid_device.h"

static char buffer[33];

void HID_HandlePacket(unsigned char ep, short length) {
    USB_Fetch(1, buffer, &length);
}

char HID_SetupPacket(USB_SETUP_PACKET *setup, char *data, short length) {
    return USB_ERR;
}