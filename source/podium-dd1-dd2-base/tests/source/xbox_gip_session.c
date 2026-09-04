#include "usb/xbox_gip_session.h"

#include <assert.h>
#include <stdint.h>

static UsbXboxGipSessionAction send_command(UsbXboxGipSession *session, uint8_t command) {
    uint8_t request[5] = {5, 0, 0, 0, command};
    return usb_xbox_gip_session_handle(session, request);
}

static void send_memory_packet(UsbXboxGipSession *session, uint8_t mode, uint8_t state) {
    uint8_t request[6] = {6, 0, 0, 0, mode, state};
    assert(usb_xbox_gip_session_handle(session, request) == USB_XBOX_GIP_SESSION_ACTION_NONE);
}

static void test_moves_from_discovery_through_active_and_ready(void) {
    UsbXboxGipSession session;
    usb_xbox_gip_session_init(&session);

    usb_xbox_gip_session_begin_metadata(&session);
    assert(session.state == USB_XBOX_GIP_SESSION_METADATA_DOWNLOAD);
    usb_xbox_gip_session_finish_metadata(&session);
    assert(session.state == USB_XBOX_GIP_SESSION_READY);

    assert(send_command(&session, 0) ==
           (USB_XBOX_GIP_SESSION_ACTION_SEND_READY | USB_XBOX_GIP_SESSION_ACTION_REFRESH_STATE));
    assert(session.state == USB_XBOX_GIP_SESSION_ACTIVE);
    assert(send_command(&session, 1) == USB_XBOX_GIP_SESSION_ACTION_SEND_READY);
    assert(session.state == USB_XBOX_GIP_SESSION_READY);
    assert(send_command(&session, 1) == USB_XBOX_GIP_SESSION_ACTION_NONE);
}

static void test_handles_status_and_reset_commands(void) {
    UsbXboxGipSession session;
    usb_xbox_gip_session_init(&session);

    assert(send_command(&session, 3) == USB_XBOX_GIP_SESSION_ACTION_SEND_TRANSFER_STATUS);
    assert(send_command(&session, 6) == USB_XBOX_GIP_SESSION_ACTION_SEND_TRANSFER_STATUS);
    assert(send_command(&session, 5) == USB_XBOX_GIP_SESSION_ACTION_NONE);

    assert(send_command(&session, 0) != USB_XBOX_GIP_SESSION_ACTION_NONE);
    assert(send_command(&session, 5) == (USB_XBOX_GIP_SESSION_ACTION_SEND_READY |
                                         USB_XBOX_GIP_SESSION_ACTION_RESET_FORCE_FEEDBACK));
    assert(session.state == USB_XBOX_GIP_SESSION_RESET_FORCE_FEEDBACK);
    usb_xbox_gip_session_finish_force_feedback_reset(&session);
    assert(session.state == USB_XBOX_GIP_SESSION_ACTIVE);

    assert(send_command(&session, 7) ==
           (USB_XBOX_GIP_SESSION_ACTION_SEND_READY | USB_XBOX_GIP_SESSION_ACTION_RESET_DEVICE));
    assert(session.state == USB_XBOX_GIP_SESSION_RESET_DEVICE);
}

static void test_suspends_only_discovery_ready_or_active_sessions(void) {
    UsbXboxGipSession session;
    usb_xbox_gip_session_init(&session);

    assert(send_command(&session, 4) ==
           (USB_XBOX_GIP_SESSION_ACTION_SEND_READY | USB_XBOX_GIP_SESSION_ACTION_SUSPEND_OUTPUT));
    assert(session.state == USB_XBOX_GIP_SESSION_OUTPUT_SUSPENDED);
    assert(send_command(&session, 4) == USB_XBOX_GIP_SESSION_ACTION_NONE);

    uint8_t unrelated[5] = {4, 0, 0, 0, 0};
    assert(usb_xbox_gip_session_handle(&session, unrelated) == USB_XBOX_GIP_SESSION_ACTION_NONE);
}

static void test_suspends_an_active_session(void) {
    UsbXboxGipSession session;
    usb_xbox_gip_session_init(&session);
    usb_xbox_gip_session_finish_metadata(&session);
    assert(send_command(&session, 0) ==
           (USB_XBOX_GIP_SESSION_ACTION_SEND_READY | USB_XBOX_GIP_SESSION_ACTION_REFRESH_STATE));

    assert(send_command(&session, 4) ==
           (USB_XBOX_GIP_SESSION_ACTION_SEND_READY | USB_XBOX_GIP_SESSION_ACTION_SUSPEND_OUTPUT));
    assert(session.state == USB_XBOX_GIP_SESSION_OUTPUT_SUSPENDED);
}

static void test_handles_memory_control_and_response_states(void) {
    UsbXboxGipSession session;
    usb_xbox_gip_session_init(&session);
    usb_xbox_gip_session_finish_metadata(&session);
    assert(send_command(&session, 0) ==
           (USB_XBOX_GIP_SESSION_ACTION_SEND_READY | USB_XBOX_GIP_SESSION_ACTION_REFRESH_STATE));

    send_memory_packet(&session, 0, 0);
    assert(session.state == USB_XBOX_GIP_SESSION_MEMORY_CONTROL);

    send_memory_packet(&session, 1, 1);
    assert(session.state == USB_XBOX_GIP_SESSION_MEMORY_CONTROL);
    send_memory_packet(&session, 1, 0);
    assert(session.state == USB_XBOX_GIP_SESSION_ACTIVE);

    send_memory_packet(&session, 0, 5);
    assert(session.state == USB_XBOX_GIP_SESSION_MEMORY_RESPONSE);
    send_memory_packet(&session, 1, 0);
    assert(session.state == USB_XBOX_GIP_SESSION_MEMORY_RESPONSE);
    usb_xbox_gip_session_finish_memory_response(&session);
    assert(session.state == USB_XBOX_GIP_SESSION_ACTIVE);
    usb_xbox_gip_session_finish_memory_response(&session);
    assert(session.state == USB_XBOX_GIP_SESSION_ACTIVE);
}

int main(void) {
    test_moves_from_discovery_through_active_and_ready();
    test_handles_status_and_reset_commands();
    test_suspends_only_discovery_ready_or_active_sessions();
    test_suspends_an_active_session();
    test_handles_memory_control_and_response_states();
    return 0;
}
