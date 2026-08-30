#ifndef OPENTEC_BASE_PLATFORM_USB_H
#define OPENTEC_BASE_PLATFORM_USB_H

#include <stdbool.h>
#include <stdint.h>

enum { PLATFORM_USB_PACKET_SIZE = 64 };

typedef enum {
    PLATFORM_USB_EVENT_RESET,
    PLATFORM_USB_EVENT_SETUP,
    PLATFORM_USB_EVENT_OUT,
    PLATFORM_USB_EVENT_IN_COMPLETE,
    PLATFORM_USB_EVENT_SUSPEND,
} PlatformUsbEventType;

typedef struct {
    PlatformUsbEventType type;
    uint8_t endpoint;
    uint8_t length;
    uint8_t data[PLATFORM_USB_PACKET_SIZE];
} PlatformUsbEvent;

void platform_usb_init(void);
bool platform_usb_connected(void);
void platform_usb_attach(void);
void platform_usb_detach(void);
void platform_usb_restart(void);
void platform_usb_signal_resume(void);
bool platform_usb_take_event(PlatformUsbEvent *event);
bool platform_usb_send(uint8_t endpoint, const uint8_t *data, uint8_t length, bool data_one);
bool platform_usb_receive(uint8_t endpoint, uint8_t length, bool data_one);
void platform_usb_control_ready(void);
void platform_usb_set_address(uint8_t address);
void platform_usb_configure_endpoint(uint8_t endpoint, bool input, bool output);
void platform_usb_unconfigure_endpoint(uint8_t endpoint);
void platform_usb_stall(uint8_t endpoint);
bool platform_usb_endpoint_halted(uint8_t endpoint_address);
void platform_usb_set_endpoint_halt(uint8_t endpoint_address, bool halted);

#endif
