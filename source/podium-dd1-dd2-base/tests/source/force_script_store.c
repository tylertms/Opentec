#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "force_feedback/script_store.h"

static void write_u16(uint8_t *data, uint16_t value) {
    data[0] = (uint8_t)value;
    data[1] = (uint8_t)(value >> 8);
}

static void prepare_packet(uint8_t *packet, uint8_t slot, uint16_t size, uint16_t offset,
                           uint8_t first_value) {
    memset(packet, 0, FORCE_FEEDBACK_SCRIPT_PACKET_SIZE);
    packet[0] = 0x0d;
    packet[4] = slot;
    write_u16(&packet[5], size);
    write_u16(&packet[7], offset);
    for (uint8_t index = 0; index < FORCE_FEEDBACK_SCRIPT_CHUNK_SIZE; index++) {
        packet[9 + index] = first_value + index;
    }
}

static void test_uploads_script_chunks(void) {
    ForceFeedbackScriptStore store;
    ForceFeedbackScriptSlot slots[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT] = {0};
    uint8_t packet[FORCE_FEEDBACK_SCRIPT_PACKET_SIZE];
    force_feedback_script_store_init(&store);
    store.position_request_pending = true;

    prepare_packet(packet, 3, 100, 0, 0);
    assert(force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    assert(store.used == 100);
    assert(!store.position_request_pending);
    assert(store.slots[3].allocated);
    assert(store.slots[3].offset == 0);
    assert(store.slots[3].size == 100);
    assert(slots[3].state == FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY);

    prepare_packet(packet, 3, 100, 48, 48);
    assert(force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    assert(slots[3].state == FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY);
    prepare_packet(packet, 3, 100, 96, 96);
    assert(force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    assert(slots[3].state == FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE);

    uint16_t size = 0;
    const uint8_t *script = force_feedback_script_store_data(&store, 3, &size);
    assert(script != NULL);
    assert(size == 100);
    for (uint16_t index = 0; index < size; index++) {
        assert(script[index] == (uint8_t)index);
    }
}

static void test_inserts_scripts_in_slot_order(void) {
    ForceFeedbackScriptStore store;
    ForceFeedbackScriptSlot slots[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT] = {0};
    uint8_t packet[FORCE_FEEDBACK_SCRIPT_PACKET_SIZE];
    force_feedback_script_store_init(&store);

    prepare_packet(packet, 8, 3, 0, 0x80);
    assert(force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    prepare_packet(packet, 2, 2, 0, 0x20);
    assert(force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    prepare_packet(packet, 12, 4, 0, 0xc0);
    assert(force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));

    assert(store.used == 9);
    assert(store.slots[2].offset == 0);
    assert(store.slots[8].offset == 2);
    assert(store.slots[12].offset == 5);
    assert(store.data[0] == 0x20 && store.data[1] == 0x21);
    assert(store.data[2] == 0x80 && store.data[4] == 0x82);
    assert(store.data[5] == 0xc0 && store.data[8] == 0xc3);
}

static void test_uploads_slot_fifteen(void) {
    ForceFeedbackScriptStore store;
    ForceFeedbackScriptSlot slots[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT] = {0};
    uint8_t packet[FORCE_FEEDBACK_SCRIPT_PACKET_SIZE];
    force_feedback_script_store_init(&store);

    prepare_packet(packet, 15, 4, 0, 0xa0);
    assert(force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    assert(store.slots[15].allocated);
    assert(store.slots[15].offset == 0);
    assert(store.slots[15].size == 4);
    assert(slots[15].state == FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE);
}

static void test_compacts_cleared_scripts(void) {
    ForceFeedbackScriptStore store;
    ForceFeedbackScriptSlot slots[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT] = {0};
    uint8_t packet[FORCE_FEEDBACK_SCRIPT_PACKET_SIZE];
    force_feedback_script_store_init(&store);

    prepare_packet(packet, 1, 2, 0, 0x10);
    assert(force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    prepare_packet(packet, 5, 3, 0, 0x50);
    assert(force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    prepare_packet(packet, 9, 2, 0, 0x90);
    assert(force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    slots[1].state = FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE;
    slots[5].state = FORCE_FEEDBACK_SCRIPT_SLOT_EMPTY;
    slots[9].state = FORCE_FEEDBACK_SCRIPT_SLOT_INACTIVE;

    force_feedback_script_store_compact(&store, slots);
    assert(store.used == 4);
    assert(store.slots[1].offset == 0);
    assert(!store.slots[5].allocated);
    assert(store.slots[9].offset == 2);
    assert(store.data[0] == 0x10 && store.data[1] == 0x11);
    assert(store.data[2] == 0x90 && store.data[3] == 0x91);
    assert(store.data[4] == 0 && store.data[6] == 0);
}

static void test_rejects_invalid_uploads(void) {
    ForceFeedbackScriptStore store;
    ForceFeedbackScriptSlot slots[FORCE_FEEDBACK_SCRIPT_SLOT_COUNT] = {0};
    uint8_t packet[FORCE_FEEDBACK_SCRIPT_PACKET_SIZE];
    force_feedback_script_store_init(&store);

    prepare_packet(packet, FORCE_FEEDBACK_SCRIPT_SLOT_COUNT, 1, 0, 0);
    assert(!force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    prepare_packet(packet, 0, FORCE_FEEDBACK_SCRIPT_STORAGE_SIZE + 1, 0, 0);
    assert(!force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    prepare_packet(packet, 0, 10, 1, 0);
    assert(!force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    prepare_packet(packet, 0, 10, 0, 0);
    assert(!force_feedback_script_store_upload(&store, slots, packet, sizeof(packet) - 1));
    packet[0] = 0x0c;
    assert(!force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    packet[0] = 0x0d;
    assert(force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    assert(!force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    prepare_packet(packet, 0, 11, 5, 0);
    assert(!force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    prepare_packet(packet, 0, 10, 10, 0);
    assert(!force_feedback_script_store_upload(&store, slots, packet, sizeof(packet)));
    assert(!force_feedback_script_store_upload(NULL, slots, packet, sizeof(packet)));
    assert(!force_feedback_script_store_upload(&store, NULL, packet, sizeof(packet)));
    assert(!force_feedback_script_store_upload(&store, slots, NULL, sizeof(packet)));

    uint16_t size;
    assert(force_feedback_script_store_data(&store, 1, &size) == NULL);
    assert(force_feedback_script_store_data(NULL, 0, &size) == NULL);
    assert(force_feedback_script_store_data(&store, 0, NULL) == NULL);
    force_feedback_script_store_compact(NULL, slots);
    force_feedback_script_store_compact(&store, NULL);
    force_feedback_script_store_init(NULL);
}

int main(void) {
    test_uploads_script_chunks();
    test_inserts_scripts_in_slot_order();
    test_uploads_slot_fifteen();
    test_compacts_cleared_scripts();
    test_rejects_invalid_uploads();
    return 0;
}
