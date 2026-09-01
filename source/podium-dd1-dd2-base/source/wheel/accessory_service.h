#ifndef OPENTEC_BASE_WHEEL_ACCESSORY_SERVICE_H
#define OPENTEC_BASE_WHEEL_ACCESSORY_SERVICE_H

#include <stdbool.h>
#include <stdint.h>

#include "transfer/command.h"
#include "wheel/accessory.h"

/**
 * @brief Attached wheel accessory polling and identity state.
 *
 * The service advances status, version, and extended accessory-type reads one transport request
 * at a time while retaining the last accepted identity.
 */
/** @brief Tuning values mirrored to an attached wheel accessory. */
typedef struct {
    uint8_t sensitivity;             /**< Steering sensitivity. */
    uint8_t force_feedback_strength; /**< Overall force-feedback strength. */
    uint8_t force_feedback_scale;    /**< Force-feedback scale mode. */
    uint8_t natural_damper;          /**< Natural damper strength. */
    uint16_t natural_friction;       /**< Natural friction strength. */
    uint8_t natural_inertia;         /**< Natural inertia strength. */
    uint8_t interpolation_filter;    /**< Interpolation-filter level. */
    uint8_t force_effect_intensity;  /**< Force-effect intensity. */
    uint8_t force_effect_strength;   /**< Force-effect strength. */
    uint8_t spring_effect_strength;  /**< Spring-effect strength. */
    uint8_t damper_effect_strength;  /**< Damper-effect strength. */
} WheelAccessorySyncParameters;

typedef struct {
    WheelAccessory accessory;    /**< Last accepted accessory identity and protocol state. */
    uint8_t version_bytes[4];    /**< Four bytes retained for the little-endian version value. */
    uint8_t status_byte;         /**< Signed status byte retained from the status read. */
    uint8_t accessory_type_byte; /**< Type byte retained from an extended accessory read. */
    bool version_stage;          /**< True when the next request reads the accessory version. */
    bool accessory_type_stage;   /**< True when the next request reads the extended type byte. */
    bool request_pending; /**< True while the command transport owns an active service request. */
    uint8_t desired_parameters[15];  /**< Desired encoded accessory parameter bytes. */
    uint8_t mirrored_parameters[15]; /**< Last synchronized accessory parameter bytes. */
    uint16_t dirty_parameters;       /**< Bit mask of parameters awaiting synchronization. */
    uint8_t sync_index;              /**< Parameter currently selected for synchronization. */
    bool sync_request;               /**< True while a parameter write owns the transport. */
    bool sync_initialized;           /**< True after mandatory startup values are queued. */
    bool status_read_pending;        /**< True when the accessory status read is due. */
    bool status_read_request;        /**< True while the status read owns the transport. */
    bool output_inhibited;           /**< Latest decoded accessory force-output inhibition. */
    uint8_t status_response;         /**< Raw accessory status response byte. */
} WheelAccessoryService;

/**
 * @brief Initializes attached wheel accessory polling.
 *
 * Clears pending response storage, starts at the status stage, and initializes the logical
 * accessory identity as disconnected. A null destination is ignored.
 *
 * @param[out] service Accessory service to initialize.
 */
void wheel_accessory_service_init(WheelAccessoryService *service);

/**
 * @brief Advances attached wheel accessory identity polling.
 *
 * Completes the active request or queues the next status, version, or accessory-type read using
 * the shared command transport. A null service or transport is ignored.
 *
 * @param[in,out] service Accessory service to advance.
 * @param[in,out] transport Shared command transport used by the service.
 */
void wheel_accessory_service_run(WheelAccessoryService *service, CommandTransport *transport);

/**
 * @brief Updates accessory tuning values and marks changed values for synchronization.
 *
 * @param[in,out] service Accessory service retaining desired and mirrored values.
 * @param[in] parameters Current tuning values to mirror to the accessory.
 */
void wheel_accessory_service_configure(WheelAccessoryService *service,
                                       const WheelAccessorySyncParameters *parameters);

/**
 * @brief Reports whether the attached accessory requests force-output inhibition.
 *
 * @param[in] service Accessory service to inspect.
 * @return True when the latest accessory status inhibits force output.
 */
bool wheel_accessory_service_output_inhibited(const WheelAccessoryService *service);

/**
 * @brief Returns the latest attached wheel accessory identity.
 *
 * Returns a pointer to the identity retained inside service; the pointer remains valid while
 * service is unchanged.
 *
 * @param[in] service Accessory service to inspect.
 * @return Pointer to the retained identity, or null when service is null.
 */
const WheelAccessory *wheel_accessory_service_identity(const WheelAccessoryService *service);

#endif
