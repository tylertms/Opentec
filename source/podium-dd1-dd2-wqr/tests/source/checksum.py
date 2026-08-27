import importlib.util
import struct
import sys

APPLICATION_BASE = 0xA000
FLASH_LIMIT = 0x20000
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


def source_image(checksum, size=0x420):
    image = bytearray(b"\xff" * size)
    handler = APPLICATION_BASE + 1
    struct.pack_into(
        "<16I", image, checksum.METADATA_OFFSET, *([handler] * 15), 0xFFFFFFFF
    )
    start = checksum.FLASH_CONFIGURATION_OFFSET
    image[start : start + len(VENDOR_FLASH_CONFIGURATION)] = VENDOR_FLASH_CONFIGURATION
    return bytes(image)


def update_checksum(checksum, image):
    checksum_data = (
        image[: checksum.CHECKSUM_OFFSET] + image[checksum.CHECKSUM_OFFSET + 4 :]
    )
    struct.pack_into(
        "<I", image, checksum.CHECKSUM_OFFSET, checksum.crc32_mpeg2(checksum_data)
    )


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
    assert checksum.FIRMWARE_VERSION == FIRMWARE_VERSION
    assert checksum.VENDOR_FLASH_CONFIGURATION == VENDOR_FLASH_CONFIGURATION
    assert checksum.WQR_FLASH_CONFIGURATION == WQR_FLASH_CONFIGURATION

    source = source_image(checksum)
    packaged = checksum.add_checksum(source)

    checksum.validate_checksum(packaged)
    assert checksum.crc32_mpeg2(b"123456789") == 0x0376E6E7
    assert source[checksum.METADATA_OFFSET : checksum.METADATA_OFFSET + 4] != b"wqrb"

    for offset, preserve_checksum in (
        (checksum.METADATA_OFFSET, True),
        (checksum.METADATA_OFFSET + 4, True),
        (checksum.METADATA_OFFSET + 8, True),
        (checksum.CHECKSUM_OFFSET, False),
        (checksum.METADATA_OFFSET + 16, True),
        (checksum.METADATA_OFFSET + 20, True),
        (checksum.METADATA_OFFSET + 32, True),
        (checksum.FLASH_CONFIGURATION_OFFSET, True),
    ):
        corrupted = corrupt(checksum, packaged, offset, preserve_checksum)
        assert_rejected(
            lambda corrupted=corrupted: checksum.validate_checksum(corrupted)
        )

    maximum_source = source_image(checksum, FLASH_LIMIT - APPLICATION_BASE)
    maximum_packaged = checksum.add_checksum(maximum_source)
    checksum.validate_checksum(maximum_packaged)

    assert_rejected(
        lambda: checksum.validate_layout(source[: checksum.FLASH_CONFIGURATION_OFFSET])
    )
    assert_rejected(
        lambda: checksum.validate_layout(bytes(FLASH_LIMIT - APPLICATION_BASE + 1))
    )

    occupied = bytearray(source)
    occupied[checksum.METADATA_OFFSET : checksum.FLASH_CONFIGURATION_OFFSET] = bytes(64)
    assert_rejected(lambda: checksum.validate_layout(bytes(occupied)))


if __name__ == "__main__":
    main()
