import struct
import sys
from collections.abc import Iterable
from pathlib import Path

APPLICATION_BASE = 0xA000
FLASH_LIMIT = 0x1D800
METADATA_OFFSET = 0x3C0
CHECKSUM_OFFSET = METADATA_OFFSET + 12
FLASH_CONFIGURATION_OFFSET = 0x400
METADATA = struct.Struct("<4sIIII12s32s")
FIRMWARE_VERSION = 0x01F4FFFF
VENDOR_FLASH_CONFIGURATION = b"\xff" * 12 + b"\xfe\xff\xff\xff"
MOTOR_FLASH_CONFIGURATION = b"\xff" * 12 + b"\xfe\xfb\xff\xff"
PRODUCTS = {"dd1": b"dd10", "dd2": b"dd20"}


def crc32_mpeg2(data: Iterable[int]) -> int:
    crc = 0xFFFFFFFF
    for value in data:
        crc ^= value << 24
        for _ in range(8):
            crc = ((crc << 1) ^ (0x04C11DB7 if crc & 0x80000000 else 0)) & 0xFFFFFFFF
    return crc


def validate_layout(image: bytes) -> None:
    if len(image) < FLASH_CONFIGURATION_OFFSET + len(MOTOR_FLASH_CONFIGURATION):
        raise ValueError("image does not contain the metadata region")
    if len(image) > FLASH_LIMIT - APPLICATION_BASE:
        raise ValueError("image exceeds application flash")

    metadata = image[METADATA_OFFSET:FLASH_CONFIGURATION_OFFSET]
    if not metadata.startswith(tuple(PRODUCTS.values())):
        vector_tail = struct.unpack("<16I", metadata)
        if any(vector_tail):
            raise ValueError("firmware metadata region is occupied")

    flash_configuration = image[
        FLASH_CONFIGURATION_OFFSET : FLASH_CONFIGURATION_OFFSET
        + len(MOTOR_FLASH_CONFIGURATION)
    ]
    if flash_configuration not in (
        VENDOR_FLASH_CONFIGURATION,
        MOTOR_FLASH_CONFIGURATION,
    ):
        raise ValueError("firmware flash configuration is unexpected")


def add_checksum(image: bytes, product: str) -> bytes:
    validate_layout(image)

    checksummed = bytearray(image)
    METADATA.pack_into(
        checksummed,
        METADATA_OFFSET,
        PRODUCTS[product],
        APPLICATION_BASE,
        len(checksummed),
        0,
        FIRMWARE_VERSION,
        b"\xff" * 12,
        b"\x00" * 32,
    )
    checksummed[
        FLASH_CONFIGURATION_OFFSET : FLASH_CONFIGURATION_OFFSET
        + len(MOTOR_FLASH_CONFIGURATION)
    ] = MOTOR_FLASH_CONFIGURATION
    checksum_data = bytes(
        checksummed[:CHECKSUM_OFFSET] + checksummed[CHECKSUM_OFFSET + 4 :]
    )
    struct.pack_into("<I", checksummed, CHECKSUM_OFFSET, crc32_mpeg2(checksum_data))
    return bytes(checksummed)


def validate_checksum(image: bytes, product: str) -> None:
    validate_layout(image)

    magic, base, size, checksum, version, reserved, padding = METADATA.unpack_from(
        image, METADATA_OFFSET
    )
    checksum_data = image[:CHECKSUM_OFFSET] + image[CHECKSUM_OFFSET + 4 :]
    flash_configuration = image[
        FLASH_CONFIGURATION_OFFSET : FLASH_CONFIGURATION_OFFSET
        + len(MOTOR_FLASH_CONFIGURATION)
    ]
    if (
        magic != PRODUCTS[product]
        or base != APPLICATION_BASE
        or size != len(image)
        or checksum != crc32_mpeg2(checksum_data)
        or version != FIRMWARE_VERSION
        or reserved != b"\xff" * len(reserved)
        or padding != b"\x00" * len(padding)
        or flash_configuration != MOTOR_FLASH_CONFIGURATION
    ):
        raise ValueError("firmware metadata or checksum is invalid")


def main() -> None:
    if len(sys.argv) == 4 and sys.argv[1] == "--verify":
        validate_checksum(Path(sys.argv[3]).read_bytes(), sys.argv[2])
        return
    if len(sys.argv) != 4:
        raise SystemExit(
            f"usage: {Path(sys.argv[0]).name} [--verify] PRODUCT INPUT [OUTPUT]"
        )

    product = sys.argv[1]
    source = Path(sys.argv[2])
    destination = Path(sys.argv[3])
    destination.write_bytes(add_checksum(source.read_bytes(), product))


if __name__ == "__main__":
    main()
