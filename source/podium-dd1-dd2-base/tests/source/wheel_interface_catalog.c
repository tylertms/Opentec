#include <assert.h>
#include <string.h>

#include "wheel/interface_catalog.h"

enum {
    TEST_INTERFACE_CATALOG_FRAME_SIZE = 33,
    TEST_INTERFACE_CATALOG_BODY_SIZE = 32,
    TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE = 0x0e,
    TEST_INTERFACE_CATALOG_MAXIMUM_SECTION = 0x0e,
    TEST_INTERFACE_CATALOG_CONFIGURATION_REPORT_ID = 0xa6,
    TEST_INTERFACE_CATALOG_RECORD_PACKET_TYPE = 0x80,
    TEST_INTERFACE_CATALOG_MAXIMUM_EMPTY_COUNT = 5,
};

static void test_reactivation_preserves_record_cursor(void) {
    WheelInterfaceCatalog catalog;
    uint8_t frame[TEST_INTERFACE_CATALOG_FRAME_SIZE];
    wheel_interface_catalog_init(&catalog);

    assert(wheel_interface_catalog_activate(&catalog, 4));
    assert(wheel_interface_catalog_encode_next(&catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE,
                                               frame));
    assert(frame[2] == 1);
    assert(catalog.record_index == 1);

    assert(wheel_interface_catalog_activate(&catalog, 4));
    assert(catalog.record_index == 1);
    assert(wheel_interface_catalog_encode_next(&catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE,
                                               frame));
    assert(frame[2] == 2);
}

static void test_reactivation_preserves_configuration_cursor(void) {
    WheelInterfaceCatalog catalog;
    uint8_t frame[TEST_INTERFACE_CATALOG_FRAME_SIZE];
    wheel_interface_catalog_init(&catalog);

    assert(wheel_interface_catalog_activate(&catalog, 5));
    assert(wheel_interface_catalog_encode_next(&catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE,
                                               frame));
    uint8_t page_index = catalog.page_index;
    uint8_t section_index = catalog.section_index;
    uint8_t chunk_index = catalog.chunk_index;

    assert(wheel_interface_catalog_activate(&catalog, 5));
    assert(catalog.page_index == page_index);
    assert(catalog.section_index == section_index);
    assert(catalog.chunk_index == chunk_index);
}

static void test_activation_preserves_other_stream(void) {
    WheelInterfaceCatalog catalog;
    uint8_t frame[TEST_INTERFACE_CATALOG_FRAME_SIZE];
    wheel_interface_catalog_init(&catalog);

    assert(wheel_interface_catalog_activate(&catalog, 4));
    assert(wheel_interface_catalog_encode_next(&catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE,
                                               frame));
    assert(catalog.records_pending);
    assert(!catalog.configuration_pending);

    assert(wheel_interface_catalog_activate(&catalog, 5));
    assert(catalog.records_pending);
    assert(catalog.configuration_pending);
    assert(wheel_interface_catalog_encode_next(&catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE,
                                               frame));
    assert(frame[1] == TEST_INTERFACE_CATALOG_RECORD_PACKET_TYPE);
    assert(frame[2] == 2);

    for (uint8_t record = 3; record < 26; record++) {
        assert(wheel_interface_catalog_encode_next(
            &catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE, frame));
        assert(frame[1] == TEST_INTERFACE_CATALOG_RECORD_PACKET_TYPE);
        assert(frame[2] == record);
    }
    assert(wheel_interface_catalog_encode_next(&catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE,
                                               frame));
    assert(frame[1] == TEST_INTERFACE_CATALOG_RECORD_PACKET_TYPE);
    assert(frame[2] == 0);
    assert(!catalog.records_pending);
    assert(catalog.configuration_pending);
    assert(wheel_interface_catalog_encode_next(&catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE,
                                               frame));
    assert(frame[0] == TEST_INTERFACE_CATALOG_CONFIGURATION_REPORT_ID);

    assert(wheel_interface_catalog_activate(&catalog, 4));
    assert(catalog.records_pending);
    assert(catalog.configuration_pending);
    assert(wheel_interface_catalog_encode_next(&catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE,
                                               frame));
    assert(frame[1] == TEST_INTERFACE_CATALOG_RECORD_PACKET_TYPE);
    assert(frame[2] == 1);
}

static void test_reactivation_resets_exhausted_record_cursor(void) {
    WheelInterfaceCatalog catalog;
    uint8_t frame[TEST_INTERFACE_CATALOG_FRAME_SIZE];
    wheel_interface_catalog_init(&catalog);

    assert(wheel_interface_catalog_activate(&catalog, 4));
    for (uint8_t record = 1; record < 26; record++) {
        assert(wheel_interface_catalog_encode_next(
            &catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE, frame));
        assert(frame[2] == record);
    }
    assert(wheel_interface_catalog_encode_next(&catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE,
                                               frame));
    assert(frame[1] == TEST_INTERFACE_CATALOG_RECORD_PACKET_TYPE);
    assert(frame[2] == 0);
    assert(catalog.record_index == 26);
    assert(!catalog.records_pending);

    assert(wheel_interface_catalog_activate(&catalog, 4));
    assert(catalog.record_index == 0);
    assert(catalog.records_pending);
    assert(wheel_interface_catalog_encode_next(&catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE,
                                               frame));
    assert(frame[2] == 1);
}

