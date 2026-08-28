#ifndef OPENTEC_BASE_USB_XBOX_GIP_SESSION_H
#define OPENTEC_BASE_USB_XBOX_GIP_SESSION_H

#include <stdint.h>

typedef enum {
    USB_XBOX_GIP_SESSION_DISCOVERY = 0,
    USB_XBOX_GIP_SESSION_READY = 1,
    USB_XBOX_GIP_SESSION_ACTIVE = 2,
    USB_XBOX_GIP_SESSION_OUTPUT_SUSPENDED = 3,
    USB_XBOX_GIP_SESSION_RESET_DEVICE = 4,
    USB_XBOX_GIP_SESSION_RESET_FORCE_FEEDBACK = 6,
    USB_XBOX_GIP_SESSION_METADATA_DOWNLOAD = 7,
} UsbXboxGipSessionState;

typedef enum {
    USB_XBOX_GIP_SESSION_ACTION_NONE = 0,
    USB_XBOX_GIP_SESSION_ACTION_SEND_READY = 1 << 0,
    USB_XBOX_GIP_SESSION_ACTION_SEND_TRANSFER_STATUS = 1 << 1,
    USB_XBOX_GIP_SESSION_ACTION_REFRESH_STATE = 1 << 2,
    USB_XBOX_GIP_SESSION_ACTION_SUSPEND_OUTPUT = 1 << 3,
    USB_XBOX_GIP_SESSION_ACTION_RESET_FORCE_FEEDBACK = 1 << 4,
    USB_XBOX_GIP_SESSION_ACTION_RESET_DEVICE = 1 << 5,
} UsbXboxGipSessionAction;

typedef struct {
    UsbXboxGipSessionState state;
    UsbXboxGipSessionState resume_state;
} UsbXboxGipSession;

void usb_xbox_gip_session_init(UsbXboxGipSession *session);
void usb_xbox_gip_session_begin_metadata(UsbXboxGipSession *session);
void usb_xbox_gip_session_finish_metadata(UsbXboxGipSession *session);
UsbXboxGipSessionAction usb_xbox_gip_session_handle(UsbXboxGipSession *session,
                                                    const uint8_t request[5]);
void usb_xbox_gip_session_finish_force_feedback_reset(UsbXboxGipSession *session);

#endif
