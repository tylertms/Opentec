import importlib.util
import struct
import sys


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


def source_image(checksum):
    image = bytearray(b"\xff" * 0x420)
    handler = checksum.APPLICATION_BASE + 1
    struct.pack_into(
        "<16I", image, checksum.METADATA_OFFSET, *([handler] * 15), 0xFFFFFFFF
    )
    start = checksum.FLASH_CONFIGURATION_OFFSET
    image[start : start + len(checksum.VENDOR_FLASH_CONFIGURATION)] = (
        checksum.VENDOR_FLASH_CONFIGURATION
    )
    return bytes(image)


def main():
    checksum = load_checksum(sys.argv[1])
    source = source_image(checksum)
    packaged = checksum.add_checksum(source)

    checksum.validate_checksum(packaged)
    assert checksum.crc32_mpeg2(b"123456789") == 0x0376E6E7
    assert source[checksum.METADATA_OFFSET : checksum.METADATA_OFFSET + 4] != b"wqrb"

    for offset in (
        checksum.METADATA_OFFSET,
        checksum.METADATA_OFFSET + 4,
        checksum.METADATA_OFFSET + 8,
        checksum.CHECKSUM_OFFSET,
        checksum.METADATA_OFFSET + 16,
        checksum.METADATA_OFFSET + 20,
        checksum.METADATA_OFFSET + 32,
        checksum.FLASH_CONFIGURATION_OFFSET,
    ):
        corrupted = bytearray(packaged)
        corrupted[offset] ^= 1
        assert_rejected(
            lambda corrupted=bytes(corrupted): checksum.validate_checksum(corrupted)
        )

    assert_rejected(
        lambda: checksum.validate_layout(source[: checksum.FLASH_CONFIGURATION_OFFSET])
    )
    assert_rejected(
        lambda: checksum.validate_layout(
            bytes(checksum.FLASH_LIMIT - checksum.APPLICATION_BASE + 1)
        )
    )

    occupied = bytearray(source)
    occupied[checksum.METADATA_OFFSET : checksum.FLASH_CONFIGURATION_OFFSET] = bytes(64)
    assert_rejected(lambda: checksum.validate_layout(bytes(occupied)))


if __name__ == "__main__":
    main()
