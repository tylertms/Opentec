#ifndef OPENTEC_BASE_SYSTEM_NOTICE_H
#define OPENTEC_BASE_SYSTEM_NOTICE_H

#include <stdint.h>

/**
 * @brief Notice kinds rendered on the local system display.
 *
 * The notice controller uses these values to select presentation duration and to identify the
 * currently visible system condition.
 */
typedef enum {
    SYSTEM_NOTICE_NONE,                           /**< No system notice is visible. */
    SYSTEM_NOTICE_TUNING_MENU_RESET,              /**< Tuning-menu reset notice. */
    SYSTEM_NOTICE_WHEEL_CENTER_CALIBRATED,        /**< Wheel-center calibrated notice. */
    SYSTEM_NOTICE_POSITION_SENSOR_TEST_SUCCEEDED, /**< Successful position-sensor test notice. */
    SYSTEM_NOTICE_POSITION_SENSOR_TEST_STARTED,   /**< Position-sensor test started notice. */
    SYSTEM_NOTICE_POSITION_SENSOR_TEST_FAILED,    /**< Failed position-sensor test notice. */
    SYSTEM_NOTICE_TORQUE_REDUCED,                 /**< Reduced-torque notice. */
    SYSTEM_NOTICE_TORQUE_REDUCED_STEERING_WHEEL,  /**< Reduced steering-wheel torque notice. */
    SYSTEM_NOTICE_MOTOR_CALIBRATION_DISCONNECT_WHEEL, /**< Motor-calibration disconnect notice. */
    SYSTEM_NOTICE_MOTOR_CALIBRATION_UNSUPPORTED,      /**< Unsupported motor-calibration notice. */
    SYSTEM_NOTICE_MOTOR_CALIBRATION_ONGOING,          /**< Ongoing motor-calibration notice. */
    SYSTEM_NOTICE_MOTOR_CALIBRATION_COMPLETED,        /**< Completed motor-calibration notice. */
    SYSTEM_NOTICE_MOTOR_CALIBRATION_ERASED,           /**< Erased motor-calibration notice. */
    SYSTEM_NOTICE_STANDARD_TUNING_MODE,               /**< Standard tuning-mode notice. */
    SYSTEM_NOTICE_ADVANCED_TUNING_MODE,               /**< Advanced tuning-mode notice. */
    SYSTEM_NOTICE_TUNING_MODE_TRANSITION_STANDARD,    /**< Standard-mode transition notice. */
    SYSTEM_NOTICE_TUNING_MODE_TRANSITION_ADVANCED,    /**< Advanced-mode transition notice. */
    SYSTEM_NOTICE_MAXIMUM_ROTATIONS_EXCEEDED,         /**< Maximum-rotation notice. */
    SYSTEM_NOTICE_SHUTDOWN,                           /**< Shutdown notice. */
    SYSTEM_NOTICE_UNSUPPORTED_WHEEL_INVERTED,         /**< Unsupported inverted-wheel notice. */
    SYSTEM_NOTICE_UNSUPPORTED_WHEEL_OUTLINED,         /**< Unsupported outlined-wheel notice. */
    SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_ENABLED,        /**< Alternative-shifter enabled notice. */
    SYSTEM_NOTICE_ALTERNATIVE_SHIFTER_DISABLED,       /**< Alternative-shifter disabled notice. */
} SystemNoticeKind;

/**
 * @brief Maximum number of interrupted notice records retained between compactions.
 *
 * The official queue retains four total records. Because the active notice is stored separately,
 * three interrupted notices can remain below it. Appending a fifth record compacts the queue to
 * that new active notice.
 */
enum { SYSTEM_NOTICE_STACK_CAPACITY = 3 };

/**
 * @brief Current local system-notice presentation state.
 *
 * A zero deadline represents a persistent notice or no notice; timed notices store their expiry
 * time in milliseconds.
 */
typedef struct {
    SystemNoticeKind kind; /**< Currently visible notice kind. */
    uint32_t deadline_ms;  /**< Expiration time for a timed notice, or zero for persistence. */
    SystemNoticeKind stack[SYSTEM_NOTICE_STACK_CAPACITY]; /**< Interrupted notices, newest last. */
    uint8_t stack_count; /**< Number of interrupted notices retained below the active notice. */
} SystemNotice;

/**
 * @brief Initializes system notice presentation state.
 *
 * Clears the active notice and expiration deadline.
 *
 * @param[out] notice System notice state to initialize.
 */
void system_notice_init(SystemNotice *notice);

/**
 * @brief Starts presentation of a system notice.
 *
 * Stores the selected notice and assigns its type-specific expiration deadline when the notice is
 * transient.
 *
 * @param[in,out] notice System notice state to update.
 * @param[in] kind Notice to present.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void system_notice_show(SystemNotice *notice, SystemNoticeKind kind, uint32_t now_ms);

/**
 * @brief Dismisses the active notice and restores the newest interrupted notice.
 *
 * Restarts the restored notice's presentation interval. Clears the active notice when no saved
 * notice remains.
 *
 * @param[in,out] notice System notice state to update.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void system_notice_dismiss(SystemNotice *notice, uint32_t now_ms);

/**
 * @brief Dismisses one notice kind without disturbing unrelated presentation.
 *
 * Restores the newest interrupted notice when the requested kind is active. When the requested
 * kind is interrupted, removes its newest occurrence while retaining the active notice and the
 * order of all other interrupted notices.
 *
 * @param[in,out] notice System notice state to update.
 * @param[in] kind Notice kind to dismiss.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void system_notice_dismiss_kind(SystemNotice *notice, SystemNoticeKind kind, uint32_t now_ms);

/**
 * @brief Expires a completed timed system notice.
 *
 * Clears the notice after its deadline or restores the newest interrupted notice with a fresh
 * duration while leaving persistent notices unchanged.
 *
 * @param[in,out] notice System notice state to service.
 * @param[in] now_ms Current monotonic time in milliseconds.
 */
void system_notice_update(SystemNotice *notice, uint32_t now_ms);

#endif
