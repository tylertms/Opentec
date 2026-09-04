#include "usb/xbox_gip_session.h"

#include <stdint.h>

/** @brief Private packet and command values used by the Xbox GIP session state machine. */
enum {
    XBOX_GIP_SESSION_PACKET = 5,                    /**< Session packet identifier. */
    XBOX_GIP_MEMORY_PACKET = 6,                     /**< Memory-control packet identifier. */
    XBOX_GIP_SESSION_ACTIVATE = 0,                  /**< Activate-session command. */
    XBOX_GIP_SESSION_PAUSE = 1,                     /**< Pause-session command. */
    XBOX_GIP_SESSION_TRANSFER_STATUS = 3,           /**< Transfer-status command. */
    XBOX_GIP_SESSION_SUSPEND_OUTPUT = 4,            /**< Suspend-output command. */
    XBOX_GIP_SESSION_RESET_FORCE_FEEDBACK = 5,      /**< Force-feedback reset command. */
    XBOX_GIP_SESSION_TRANSFER_STATUS_ALTERNATE = 6, /**< Alternate transfer-status command. */
    XBOX_GIP_SESSION_RESET_DEVICE = 7,              /**< Device-reset command. */
    XBOX_GIP_MEMORY_CONTROL_COMPLETE = 1,           /**< Complete-memory-control marker. */
    XBOX_GIP_MEMORY_CONTROL_RELEASE = 0,            /**< Release-memory-control state. */
    XBOX_GIP_MEMORY_INFORMATION = 5,                /**< Memory-information selector. */
};

/**
 * @brief Applies one packet-6 memory state transition.
 *
 * Enters memory control or memory response after an active-session request and releases memory
 * control only after a complete packet requests the release state. Memory response remains in
 * state 9 until the lower-level response transfer calls the completion operation.
 *
 * @param[in,out] session Xbox GIP session state.
 * @param[in] request Packet-6 request with at least six valid bytes.
 */
static void handle_memory_packet(UsbXboxGipSession *session, const uint8_t request[]) {
    if (request[0] != XBOX_GIP_MEMORY_PACKET) {
        return;
    }

    if (session->state == USB_XBOX_GIP_SESSION_ACTIVE &&
        request[4] != XBOX_GIP_MEMORY_CONTROL_COMPLETE) {
        session->state = request[5] == XBOX_GIP_MEMORY_INFORMATION
                             ? USB_XBOX_GIP_SESSION_MEMORY_RESPONSE
                             : USB_XBOX_GIP_SESSION_MEMORY_CONTROL;
        return;
    }

    if (session->state == USB_XBOX_GIP_SESSION_MEMORY_CONTROL &&
        request[4] == XBOX_GIP_MEMORY_CONTROL_COMPLETE &&
        request[5] == XBOX_GIP_MEMORY_CONTROL_RELEASE) {
        session->state = USB_XBOX_GIP_SESSION_ACTIVE;
    }
}

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
                                                    const uint8_t request[]) {
    if (request[0] == XBOX_GIP_MEMORY_PACKET) {
        handle_memory_packet(session, request);
        return USB_XBOX_GIP_SESSION_ACTION_NONE;
    }
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

void usb_xbox_gip_session_finish_memory_response(UsbXboxGipSession *session) {
    if (session->state == USB_XBOX_GIP_SESSION_MEMORY_RESPONSE) {
        session->state = USB_XBOX_GIP_SESSION_ACTIVE;
    }
}
