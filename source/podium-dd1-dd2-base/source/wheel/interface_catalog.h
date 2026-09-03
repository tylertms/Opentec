#ifndef OPENTEC_BASE_WHEEL_INTERFACE_CATALOG_H
#define OPENTEC_BASE_WHEEL_INTERFACE_CATALOG_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Transfer state for host-requested attached-wheel tuning catalogs. */
typedef struct {
    uint8_t record_index;       /**< Next binary tuning-record index. */
    uint8_t page_index;         /**< Current indexed-help page. */
    uint8_t section_index;      /**< Current indexed-help section. */
    uint8_t chunk_index;        /**< Current chunk within the section. */
    bool records_pending;       /**< Whether binary tuning records remain. */
    bool configuration_pending; /**< Whether indexed-help text remains. */
} WheelInterfaceCatalog;

/**
 * @brief Initializes attached-wheel catalog transfer state.
 *
 * Clears both pending streams and resets their record, page, section, and chunk positions.
 *
 * @param[out] catalog Catalog state to initialize.
 */
void wheel_interface_catalog_init(WheelInterfaceCatalog *catalog);

/**
 * @brief Starts one host-interface catalog stream.
 *
 * Mode four marks binary tuning records pending. Mode five marks indexed help text pending. Each
 * mode resets only an exhausted stream; activation preserves midstream cursors and leaves the
 * other stream pending. Other modes do not change the transfer state.
 *
 * @param[in,out] catalog Catalog state to update.
 * @param[in] mode Host-interface presentation mode.
 * @return True when mode four or five marked a stream pending; otherwise false.
 */
bool wheel_interface_catalog_activate(WheelInterfaceCatalog *catalog, uint8_t mode);

/**
 * @brief Encodes the next pending catalog frame.
 *
 * Emits catalog frames only for legacy wheel mode 0x0e. Gives binary records priority over indexed
 * help text and advances the selected stream by one transfer frame.
 *
 * @param[in,out] catalog Catalog state to advance.
 * @param[in] wheel_mode Negotiated attached-wheel mode.
 * @param[out] frame Thirty-three-byte response frame destination.
 * @return True when a catalog frame was encoded; otherwise false.
 */
bool wheel_interface_catalog_encode_next(WheelInterfaceCatalog *catalog, uint8_t wheel_mode,
                                         uint8_t frame[33]);

#endif
