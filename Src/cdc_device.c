#include "cdc_device.h"

static char buffer[64];
static char lineCoding[7];

char CDC_SetupPacket(USB_SETUP_PACKET *setup, char *data, short length) {
    // Windows requires us to remember the line coding
    switch (setup->Request) {
    case CDC_CONFIG_CONTROLLINESTATE:
        break;
    case CDC_CONFIG_GETLINECODING:
        USB_Transmit(0, lineCoding, 7);
        break;
    case CDC_CONFIG_SETLINECODING:
        for (int i = 0; i < 7; i++) {
            lineCoding[i] = data[i];
        }
        return USB_OK;
        break;
    }
}

void CDC_HandlePacket(short length) {
    // Just mirror the text
    USB_Fetch(2, buffer, &length);

    // do NOT busy wait. We are still in the ISR, it will never clear.
    if (!USB_IsTransmitPending(2)) {
        USB_Transmit(2, buffer, length);
    }
}
