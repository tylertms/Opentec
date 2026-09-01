#ifndef OPENTEC_BASE_WHEEL_AXIS_OVERRIDE_H
#define OPENTEC_BASE_WHEEL_AXIS_OVERRIDE_H

#include <stdbool.h>
#include <stdint.h>

/**
 * @brief Axis override selectors, multiplex states, and paddle-clutch phases.
 *
 * The values select how attached-wheel axes are routed and how the processor advances its
 * multiplexed and calibrated-clutch state machines.
 */
enum {
    WHEEL_AXIS_OVERRIDE_MODE_NONE = 0, /**< Do not apply an axis override. */
    WHEEL_AXIS_OVERRIDE_MODE_CALIBRATED =
        1,                                  /**< Route both axes through calibrated clutch logic. */
    WHEEL_AXIS_OVERRIDE_MODE_SECONDARY = 2, /**< Route axes to the secondary destinations. */
    WHEEL_AXIS_OVERRIDE_MODE_PRIMARY = 3,   /**< Route axes to the primary destinations. */
    WHEEL_AXIS_OVERRIDE_MODE_MULTIPLEXED =
        4,                           /**< Serialize the two axes through one report axis. */
    WHEEL_AXIS_MULTIPLEX_SELECT = 0, /**< Select the next available multiplexed source. */
    WHEEL_AXIS_MULTIPLEX_X = 1,      /**< Emit the selected X source. */
    WHEEL_AXIS_MULTIPLEX_Y = 2,      /**< Emit the selected Y source. */
    WHEEL_AXIS_MULTIPLEX_INTERFACE_FIRST = 6, /**< First interface mode using axis multiplexing. */
    WHEEL_AXIS_MULTIPLEX_INTERFACE_LAST = 8,  /**< Last interface mode using axis multiplexing. */
    WHEEL_PADDLE_CLUTCH_IDLE = 0,      /**< No clutch adjustment or launch sequence is active. */
    WHEEL_PADDLE_CLUTCH_ADJUSTING = 1, /**< Bite-point adjustment is consuming controls. */
    WHEEL_PADDLE_CLUTCH_ARMED = 2, /**< Both paddles were pressed and launch release is armed. */
    WHEEL_PADDLE_CLUTCH_ACTIVE =
        3, /**< One paddle was released and bite-point scaling is active. */
};

/**
 * @brief One destination axis override.
 *
 * The enabled flag controls whether value replaces the corresponding wheel axis.
 */
typedef struct {
    bool enabled;  /**< True when value is to be applied to the destination axis. */
    uint8_t value; /**< Replacement value for the destination axis. */
} WheelAxisOverride;

/**
 * @brief Overrides for the attached-wheel pedal destinations.
 *
 * Each destination retains an independent enable flag and replacement value for the current
 * packet.
 */
typedef struct {
    WheelAxisOverride axis_5;    /**< Override for wheel axis five. */
    WheelAxisOverride axis_6;    /**< Override for wheel axis six. */
    WheelAxisOverride axis_7;    /**< Override for wheel axis seven. */
    WheelAxisOverride auxiliary; /**< Override for the auxiliary axis. */
} WheelAxisOverrides;

/**
 * @brief Persistent state for attached-wheel axis overrides.
 *
 * The processor retains multiplex selection, axis availability, paddle-clutch timing, and
 * one-shot bite-point notifications between packet calls.
 */
typedef struct {
    WheelAxisOverrides overrides;           /**< Current destination overrides. */
    uint32_t paddle_adjustment_deadline_ms; /**< Deadline for the next held paddle adjustment. */
    uint8_t multiplex_phase;                /**< Current axis-multiplex selection phase. */
    uint8_t paddle_clutch_phase;            /**< Current calibrated paddle-clutch phase. */
    bool x_available;                       /**< True when the latest X control is available. */
    bool y_available;                       /**< True when the latest Y control is available. */
    bool packet_axis_report_enabled;       /**< Latched packet-axis reporting state for CRC mode. */
    bool paddle_bite_point_report_pending; /**< True when a new bite point awaits reporting. */
    bool paddle_bite_point_commit_pending; /**< True when a released bite point awaits persistence.
                                            */
} WheelAxisOverrideProcessor;

/**
 * @brief Initializes attached-wheel axis override processing.
 *
 * Clears destination overrides, availability, pending bite-point notifications, timing, and
 * multiplexer state.
 *
 * @param[out] processor Axis override processor to initialize.
 */
void wheel_axis_override_processor_init(WheelAxisOverrideProcessor *processor);