static void test_reactivation_resets_exhausted_configuration_cursor(void) {
    WheelInterfaceCatalog catalog;
    uint8_t frame[TEST_INTERFACE_CATALOG_FRAME_SIZE];
    wheel_interface_catalog_init(&catalog);

    assert(wheel_interface_catalog_activate(&catalog, 5));
    uint16_t transfers = 0;
    while (catalog.configuration_pending) {
        assert(transfers < 512);
        assert(wheel_interface_catalog_encode_next(
            &catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE, frame));
        transfers++;
    }
    assert(transfers > 0);
    assert(catalog.page_index == 26);
    assert(catalog.section_index == 0);
    assert(catalog.chunk_index == 0);

    assert(wheel_interface_catalog_activate(&catalog, 5));
    assert(catalog.page_index == 0);
    assert(catalog.section_index == 0);
    assert(catalog.chunk_index == 0);
    assert(wheel_interface_catalog_encode_next(&catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE,
                                               frame));
    assert(frame[2] == 0);
    assert(frame[3] == 3);
}

static void test_rejects_nonlegacy_wheel_modes(void) {
    WheelInterfaceCatalog catalog;
    uint8_t frame[TEST_INTERFACE_CATALOG_FRAME_SIZE];
    wheel_interface_catalog_init(&catalog);

    assert(wheel_interface_catalog_activate(&catalog, 4));
    assert(!wheel_interface_catalog_encode_next(&catalog, 0x10, frame));
    assert(!wheel_interface_catalog_encode_next(&catalog, 0x1c, frame));
    assert(catalog.record_index == 0);
    assert(catalog.records_pending);
    assert(wheel_interface_catalog_encode_next(&catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE,
                                               frame));
    assert(frame[2] == 1);

    wheel_interface_catalog_init(&catalog);
    assert(wheel_interface_catalog_activate(&catalog, 5));
    assert(!wheel_interface_catalog_encode_next(&catalog, 0x10, frame));
    assert(!wheel_interface_catalog_encode_next(&catalog, 0x1c, frame));
    assert(catalog.page_index == 0);
    assert(catalog.section_index == 0);
    assert(catalog.chunk_index == 0);
    assert(catalog.configuration_pending);
    assert(wheel_interface_catalog_encode_next(&catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE,
                                               frame));
    assert(frame[2] == 0);
    assert(frame[3] == 3);
}

static void test_emits_empty_maximum_sections(void) {
    static const uint8_t expected_pages[TEST_INTERFACE_CATALOG_MAXIMUM_EMPTY_COUNT] = {
        16, 17, 20, 21, 22};
    WheelInterfaceCatalog catalog;
    uint8_t frame[TEST_INTERFACE_CATALOG_FRAME_SIZE];
    uint8_t empty_pages[TEST_INTERFACE_CATALOG_MAXIMUM_EMPTY_COUNT];
    uint8_t empty_count = 0;
    wheel_interface_catalog_init(&catalog);

    assert(wheel_interface_catalog_activate(&catalog, 5));
    for (uint16_t transfer = 0; transfer < 400; transfer++) {
        memset(frame, 0xa5, sizeof(frame));
        if (!wheel_interface_catalog_encode_next(
                &catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE, frame)) {
            break;
        }
        if (frame[3] != TEST_INTERFACE_CATALOG_MAXIMUM_SECTION || frame[4] != 0) {
            continue;
        }
        bool empty = true;
        for (uint8_t index = 5; index < TEST_INTERFACE_CATALOG_BODY_SIZE; index++) {
            empty = empty && frame[index] == 0;
        }
        if (empty) {
            assert(empty_count < TEST_INTERFACE_CATALOG_MAXIMUM_EMPTY_COUNT);
            empty_pages[empty_count] = frame[2];
            empty_count++;
        }
    }

    assert(empty_count == TEST_INTERFACE_CATALOG_MAXIMUM_EMPTY_COUNT);
    assert(memcmp(empty_pages, expected_pages, sizeof(expected_pages)) == 0);
    assert(!catalog.configuration_pending);
    assert(!wheel_interface_catalog_encode_next(&catalog, TEST_INTERFACE_CATALOG_LEGACY_WHEEL_MODE,
                                                frame));
}

int main(void) {
    test_reactivation_preserves_record_cursor();
    test_reactivation_preserves_configuration_cursor();
    test_activation_preserves_other_stream();
    test_reactivation_resets_exhausted_record_cursor();
    test_reactivation_resets_exhausted_configuration_cursor();
    test_rejects_nonlegacy_wheel_modes();
    test_emits_empty_maximum_sections();
    return 0;
}
