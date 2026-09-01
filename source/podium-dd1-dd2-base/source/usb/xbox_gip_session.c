#include "usb/xbox_gip_session.h"

#include <stdint.h>

/** @brief Private packet and command values used by the Xbox GIP session state machine. */
enum {
    XBOX_GIP_SESSION_PACKET = 5,                    /**< Session packet identifier. */
    XBOX_GIP_SESSION_ACTIVATE = 0,                  /**< Activate-session command. */
    XBOX_GIP_SESSION_PAUSE = 1,                     /**< Pause-session command. */
    XBOX_GIP_SESSION_TRANSFER_STATUS = 3,           /**< Transfer-status command. */
    XBOX_GIP_SESSION_SUSPEND_OUTPUT = 4,            /**< Suspend-output command. */
    XBOX_GIP_SESSION_RESET_FORCE_FEEDBACK = 5,      /**< Force-feedback reset command. */
    XBOX_GIP_SESSION_TRANSFER_STATUS_ALTERNATE = 6, /**< Alternate transfer-status command. */
    XBOX_GIP_SESSION_RESET_DEVICE = 7,              /**< Device-reset command. */
};

void usb_xbox_gip_session_init(UsbXboxGipSession *session) {
    *session = (UsbXboxGipSession){
        .state = USB_XBOX_GIP_SESSION_DISCOVERY,
        .resume_state = USB_XBOX_GIP_SESSION_DISCOVERY,
    };
}

void usb_xbox_gip_session_begin_metadata(UsbXboxGipSession *session) {
    session->state = USB_XBOX_GIP_SESSION_METADATA_DOWNLOAD;
}

void usb_xbox_gip_session_finish_metadata(UsbXboxGipSession *session) {
    session->state = USB_XBOX_GIP_SESSION_READY;
}

UsbXboxGipSessionAction usb_xbox_gip_session_handle(UsbXboxGipSession *session,
                                                    const uint8_t request[5]) {
    if (request[0] != XBOX_GIP_SESSION_PACKET) {
        return USB_XBOX_GIP_SESSION_ACTION_NONE;
    }

    switch (request[4]) {
    case XBOX_GIP_SESSION_ACTIVATE:
        if (session->state > USB_XBOX_GIP_SESSION_READY) {
            return USB_XBOX_GIP_SESSION_ACTION_NONE;
        }
        session->state = USB_XBOX_GIP_SESSION_ACTIVE;
        return USB_XBOX_GIP_SESSION_ACTION_SEND_READY | USB_XBOX_GIP_SESSION_ACTION_REFRESH_STATE;
    case XBOX_GIP_SESSION_PAUSE:
        if (session->state != USB_XBOX_GIP_SESSION_ACTIVE) {
            return USB_XBOX_GIP_SESSION_ACTION_NONE;
        }
        session->state = USB_XBOX_GIP_SESSION_READY;
        return USB_XBOX_GIP_SESSION_ACTION_SEND_READY;
    case XBOX_GIP_SESSION_TRANSFER_STATUS:
    case XBOX_GIP_SESSION_TRANSFER_STATUS_ALTERNATE:
        return USB_XBOX_GIP_SESSION_ACTION_SEND_TRANSFER_STATUS;
    case XBOX_GIP_SESSION_SUSPEND_OUTPUT:
        if (session->state > USB_XBOX_GIP_SESSION_ACTIVE) {
            return USB_XBOX_GIP_SESSION_ACTION_NONE;
        }
        session->state = USB_XBOX_GIP_SESSION_OUTPUT_SUSPENDED;
        return USB_XBOX_GIP_SESSION_ACTION_SEND_READY | USB_XBOX_GIP_SESSION_ACTION_SUSPEND_OUTPUT;
    case XBOX_GIP_SESSION_RESET_FORCE_FEEDBACK:
        if (session->state != USB_XBOX_GIP_SESSION_ACTIVE) {
            return USB_XBOX_GIP_SESSION_ACTION_NONE;
        }
        session->resume_state = USB_XBOX_GIP_SESSION_ACTIVE;
        session->state = USB_XBOX_GIP_SESSION_RESET_FORCE_FEEDBACK;
        return USB_XBOX_GIP_SESSION_ACTION_SEND_READY |
               USB_XBOX_GIP_SESSION_ACTION_RESET_FORCE_FEEDBACK;
    case XBOX_GIP_SESSION_RESET_DEVICE:
        session->state = USB_XBOX_GIP_SESSION_RESET_DEVICE;
        return USB_XBOX_GIP_SESSION_ACTION_SEND_READY | USB_XBOX_GIP_SESSION_ACTION_RESET_DEVICE;
    default:
        return USB_XBOX_GIP_SESSION_ACTION_NONE;
    }
}

void usb_xbox_gip_session_finish_force_feedback_reset(UsbXboxGipSession *session) {
    session->state = session->resume_state;
}
