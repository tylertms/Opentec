#ifndef OPENTEC_BASE_USB_FEATURE_REPORT_H
#define OPENTEC_BASE_USB_FEATURE_REPORT_H

#include <stdbool.h>
#include <stdint.h>

#include "profile/bank.h"
#include "usb/device.h"

/** @brief Status values encoded in native feature report 31. */
typedef struct {
    uint16_t status;                  /**< Current system status value. */
    uint8_t wheel_mode;               /**< Attached-wheel operating mode. */
    uint8_t pedal_active;             /**< Active pedal profile identifier. */
    uint8_t auxiliary_profile;        /**< Active auxiliary pedal profile. */
    uint8_t axis_modes[2];            /**< Primary and secondary shifter axis modes. */
    uint8_t transfer_code;            /**< Attached-wheel transfer code. */
    uint8_t rotary_mode;              /**< Multi-position rotary reporting mode. */
    bool pedal_legacy;                /**< True while legacy pedal transport is active. */
    bool pedal_io_active;             /**< True while pedal I/O is active. */
    bool pedal_handshake_active;      /**< True while pedal startup handshake is active. */
    bool resistance_active;           /**< True while pedal resistance adjustment is active. */
    bool pedal_calibration_active;    /**< True while pedal calibration is active. */
    bool wheel_calibration_available; /**< True when wheel calibration controls are available. */
    bool wheel_axis_report_enabled;   /**< True when wheel axis reporting is enabled. */
    bool adapter_connected;           /**< True when an adapter is connected. */
} UsbFeatureReport31State;

/** @brief Rotary, pulse, and button values encoded in native feature report 33. */
typedef struct {
    uint8_t positions[3];         /**< One-based positions for the rotary channels. */
    int8_t events[3];             /**< Pending rotary events for the channels. */
    int8_t pulse_directions[4];   /**< Pending signed pulse directions. */
    uint8_t extended_buttons;     /**< Extended button bits. */
    uint8_t auxiliary_buttons[3]; /**< Auxiliary button bytes. */
    uint8_t rotary_mode;          /**< Multi-position rotary reporting mode. */
    bool tertiary_active;         /**< True when the third rotary channel is active. */
    bool remap_selectors;         /**< True when the alternate selector mapping is active. */
} UsbFeatureReport33State;

/**
 * @brief Encodes native feature report 31.
 *
 * Publishes wheel, pedal, shifter, adapter, rotary, transfer, calibration, and system status in
 * the reference 64-byte layout.
 *
 * @param[in] state Current feature-report status sources.
 * @param[out] output Encoded 64-byte report.
 */
void usb_feature_report_31_encode(const UsbFeatureReport31State *state,
                                  uint8_t output[USB_DEVICE_REPORT_SIZE]);

/**
 * @brief Encodes native feature report 32.
 *
 * Publishes active and selected profile slots, persistence state, and active profile values.
 *
 * @param[in] bank Current tuning-profile bank.
 * @param[in] dirty True when retained settings require persistence.
 * @param[out] output Encoded 64-byte report.
 */
void usb_feature_report_32_encode(const TuningProfileBank *bank, bool dirty,
                                  uint8_t output[USB_DEVICE_REPORT_SIZE]);

/**
 * @brief Encodes native feature report 33.
 *
 * Publishes rotary events or positions, queued motion directions, and extended and auxiliary
 * button state.
 *
 * @param[in] state Current rotary, motion, and button sources.
 * @param[out] output Encoded 64-byte report.
 */
void usb_feature_report_33_encode(const UsbFeatureReport33State *state,
                                  uint8_t output[USB_DEVICE_REPORT_SIZE]);

/**
 * @brief Encodes native feature report 36.
 *
 * Publishes the fixed presentation code and current tuning-menu page.
 *
 * @param[in] page Current tuning-menu page identifier.
 * @param[out] output Encoded 64-byte report.
 */
void usb_feature_report_36_encode(uint8_t page, uint8_t output[USB_DEVICE_REPORT_SIZE]);

#endif
