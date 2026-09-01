#include "force_feedback/script_store.h"

#include <stddef.h>
#include <stdint.h>

/**
 * @brief Wire identifiers and offsets used by script uploads.
 *
 * The values describe the upload packet opcode and its slot, size, chunk, and data fields.
 */
enum {
    SCRIPT_UPLOAD_OPCODE = 0x0d,    /**< Script upload packet opcode. */
    SCRIPT_UPLOAD_SLOT_OFFSET = 4,  /**< Upload slot-index field offset. */
    SCRIPT_UPLOAD_SIZE_OFFSET = 5,  /**< Upload script-size field offset. */
    SCRIPT_UPLOAD_CHUNK_OFFSET = 7, /**< Upload chunk-offset field offset. */
    SCRIPT_UPLOAD_DATA_OFFSET = 9,  /**< First upload data byte offset. */
};

/**
 * @brief Reads a little-endian 16-bit upload field.
 *
 * Combines two consecutive bytes with the least significant byte first.
 *
 * @param[in] data Two-byte field.
 * @return The decoded unsigned value.
 */
static uint16_t read_u16(const uint8_t *data) {
    return (uint16_t)data[0] | (uint16_t)((uint16_t)data[1] << 8);
}

/**
 * @brief Opens space inside the shared script buffer.
 *
 * Copies the selected half-open byte range backward so overlapping source and destination ranges
 * remain intact.
 *
 * @param[in,out] data Shared script buffer.
 * @param[in] start First byte to move.
 * @param[in] end Offset following the last byte to move.
 * @param[in] distance Number of bytes to open before the range.
 */
static void move_right(uint8_t *data, uint16_t start, uint16_t end, uint16_t distance) {
    if (distance == 0) {
        return;
    }
    for (uint16_t index = end; index > start; index--) {
        data[index + distance - 1] = data[index - 1];
    }
}

/**
 * @brief Closes space inside the shared script buffer.
 *
 * Copies the selected half-open byte range forward so overlapping source and destination ranges
 * remain intact.
 *
 * @param[in,out] data Shared script buffer.
 * @param[in] start First byte to move.
 * @param[in] end Offset following the last byte to move.
 * @param[in] distance Number of bytes to remove before the range.
 */
static void move_left(uint8_t *data, uint16_t start, uint16_t end, uint16_t distance) {
    if (distance == 0) {
        return;
    }
    for (uint16_t index = start; index < end; index++) {
        data[index - distance] = data[index];
    }
}

void force_feedback_script_store_init(ForceFeedbackScriptStore *store) {
    if (store != NULL) {
        *store = (ForceFeedbackScriptStore){0};
    }
}

bool force_feedback_script_store_upload(ForceFeedbackScriptStore *store,
                                        ForceFeedbackScriptSlot *runtime_slots,
                                        const uint8_t *packet, size_t length) {
    if (store == NULL || runtime_slots == NULL || packet == NULL ||
        length != FORCE_FEEDBACK_SCRIPT_PACKET_SIZE || packet[0] != SCRIPT_UPLOAD_OPCODE) {
        return false;
    }

    uint8_t slot_index = packet[SCRIPT_UPLOAD_SLOT_OFFSET];
    uint16_t script_size = read_u16(&packet[SCRIPT_UPLOAD_SIZE_OFFSET]);
    uint16_t chunk_offset = read_u16(&packet[SCRIPT_UPLOAD_CHUNK_OFFSET]);
    if (slot_index >= FORCE_FEEDBACK_SCRIPT_SLOT_COUNT ||
        script_size > FORCE_FEEDBACK_SCRIPT_STORAGE_SIZE) {
        return false;
    }

    ForceFeedbackScriptStorageSlot *slot = &store->slots[slot_index];
    if (chunk_offset == 0) {
        if (slot->allocated || script_size > FORCE_FEEDBACK_SCRIPT_STORAGE_SIZE - store->used) {
            return false;
        }
        uint16_t insertion = 0;
        for (uint8_t index = 0; index < slot_index; index++) {
            if (store->slots[index].allocated) {
                insertion = store->slots[index].offset + store->slots[index].size;
            }
        }
        move_right(store->data, insertion, store->used, script_size);
        for (uint8_t index = slot_index + 1; index < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; index++) {
            if (store->slots[index].allocated) {
                store->slots[index].offset += script_size;
            }
        }
        *slot = (ForceFeedbackScriptStorageSlot){
            .offset = insertion,
            .size = script_size,
            .allocated = true,
        };
        store->used += script_size;
        store->position_request_pending = false;
    } else if (!slot->allocated || slot->size != script_size || chunk_offset >= script_size) {
        return false;
    }

    uint16_t remaining = script_size - chunk_offset;
    uint16_t copied =
        remaining < FORCE_FEEDBACK_SCRIPT_CHUNK_SIZE ? remaining : FORCE_FEEDBACK_SCRIPT_CHUNK_SIZE;
    for (uint16_t index = 0; index < copied; index++) {
        store->data[slot->offset + chunk_offset + index] =
            packet[SCRIPT_UPLOAD_DATA_OFFSET + index];
    }
    if (remaining <= FORCE_FEEDBACK_SCRIPT_CHUNK_SIZE) {
        runtime_slots[slot_index].state = FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE;
    }
    return true;
}

void force_feedback_script_store_compact(ForceFeedbackScriptStore *store,
                                         const ForceFeedbackScriptSlot *runtime_slots) {
    if (store == NULL || runtime_slots == NULL) {
        return;
    }

    for (uint8_t slot_index = 0; slot_index < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; slot_index++) {
        ForceFeedbackScriptStorageSlot removed = store->slots[slot_index];
        if (!removed.allocated ||
            runtime_slots[slot_index].state != FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY) {
            continue;
        }
        uint16_t end = removed.offset + removed.size;
        move_left(store->data, end, store->used, removed.size);
        store->used -= removed.size;
        for (uint16_t index = store->used; index < store->used + removed.size; index++) {
            store->data[index] = 0;
        }
        for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_SLOT_COUNT; index++) {
            if (store->slots[index].allocated && store->slots[index].offset > removed.offset) {
                store->slots[index].offset -= removed.size;
            }
        }
        store->slots[slot_index] = (ForceFeedbackScriptStorageSlot){0};
    }
}

const uint8_t *force_feedback_script_store_data(const ForceFeedbackScriptStore *store, uint8_t slot,
                                                uint16_t *size) {
    if (store == NULL || size == NULL || slot >= FORCE_FEEDBACK_SCRIPT_SLOT_COUNT ||
        !store->slots[slot].allocated) {
        return NULL;
    }
    *size = store->slots[slot].size;
    return &store->data[store->slots[slot].offset];
}
