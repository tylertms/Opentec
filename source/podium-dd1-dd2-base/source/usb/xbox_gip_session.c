#include "usb/xbox_gip_session.h"

#include <stdint.h>

enum {
    XBOX_GIP_SESSION_PACKET = 5,
    XBOX_GIP_SESSION_ACTIVATE = 0,
    XBOX_GIP_SESSION_PAUSE = 1,
    XBOX_GIP_SESSION_TRANSFER_STATUS = 3,
    XBOX_GIP_SESSION_SUSPEND_OUTPUT = 4,
    XBOX_GIP_SESSION_RESET_FORCE_FEEDBACK = 5,
    XBOX_GIP_SESSION_TRANSFER_STATUS_ALTERNATE = 6,
    XBOX_GIP_SESSION_RESET_DEVICE = 7,
};

/**
 * @brief Initializes the Xbox GIP session.
 *
 * Starts the protocol in discovery with discovery also selected as the fallback state.
 *
 * @param[out] session Session state to initialize.
 */
void usb_xbox_gip_session_init(UsbXboxGipSession *session) {
    *session = (UsbXboxGipSession){
        .state = USB_XBOX_GIP_SESSION_DISCOVERY,
        .resume_state = USB_XBOX_GIP_SESSION_DISCOVERY,
    };
}

/**
 * @brief Enters the Xbox GIP metadata state.
 *
 * Selects the transient state used between discovery and the ready session.
 *
 * @param[in,out] session Session entering metadata download.
 */
void usb_xbox_gip_session_begin_metadata(UsbXboxGipSession *session) {
    session->state = USB_XBOX_GIP_SESSION_METADATA_DOWNLOAD;
}

/**
 * @brief Finishes the Xbox GIP metadata state.
 *
 * Advances the session to ready after the metadata download has been queued.
 *
 * @param[in,out] session Session completing metadata setup.
 */
void usb_xbox_gip_session_finish_metadata(UsbXboxGipSession *session) {
    session->state = USB_XBOX_GIP_SESSION_READY;
}

/**
 * @brief Handles an Xbox GIP session command.
 *
 * Applies activation, pause, output suspension, reset, and transfer-status commands from byte 4
 * of request packet 5 while enforcing each command's permitted session state.
 *
 * @param[in,out] session Active Xbox GIP session.
 * @param[in] request Request packet containing the packet identifier and command.
 * @return Actions required to complete the accepted command.
 */
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

/**
 * @brief Completes an Xbox GIP force-feedback reset.
 *
 * Restores the session state saved when the reset command was accepted.
 *
 * @param[in,out] session Session completing the force-feedback reset.
 */
void usb_xbox_gip_session_finish_force_feedback_reset(UsbXboxGipSession *session) {
    session->state = session->resume_state;
}
