#ifndef OPENTEC_BASE_WHEEL_PACKET_COMMON_H
#define OPENTEC_BASE_WHEEL_PACKET_COMMON_H

#include <stdint.h>

enum {
    WHEEL_PACKET_COMMON_BUTTON_COUNT = 3,
    WHEEL_PACKET_COMMON_CONTROL_COUNT = 8,
    WHEEL_PACKET_COMMON_AXIS_VALUE_COUNT = 2,
};

/** @brief Logical fields shared by the common 30-byte attached-wheel packet layouts. */
typedef struct {
    uint8_t buttons[WHEEL_PACKET_COMMON_BUTTON_COUNT];
    uint8_t axis_outputs[2];
    int8_t motion;
    uint8_t controls[WHEEL_PACKET_COMMON_CONTROL_COUNT];
    uint8_t reserved_axes[2];
    uint16_t axis_values[WHEEL_PACKET_COMMON_AXIS_VALUE_COUNT];
    uint8_t mode_buttons;
    uint8_t axis_report_enabled;
    uint8_t auxiliary_data[4];
    uint8_t report_mode;
    uint8_t reserved_report;
    uint8_t report_capabilities;
    uint8_t axis_limit;
} WheelPacketCommonInput;

#endif
