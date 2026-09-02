#ifndef OPENTEC_BASE_USB_PLAYSTATION_INPUT_H
#define OPENTEC_BASE_USB_PLAYSTATION_INPUT_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Sizes and counts used by the PlayStation input report. */
enum {
    USB_PLAYSTATION_INPUT_REPORT_SIZE = 64, /**< PlayStation input report size in bytes. */
    USB_PLAYSTATION_INPUT_PEDAL_COUNT = 3,  /**< Number of encoded pedal axes. */
};

/** @brief Logical controls and axes carried by the PlayStation input report. */
typedef struct {
    uint8_t clutch_axes[2]; /**< Two controller clutch-axis values. */
    uint8_t hat;            /**< Directional-hat value from zero through eight. */
    uint16_t buttons;       /**< PlayStation button bit field. */
    uint8_t vendor_buttons; /**< Vendor button bit field. */
    uint16_t steering;      /**< Sixteen-bit steering-axis value. */
    uint16_t pedals[USB_PLAYSTATION_INPUT_PEDAL_COUNT]; /**< Sixteen-bit pedal-axis values. */
    uint8_t wheel_hat;       /**< Local H-pattern hat value before report rotation. */
    uint16_t auxiliary_axis; /**< Sixteen-bit auxiliary-axis value. */
} UsbPlaystationInputState;

/** @brief Clutch inputs used to produce the two PlayStation controller axes. */
typedef struct {
    uint8_t wheel_mode;      /**< Attached-wheel protocol mode. */
    uint8_t paddle_mode;     /**< Configured paddle mode. */
    uint8_t wheel_axes[2];   /**< Normalized attached-wheel paddle axes. */
    uint8_t adapter_axes[2]; /**< Attached-adapter clutch axes. */
    bool wheel_axis_enabled; /**< True when the attached wheel supplies its clutch axis. */
    bool adapter_connected;  /**< True when an attached adapter supplies clutch inputs. */
} UsbPlaystationClutchInput;

/** @brief Attached-wheel controls used to produce PlayStation buttons. */
typedef struct {
    uint16_t secondary_buttons;    /**< Attached-wheel secondary button bits. */
    uint16_t adapter_mode;         /**< Active attached-adapter button layout. */
    uint8_t wheel_mode;            /**< Attached-wheel protocol mode. */
    uint8_t directional_buttons;   /**< Encoded directional button bits. */
    uint8_t adapter_buttons[3];    /**< Attached-adapter button bytes. */
    uint8_t auxiliary_buttons[2];  /**< Auxiliary button bytes. */
    uint8_t auxiliary_history;     /**< Retained auxiliary button history bits. */
    uint8_t extended_buttons;      /**< Extended wheel button bits. */
    uint8_t axis_modes[2];         /**< Configured auxiliary axis modes. */
    bool adapter_connected;        /**< True when an attached adapter supplies button inputs. */
    bool hat_suppressed;           /**< True when the PlayStation hat must be neutral. */
    bool system_button_suppressed; /**< True when the PlayStation system button must be clear. */
} UsbPlaystationButtonInput;

/** @brief Retained timing state for PlayStation button mapping. */
typedef struct {
    uint32_t system_button_deadline_ms; /**< Deadline for the held system-button mapping. */
    bool system_button_hold_active;     /**< True while the system-button hold is in progress. */
} UsbPlaystationInputMapper;

/**
 * @brief Initializes PlayStation input mapping state.
 *
 * Clears the retained system-button hold state and its deadline.
 *
 * @param[out] mapper Mapping state to initialize.
 */
void usb_playstation_input_mapper_init(UsbPlaystationInputMapper *mapper);

/**
 * @brief Maps attached-wheel directional buttons to a PlayStation hat.
 *
 * Permutes the four low directional input bits through the protocol lookup table used by the
 * PlayStation report.
 *
 * @param[in] directional_buttons Encoded attached-wheel directional button bits.
 * @return PlayStation hat value from zero through eight.
 */
uint8_t usb_playstation_input_map_hat(uint8_t directional_buttons);

/**
 * @brief Maps attached-wheel clutch inputs to PlayStation axes.
 *
 * Applies the wheel-mode and adapter policy, starting with centered axes and replacing them when
 * the wheel or a connected adapter supplies clutch inputs.
 *
 * @param[out] axes Destination for two PlayStation clutch-axis bytes.
 * @param[in] input Wheel mode, paddle mode, and clutch input sources.
 */
void usb_playstation_input_map_clutch(uint8_t axes[2], const UsbPlaystationClutchInput *input);

/**
 * @brief Maps attached-wheel controls to PlayStation buttons.
 *
 * Rebuilds the hat, button, and vendor-button fields from wheel, adapter, auxiliary, suppression,
 * and held system-button inputs while retaining mapper timing state. Hat suppression clears
 * secondary bit nine before button mapping, and system-button suppression is applied before
 * mode-specific mappings can reassert a mode-owned system source.
 *
 * @param[in,out] mapper Retained system-button hold state.
 * @param[in] input Wheel, adapter, and auxiliary button sources.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] state PlayStation input state whose button fields are replaced.
 * @return True when all pointers are valid and the button fields are mapped; otherwise false.
 */
bool usb_playstation_input_map_buttons(UsbPlaystationInputMapper *mapper,
                                       const UsbPlaystationButtonInput *input, uint32_t now_ms,
                                       UsbPlaystationInputState *state);

/**
 * @brief Encodes a PlayStation input report.
 *
 * Writes report identifier one, clutch axes, packed hat and button fields, and the steering, pedal,
 * local H-pattern hat, and auxiliary axis fields into a zero-filled 64-byte report.
 *
 * @param[out] report Buffer with room for USB_PLAYSTATION_INPUT_REPORT_SIZE bytes.
 * @param[in] state Logical PlayStation input values.
 * @return True when both pointers are valid and the hat value is encodable; otherwise false.
 */
bool usb_playstation_input_encode(uint8_t report[USB_PLAYSTATION_INPUT_REPORT_SIZE],
                                  const UsbPlaystationInputState *state);

#endif
