#ifndef OPENTEC_BASE_USB_XBOX_GIP_SESSION_H
#define OPENTEC_BASE_USB_XBOX_GIP_SESSION_H

#include <stdint.h>

/** @brief State of the Xbox GIP session state machine. */
typedef enum {
    USB_XBOX_GIP_SESSION_DISCOVERY = 0, /**< Discovery is in progress. */
    USB_XBOX_GIP_SESSION_READY = 1,     /**< Session is ready for activation. */
    USB_XBOX_GIP_SESSION_ACTIVE = 2,    /**< Session is actively exchanging input and output. */
    USB_XBOX_GIP_SESSION_OUTPUT_SUSPENDED = 3,     /**< Session output is suspended. */
    USB_XBOX_GIP_SESSION_RESET_DEVICE = 4,         /**< Device reset has been requested. */
    USB_XBOX_GIP_SESSION_RESET_FORCE_FEEDBACK = 6, /**< Force-feedback reset is in progress. */
    USB_XBOX_GIP_SESSION_METADATA_DOWNLOAD = 7,    /**< Metadata download is in progress. */
    USB_XBOX_GIP_SESSION_MEMORY_CONTROL = 8,       /**< Memory control transfer is in progress. */
    USB_XBOX_GIP_SESSION_MEMORY_RESPONSE = 9,      /**< Memory response is being completed. */
} UsbXboxGipSessionState;

/** @brief Bit flags describing actions accepted by the Xbox GIP session state machine. */
typedef enum {
    USB_XBOX_GIP_SESSION_ACTION_NONE = 0,            /**< No action is required. */
    USB_XBOX_GIP_SESSION_ACTION_SEND_READY = 1 << 0, /**< Send a ready response. */
    USB_XBOX_GIP_SESSION_ACTION_SEND_TRANSFER_STATUS =
        1 << 1,                                          /**< Send a transfer-status response. */
    USB_XBOX_GIP_SESSION_ACTION_REFRESH_STATE = 1 << 2,  /**< Refresh the exported session state. */
    USB_XBOX_GIP_SESSION_ACTION_SUSPEND_OUTPUT = 1 << 3, /**< Suspend output processing. */
    USB_XBOX_GIP_SESSION_ACTION_RESET_FORCE_FEEDBACK = 1 << 4, /**< Reset force-feedback state. */
    USB_XBOX_GIP_SESSION_ACTION_RESET_DEVICE = 1 << 5,         /**< Request a device reset. */
} UsbXboxGipSessionAction;

/** @brief Current Xbox GIP session state and force-feedback resume state. */
typedef struct {
    UsbXboxGipSessionState state;        /**< Current session state. */
    UsbXboxGipSessionState resume_state; /**< State restored after force-feedback reset. */
} UsbXboxGipSession;

/**
 * @brief Initializes the Xbox GIP session.
 *
 * Starts the protocol in discovery with discovery also selected as the fallback state.
 *
 * @param[out] session Session state to initialize.
 */
void usb_xbox_gip_session_init(UsbXboxGipSession *session);

/**
 * @brief Enters the Xbox GIP metadata state.
 *
 * Selects the transient state used between discovery and the ready session.
 *
 * @param[in,out] session Session entering metadata download.
 */
void usb_xbox_gip_session_begin_metadata(UsbXboxGipSession *session);

/**
 * @brief Finishes the Xbox GIP metadata state.
 *
 * Advances the session to ready after metadata transfer setup has been queued.
 *
 * @param[in,out] session Session completing metadata setup.
 */
void usb_xbox_gip_session_finish_metadata(UsbXboxGipSession *session);

/**
 * @brief Handles an Xbox GIP session command.
 *
 * Applies activation, pause, output suspension, reset, transfer-status, and packet-6 memory
 * commands while enforcing state restrictions for the state-changing commands. Memory packet
 * handling uses byte 4 as the complete marker and byte 5 as the information selector, matching
 * the two transient memory states in the released session state machine.
 *
 * @param[in,out] session Active Xbox GIP session.
 * @param[in] request Request packet with at least six valid bytes.
 * @return Actions required to complete the accepted command.
 */
UsbXboxGipSessionAction usb_xbox_gip_session_handle(UsbXboxGipSession *session,
                                                    const uint8_t request[]);

/**
 * @brief Completes an Xbox GIP force-feedback reset.
 *
 * Restores the session state saved when the reset command was accepted.
 *
 * @param[in,out] session Session completing the force-feedback reset.
 */
void usb_xbox_gip_session_finish_force_feedback_reset(UsbXboxGipSession *session);

/**
 * @brief Completes an Xbox GIP memory-response transfer.
 *
 * Returns a session held in the official memory-response state to active after its lower-level
 * response transfer has been accepted. Calls for any other session state leave the session
 * unchanged.
 *
 * @param[in,out] session Session completing its memory response.
 */
void usb_xbox_gip_session_finish_memory_response(UsbXboxGipSession *session);

#endif
