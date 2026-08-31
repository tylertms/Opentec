#ifndef OPENTEC_BASE_WHEEL_INTERFACE_CATALOG_H
#define OPENTEC_BASE_WHEEL_INTERFACE_CATALOG_H

#include <stdbool.h>
#include <stdint.h>

/** @brief Transfer state for host-requested attached-wheel tuning catalogs. */
typedef struct {
    uint8_t record_index;
    uint8_t page_index;
    uint8_t section_index;
    uint8_t chunk_index;
    bool records_pending;
    bool configuration_pending;
} WheelInterfaceCatalog;

void wheel_interface_catalog_init(WheelInterfaceCatalog *catalog);
bool wheel_interface_catalog_activate(WheelInterfaceCatalog *catalog, uint8_t mode);
bool wheel_interface_catalog_encode_next(WheelInterfaceCatalog *catalog, uint8_t wheel_mode,
                                         uint8_t frame[33]);

#endif
