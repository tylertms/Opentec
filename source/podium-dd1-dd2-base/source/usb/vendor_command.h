#ifndef OPENTEC_BASE_USB_VENDOR_COMMAND_H
#define OPENTEC_BASE_USB_VENDOR_COMMAND_H

#include <stdbool.h>
#include <stdint.h>

#include "usb/output_command.h"
#include "wheel/transfer_service.h"

/** @brief Category selected by a decoded vendor command opcode. */
typedef enum {
    USB_VENDOR_COMMAND_WHEEL_OUTPUT_REPORT,    /**< Wheel output report command. */
    USB_VENDOR_COMMAND_TUNING_MENU,            /**< Tuning-menu command. */
    USB_VENDOR_COMMAND_DEVICE_CONTROL_UPDATE,  /**< Device-control update command. */
    USB_VENDOR_COMMAND_DIAGNOSTIC_SNAPSHOT,    /**< Diagnostic snapshot command. */
    USB_VENDOR_COMMAND_REMOTE_TUNING,          /**< Opcode-five remote-tuning command. */
    USB_VENDOR_COMMAND_TUNING_STATUS,          /**< Tuning-status command. */
    USB_VENDOR_COMMAND_SCRIPT_AXES,            /**< Script axes query. */
    USB_VENDOR_COMMAND_SCRIPT_SAMPLES,         /**< Script sample query. */
    USB_VENDOR_COMMAND_SCRIPT_SLOT,            /**< Script slot query. */
    USB_VENDOR_COMMAND_SCRIPT_STATUS,          /**< Script status query. */
    USB_VENDOR_COMMAND_SCRIPT_VALUES,          /**< Script values query. */
    USB_VENDOR_COMMAND_NATIVE_RESET,           /**< Native service reset command. */
    USB_VENDOR_COMMAND_WHEEL_TRANSFER_PAYLOAD, /**< Native attached-wheel transfer fragment. */
    USB_VENDOR_COMMAND_TRANSFER_REQUEST =
        USB_VENDOR_COMMAND_WHEEL_TRANSFER_PAYLOAD, /**< Compatibility alias for wheel transfer. */
    USB_VENDOR_COMMAND_EXTENDED,                   /**< Extended vendor command. */
} UsbVendorCommandKind;

/** @brief Decoded vendor command category, opcode, and argument view. */
typedef struct {
    UsbVendorCommandKind kind; /**< Decoded command category. */
    uint8_t opcode;            /**< Top-level vendor command opcode. */
    const uint8_t *arguments;  /**< Bytes following the opcode. */
    uint8_t length;            /**< Number of bytes in #arguments. */
} UsbVendorCommand;

/** @brief Action requested for a wheel transfer command. */
typedef enum {
    USB_WHEEL_TRANSFER_START = 1,  /**< Start the selected wheel transfer. */
    USB_WHEEL_TRANSFER_STATUS = 2, /**< Query the selected wheel transfer status. */
} UsbWheelTransferAction;

/** @brief Decoded wheel transfer request and requested action. */
typedef struct {
    WheelTransferRequest request;  /**< Wheel transfer channel. */
    UsbWheelTransferAction action; /**< Action to perform on the channel. */
} UsbWheelTransferCommand;

/**
 * @brief Decodes a USB vendor-transfer output command.
 *
 * Selects the supported command category from the top-level opcode and exposes the bytes after
 * that opcode as the command arguments.
 *
 * @param[in] output Vendor-transfer output command to classify.
 * @param[out] command Destination for the decoded command view.
 * @return `true` when the output contains a supported vendor command; otherwise `false`.
 */
bool usb_vendor_command_decode(const UsbOutputCommand *output, UsbVendorCommand *command);

/**
 * @brief Extracts the first sample index from a script sample query.
 *
 * Combines the little-endian sample-index bytes after confirming that the decoded command is a
 * bounded script sample query.
 *
 * @param[in] command Decoded script sample query.
 * @param[out] index Destination for the first sample index.
 * @return `true` when the command contains a valid sample query index; otherwise `false`.
 */
bool usb_vendor_command_script_sample_index(const UsbVendorCommand *command, uint16_t *index);

/**
 * @brief Extracts the slot index from a script slot query.
 *
 * Reads the slot index after confirming that the decoded command selects a reportable script slot.
 *
 * @param[in] command Decoded script slot query.
 * @param[out] index Destination for the slot index.
 * @return `true` when the command contains a reportable slot index; otherwise `false`.
 */
bool usb_vendor_command_script_slot_index(const UsbVendorCommand *command, uint8_t *index);

/**
 * @brief Identifies the extended request that starts the motor-command handshake.
 *
 * Matches an extended request whose first three argument bytes are 0, 1, and 1.
 *
 * @param[in] command Decoded vendor command.
 * @return `true` for extended arguments 00 01 01; otherwise `false`.
 */
bool usb_vendor_command_requests_motor_command(const UsbVendorCommand *command);

/**
 * @brief Tests whether an extended vendor command selects the auxiliary menu route.
 *
 * @param[in] command Decoded vendor command.
 * @return True for the official auxiliary-menu command signature; otherwise false.
 */
bool usb_vendor_command_requests_auxiliary_menu(const UsbVendorCommand *command);

/**
 * @brief Decodes an extended wheel-transfer vendor command.
 *
 * Accepts the E0 route followed by little-endian command 0x0402 or 0x0502 and action one or two.
 *
 * @param[in] command Decoded vendor command.
 * @param[out] transfer Wheel-transfer request and action.
 * @return `true` when the extended arguments select a supported wheel transfer; otherwise `false`.
 */
bool usb_vendor_command_decode_wheel_transfer(const UsbVendorCommand *command,
                                              UsbWheelTransferCommand *transfer);

/**
 * @brief Extracts a complete attached-wheel report-17 payload.
 *
 * Accepts tuning-menu action one and exposes its complete 61-byte report payload.
 *
 * @param[in] command Decoded vendor command.
 * @return Pointer to the report-17 payload, or `NULL` when the command is incomplete or uses a
 * different action.
 */
const uint8_t *usb_vendor_command_decode_wheel_report_seventeen(const UsbVendorCommand *command);

/**
 * @brief Encodes an extended wheel-transfer status response.
 *
 * Clears a 64-byte vendor report and writes the FF E0 route, selected little-endian command, and
 * status byte into its header.
 *
 * @param[in] request Write or read request channel.
 * @param[in] status Current wheel-transfer status.
 * @param[out] output Encoded 64-byte vendor report.
 */
void usb_vendor_command_encode_wheel_transfer_response(WheelTransferRequest request,
                                                       WheelTransferStatus status,
                                                       uint8_t output[USB_DEVICE_REPORT_SIZE]);

#endif
