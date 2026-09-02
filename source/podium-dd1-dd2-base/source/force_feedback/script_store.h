#ifndef OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_STORE_H
#define OPENTEC_BASE_FORCE_FEEDBACK_SCRIPT_STORE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "force_feedback/script_control.h"
#include "force_feedback/script_input.h"

/**
 * @brief Capacity limits for uploaded force-feedback scripts.
 *
 * Script bytes share a fixed storage buffer and uploads copy at most one fixed-size chunk per
 * packet.
 */
enum {
    FORCE_FEEDBACK_SCRIPT_STORAGE_SIZE =
        2048, /**< Total shared script-storage capacity in bytes. */
    FORCE_FEEDBACK_SCRIPT_CHUNK_SIZE =
        48, /**< Maximum script bytes copied from one upload packet. */
};

/**
 * @brief Allocation record for one script slot.
 *
 * The record identifies a script's half-open byte range in the shared storage buffer.
 */
typedef struct {
    uint16_t offset; /**< Offset of the script bytes in shared storage. */
    uint16_t size;   /**< Declared script size in bytes. */
    bool allocated;  /**< Whether this slot owns an allocated range. */
} ForceFeedbackScriptStorageSlot;

/**
 * @brief Shared storage for all uploaded force-feedback scripts.
 *
 * Scripts are stored contiguously in slot order, with allocation records tracking each script's
 * location and a flag indicating a pending position request.
 */
typedef struct {
    uint8_t data[FORCE_FEEDBACK_SCRIPT_STORAGE_SIZE]; /**< Shared script bytes. */
    ForceFeedbackScriptStorageSlot
        slots[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT]; /**< Per-slot allocations. */
    uint16_t used;                 /**< Number of bytes currently allocated in data. */
    bool position_request_pending; /**< Whether a position output request is pending. */
} ForceFeedbackScriptStore;

/**
 * @brief Initialize shared force-feedback script storage.
 *
 * Clears script bytes, allocation records, the used-byte count, and the pending position request.
 *
 * @param[out] store Script storage to initialize.
 */
void force_feedback_script_store_init(ForceFeedbackScriptStore *store);

/**
 * @brief Apply one script upload packet to shared storage.
 *
 * Reads the slot, script size, chunk offset, and up to 48 data bytes from the packet. A first chunk
 * allocates storage in slot order and moves later allocations; continuation chunks require the same
 * allocation and script size. Each selected chunk is copied, and the runtime slot becomes inactive
 * after the final chunk.
 *
 * @param[in,out] store Shared script storage and allocation records.
 * @param[in,out] runtime_slots Runtime state for all script slots.
 * @param[in] packet Complete script upload packet.
 * @param[in] length Number of available packet bytes.
 * @return true when the packet describes an accepted allocation or continuation chunk; otherwise
 * false.
 */
bool force_feedback_script_store_upload(ForceFeedbackScriptStore *store,
                                        ForceFeedbackScriptSlot *runtime_slots,
                                        const uint8_t *packet, size_t length);

/**
 * @brief Reclaim storage for runtime slots in the empty state.
 *
 * Removes empty-slot allocations, closes the resulting gaps, and adjusts later allocation offsets
 * while preserving the remaining script bytes.
 *
 * @param[in,out] store Shared script storage to compact.
 * @param[in,out] runtime_slots Runtime state used to identify empty slots and cleared for released
 * slots. Retained slot values are preserved while state and execution metrics are reset.
 */
void force_feedback_script_store_compact(ForceFeedbackScriptStore *store,
                                         ForceFeedbackScriptSlot *runtime_slots);

/**
 * @brief Get a stored script's bytes and declared size.
 *
 * Returns a pointer into shared storage without copying the selected script and writes its declared
 * size when the slot is allocated.
 *
 * @param[in] store Shared script storage containing slot allocations.
 * @param[in] slot Script slot index.
 * @param[out] size Destination for the selected script's declared size.
 * @return A pointer to stored bytes when the slot and size pointer are valid and allocated;
 * otherwise NULL, with size left unchanged.
 */
const uint8_t *force_feedback_script_store_data(const ForceFeedbackScriptStore *store, uint8_t slot,
                                                uint16_t *size);

#endif