/**
 * @brief Applies the attached-wheel axis override mode.
 *
 * Clears prior overrides, routes enabled axes to the selected destinations, and converts or
 * multiplexes the two report axes according to mode and interface.
 *
 * @param[in,out] processor Persistent override and multiplex state.
 * @param[in] mode Selected axis override mode.
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @param[in] interface_mode Current input-report interface mode.
 * @param[in] enabled True when the attached device provides axis overrides.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] bite_point_percent Active profile bite-point percentage.
 * @param[in,out] buttons Primary attached-wheel button bank.
 * @param[in,out] motion Primary attached-wheel rotary motion.
 * @param[in] x First attached-device axis value.
 * @param[in] y Second attached-device axis value.
 * @param[in,out] axes Two report axes updated in place.
 */
void wheel_axis_override_process(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                 uint8_t wheel_mode, uint8_t interface_mode, bool enabled,
                                 uint32_t now_ms, uint8_t *bite_point_percent, uint8_t *buttons,
                                 int8_t *motion, uint8_t x, uint8_t y, uint8_t axes[2]);

/**
 * @brief Applies a packet family's axis-control mode.
 *
 * Normalizes packet controls, updates persistent availability, then routes the selected override
 * mode or advances packet-axis multiplexing.
 *
 * @param[in,out] processor Persistent override, availability, and multiplex state.
 * @param[in] mode Selected axis-control mode.
 * @param[in] wheel_mode Selected attached-wheel mode.
 * @param[in] interface_mode Current input-report interface mode.
 * @param[in] axis_limit Attached-wheel axis-limit capability value.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] bite_point_percent Active profile bite-point percentage.
 * @param[in,out] buttons Primary attached-wheel button bank.
 * @param[in,out] motion Primary attached-wheel rotary motion.
 * @param[in,out] controls Eight packet control bytes updated in place.
 * @param[in,out] axes Two packet output axes updated in place when multiplexing is selected.
 */
void wheel_axis_override_process_packet(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                        uint8_t wheel_mode, uint8_t interface_mode,
                                        uint8_t axis_limit, uint32_t now_ms,
                                        uint8_t *bite_point_percent, uint8_t *buttons,
                                        int8_t *motion, uint8_t controls[8], uint8_t axes[2]);
/**
 * @brief Applies an axis-mode packet's selected override behavior.
 *
 * Maps filtered controls to the selected pedal destinations, runs calibrated clutch policy when
 * requested, or advances axis-mode multiplexing; unsupported selectors clear the overrides.
 *
 * @param[in,out] processor Persistent override, clutch, availability, and multiplex state.
 * @param[in] mode Axis-control selector carried by the packet.
 * @param[in] interface_mode Current input-report interface mode.
 * @param[in] now_ms Current monotonic time in milliseconds.
 * @param[in,out] bite_point_percent Active profile bite-point percentage.
 * @param[in,out] buttons Primary attached-wheel button bank.
 * @param[in,out] motion Primary attached-wheel rotary motion.
 * @param[in] controls Eight filtered axis-mode control bytes.
 * @param[out] axes Two packet output axes.
 */
void wheel_axis_override_process_axis_mode(WheelAxisOverrideProcessor *processor, uint8_t mode,
                                           uint8_t interface_mode, uint32_t now_ms,
                                           uint8_t *bite_point_percent, uint8_t *buttons,
                                           int8_t *motion, const uint8_t controls[8],
                                           uint8_t axes[2]);

/**
 * @brief Takes a completed wheel-side bite-point adjustment.
 *
 * Returns the current percentage once after the analog paddle leaves adjustment mode and clears
 * the one-shot persistence latch.
 *
 * @param[in,out] processor Persistent paddle-clutch state.
 * @param[in] bite_point_percent Current adjusted bite-point percentage.
 * @param[out] updated_percent Destination for the completed percentage.
 * @return true when a completed adjustment was available and copied; false otherwise.
 */
bool wheel_axis_override_take_bite_point(WheelAxisOverrideProcessor *processor,
                                         uint8_t bite_point_percent, uint8_t *updated_percent);

/**
 * @brief Takes a wheel-side bite-point report update.
 *
 * Returns the current percentage once after an accepted increment or decrement and clears the
 * one-shot report latch.
 *
 * @param[in,out] processor Persistent paddle-clutch state.
 * @param[in] bite_point_percent Current adjusted bite-point percentage.
 * @param[out] updated_percent Destination for the percentage to publish.
 * @return true when a new percentage was available and copied; false otherwise.
 */
bool wheel_axis_override_take_bite_point_report(WheelAxisOverrideProcessor *processor,
                                                uint8_t bite_point_percent,
                                                uint8_t *updated_percent);

#endif
