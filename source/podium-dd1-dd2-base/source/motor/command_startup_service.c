#include "motor/command_startup_service.h"

#include <stdbool.h>
#include <stdint.h>

#include "motor/command_application.h"
#include "motor/command_channel.h"
#include "motor/command_channel_mailbox.h"
#include "motor/command_mailbox.h"
#include "motor/command_startup.h"
#include "transfer/command.h"

/**
 * @brief Marks a motor-command startup attempt as failed.
 *
 * Releases owner 0x20, resets mailbox progress, and latches failure so no further startup traffic
 * is generated.
 *
 * @param[in,out] service Startup service to stop.
 * @param[in,out] exchange Mailbox exchange to reset.
 * @param[in,out] transport Shared command transport to release.
 * @return Failed startup result.
 */
static MotorCommandStartupServiceResult fail(MotorCommandStartupService *service,
                                             MotorCommandMailboxExchange *exchange,
                                             CommandTransport *transport) {
    service->failed = true;
    service->startup.active = false;
    motor_command_mailbox_exchange_reset(exchange);
    command_transport_release(transport, MOTOR_COMMAND_STARTUP_OWNER);
    return MOTOR_COMMAND_STARTUP_SERVICE_FAILED;
}

/**
 * @brief Queues the startup action selected by the sequence planner.
 *
 * Schedules the initial mailbox capacity read or frames and retains the selected reset, digest, or
 * information request until the mailbox reports a completed write.
 *
 * @param[in,out] service Startup service applying the action.
 * @param[in,out] channel Motor-command protocol channel.
 * @param[in,out] exchange Mailbox exchange receiving protocol packets.
 * @param[in,out] transport Shared command transport.
 * @param[in] action Startup action to queue.
 * @return True when the action was queued or required no transport work.
 */
static bool queue_action(MotorCommandStartupService *service, MotorCommandChannel *channel,
                         MotorCommandMailboxExchange *exchange, CommandTransport *transport,
                         MotorCommandStartupAction action) {
    if (action.type == MOTOR_COMMAND_STARTUP_ACTION_NONE) {
        return true;
    }
    if (action.type == MOTOR_COMMAND_STARTUP_ACTION_READ_STATUS) {
        if (motor_command_mailbox_queue_length_read(transport, service->length_record) !=
            COMMAND_TRANSPORT_COMPLETE) {
            return false;
        }
        service->status_read_pending = true;
        return true;
    }

    bool encoded = false;
    MotorCommandStartupWrite write = MOTOR_COMMAND_STARTUP_WRITE_REQUEST;
    if (action.command == MOTOR_COMMAND_STARTUP_SEQUENCE_RESET_COMMAND) {
        encoded = motor_command_channel_queue_sequence_reset(channel);
        write = MOTOR_COMMAND_STARTUP_WRITE_RESET;
    } else if (action.command == MOTOR_COMMAND_STARTUP_DIGEST_COMMAND) {
        encoded = motor_command_channel_queue_digest_request(channel);
    } else if (action.command == MOTOR_COMMAND_STARTUP_INFO_COMMAND) {
        encoded = motor_command_channel_queue_information_request(channel, action.selector);
    }
    if (!encoded || !motor_command_mailbox_exchange_queue(exchange, channel->buffers.transmit,
                                                          channel->transmit_length)) {
        return false;
    }
    service->current_command = action.command;
    service->response_ready = false;
    service->pending_write = write;
    return true;
}

void motor_command_startup_service_init(MotorCommandStartupService *service) {
    *service = (MotorCommandStartupService){0};
    motor_command_startup_init(&service->startup);
}

MotorCommandStartupServiceResult
motor_command_startup_service_run(MotorCommandStartupService *service, MotorCommandChannel *channel,
                                  MotorCommandMailboxExchange *exchange,
                                  CommandTransport *transport) {
    if (service == 0 || channel == 0 || exchange == 0 || transport == 0 || service->failed) {
        return MOTOR_COMMAND_STARTUP_SERVICE_FAILED;
    }
    if (service->startup.complete) {
        return MOTOR_COMMAND_STARTUP_SERVICE_COMPLETE;
    }

    if (service->status_read_pending) {
        CommandTransportResult status =
            command_transport_poll(transport, MOTOR_COMMAND_STARTUP_OWNER);
        if (status == COMMAND_TRANSPORT_BUSY) {
            return MOTOR_COMMAND_STARTUP_SERVICE_RUNNING;
        }
        if (status != COMMAND_TRANSPORT_COMPLETE) {
            return fail(service, exchange, transport);
        }
        service->status_read_pending = false;
    } else if (service->startup.active &&
               (service->pending_write != MOTOR_COMMAND_STARTUP_WRITE_NONE ||
                ((service->startup.phase == MOTOR_COMMAND_STARTUP_WAIT_DIGEST ||
                  service->startup.phase == MOTOR_COMMAND_STARTUP_WAIT_FIRST_INFO ||
                  service->startup.phase == MOTOR_COMMAND_STARTUP_WAIT_SECOND_INFO) &&
                 !service->response_ready))) {
        MotorCommandChannelMailboxEvent event =
            motor_command_channel_mailbox_run(channel, exchange, transport);
        if (event.mailbox_event == MOTOR_COMMAND_MAILBOX_EXCHANGE_FAILED) {
            return fail(service, exchange, transport);
        }
        if (event.mailbox_event == MOTOR_COMMAND_MAILBOX_EXCHANGE_PACKET_WRITTEN) {
            if (service->pending_write == MOTOR_COMMAND_STARTUP_WRITE_RESET) {
                service->current_command = 0;
            }
            service->pending_write = MOTOR_COMMAND_STARTUP_WRITE_NONE;
        }
        if ((event.channel_event.actions & MOTOR_COMMAND_CHANNEL_ACTION_WRITE) != 0) {
            service->pending_write = MOTOR_COMMAND_STARTUP_WRITE_CONTROL;
        }
        if (event.channel_event.receive_result == MOTOR_COMMAND_RECEIVE_RESET) {
            service->current_command = 0;
        }
        if (event.channel_event.application.result == MOTOR_COMMAND_APPLICATION_CALIBRATION ||
            event.channel_event.application.result == MOTOR_COMMAND_APPLICATION_INFORMATION) {
            service->current_command = channel->message.command;
            service->response_ready = true;
        }
    }

    if (service->status_read_pending ||
        service->pending_write != MOTOR_COMMAND_STARTUP_WRITE_NONE) {
        return MOTOR_COMMAND_STARTUP_SERVICE_RUNNING;
    }

    MotorCommandStartupInput input = {
        .command = service->current_command,
        .status_read_pending = service->status_read_pending,
        .response_ready = service->response_ready,
        .restart = false,
    };
    MotorCommandStartupAction action =
        motor_command_startup_run(&service->startup, transport, input);
    if (!queue_action(service, channel, exchange, transport, action)) {
        return fail(service, exchange, transport);
    }
    return service->startup.complete ? MOTOR_COMMAND_STARTUP_SERVICE_COMPLETE
                                     : MOTOR_COMMAND_STARTUP_SERVICE_RUNNING;
}
