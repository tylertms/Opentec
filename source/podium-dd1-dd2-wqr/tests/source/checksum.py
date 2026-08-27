import importlib.util
import struct
import sys

APPLICATION_BASE = 0xA000
FLASH_LIMIT = 0x20000
METADATA_OFFSET = 0x3C0
CHECKSUM_OFFSET = 0x3CC
FLASH_CONFIGURATION_OFFSET = 0x400
FLASH_CONFIGURATION_END = 0x410
FIRMWARE_VERSION = 0x0064FFFF
VENDOR_FLASH_CONFIGURATION = b"\xff" * 12 + b"\xfe\xff\xff\xff"
WQR_FLASH_CONFIGURATION = b"\xff" * 12 + b"\xfe\xfb\xff\xff"


def load_checksum(path):
    spec = importlib.util.spec_from_file_location("wqr_checksum", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def assert_rejected(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError("invalid firmware was accepted")


def source_image(size=0x420):
    image = bytearray(b"\xff" * size)
    handler = APPLICATION_BASE + 1
    struct.pack_into("<16I", image, METADATA_OFFSET, *([handler] * 15), 0xFFFFFFFF)
    image[
        FLASH_CONFIGURATION_OFFSET : FLASH_CONFIGURATION_OFFSET
        + len(VENDOR_FLASH_CONFIGURATION)
    ] = VENDOR_FLASH_CONFIGURATION
    return bytes(image)


def update_checksum(checksum, image):
    checksum_data = image[:CHECKSUM_OFFSET] + image[CHECKSUM_OFFSET + 4 :]
    struct.pack_into("<I", image, CHECKSUM_OFFSET, checksum.crc32_mpeg2(checksum_data))


def corrupt(checksum, image, offset, preserve_checksum=True):
    corrupted = bytearray(image)
    corrupted[offset] ^= 1
    if preserve_checksum:
        update_checksum(checksum, corrupted)
    return bytes(corrupted)


def main():
    checksum = load_checksum(sys.argv[1])
    assert checksum.APPLICATION_BASE == APPLICATION_BASE
    assert checksum.FLASH_LIMIT == FLASH_LIMIT
    assert checksum.METADATA_OFFSET == METADATA_OFFSET
    assert checksum.CHECKSUM_OFFSET == CHECKSUM_OFFSET
    assert checksum.FLASH_CONFIGURATION_OFFSET == FLASH_CONFIGURATION_OFFSET
    assert checksum.FIRMWARE_VERSION == FIRMWARE_VERSION
    assert checksum.VENDOR_FLASH_CONFIGURATION == VENDOR_FLASH_CONFIGURATION
    assert checksum.WQR_FLASH_CONFIGURATION == WQR_FLASH_CONFIGURATION
    assert (
        FLASH_CONFIGURATION_OFFSET + len(WQR_FLASH_CONFIGURATION)
        == FLASH_CONFIGURATION_END
    )

    source = source_image()
    packaged = checksum.add_checksum(source)

    checksum.validate_checksum(packaged)
    assert checksum.crc32_mpeg2(b"123456789") == 0x0376E6E7
    assert source[METADATA_OFFSET : METADATA_OFFSET + 4] != b"wqrb"
    magic, base, size, packaged_checksum, version, reserved, padding = (
        struct.unpack_from("<4sIIII12s32s", packaged, METADATA_OFFSET)
    )
    checksum_data = packaged[:CHECKSUM_OFFSET] + packaged[CHECKSUM_OFFSET + 4 :]
    assert (magic, base, size, version, reserved, padding) == (
        b"wqrb",
        APPLICATION_BASE,
        len(packaged),
        FIRMWARE_VERSION,
        b"\xff" * 12,
        bytes(32),
    )
    assert packaged_checksum == checksum.crc32_mpeg2(checksum_data)
    assert (
        packaged[
            FLASH_CONFIGURATION_OFFSET : FLASH_CONFIGURATION_OFFSET
            + len(WQR_FLASH_CONFIGURATION)
        ]
        == WQR_FLASH_CONFIGURATION
    )

    for offset, preserve_checksum in (
        (METADATA_OFFSET, True),
        (METADATA_OFFSET + 4, True),
        (METADATA_OFFSET + 8, True),
        (CHECKSUM_OFFSET, False),
        (METADATA_OFFSET + 16, True),
        (METADATA_OFFSET + 20, True),
        (METADATA_OFFSET + 32, True),
        (FLASH_CONFIGURATION_OFFSET, True),
    ):
        corrupted = corrupt(checksum, packaged, offset, preserve_checksum)
        assert_rejected(
            lambda corrupted=corrupted: checksum.validate_checksum(corrupted)
        )

    maximum_source = source_image(FLASH_LIMIT - APPLICATION_BASE)
    maximum_packaged = checksum.add_checksum(maximum_source)
    checksum.validate_checksum(maximum_packaged)

    assert_rejected(
        lambda: checksum.validate_layout(source[: FLASH_CONFIGURATION_END - 1])
    )
    checksum.validate_layout(source[:FLASH_CONFIGURATION_END])
    assert_rejected(
        lambda: checksum.validate_layout(bytes(FLASH_LIMIT - APPLICATION_BASE + 1))
    )

    handler = APPLICATION_BASE + 1
    vector_tails = (
        [handler] * 14 + [handler + 4, 0xFFFFFFFF],
        [handler] * 15 + [0],
        [APPLICATION_BASE] * 15 + [0xFFFFFFFF],
        [APPLICATION_BASE - 1] * 15 + [0xFFFFFFFF],
        [FLASH_LIMIT + 1] * 15 + [0xFFFFFFFF],
    )
    for vector_tail in vector_tails:
        occupied = bytearray(source)
        struct.pack_into("<16I", occupied, METADATA_OFFSET, *vector_tail)
        assert_rejected(
            lambda occupied=bytes(occupied): checksum.validate_layout(occupied)
        )


if __name__ == "__main__":
    main()
