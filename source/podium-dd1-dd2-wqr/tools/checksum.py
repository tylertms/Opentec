import struct
import sys
from collections.abc import Iterable
from pathlib import Path

APPLICATION_BASE = 0xA000
FLASH_LIMIT = 0x10000
METADATA_OFFSET = 0x3C0
CHECKSUM_OFFSET = METADATA_OFFSET + 12
FLASH_CONFIGURATION_OFFSET = 0x400
METADATA = struct.Struct("<4sIIII12s32s")
FIRMWARE_VERSION = 0x0064FFFF
VENDOR_FLASH_CONFIGURATION = b"\xff" * 12 + b"\xfe\xff\xff\xff"
WQR_FLASH_CONFIGURATION = b"\xff" * 12 + b"\xfe\xfb\xff\xff"


def crc32_mpeg2(data: Iterable[int]) -> int:
    crc = 0xFFFFFFFF
    for value in data:
        crc ^= value << 24
        for _ in range(8):
            crc = ((crc << 1) ^ (0x04C11DB7 if crc & 0x80000000 else 0)) & 0xFFFFFFFF
    return crc


def validate_layout(image: bytes) -> None:
    if len(image) < FLASH_CONFIGURATION_OFFSET + len(WQR_FLASH_CONFIGURATION):
        raise ValueError("image does not contain the metadata region")
    if len(image) > FLASH_LIMIT - APPLICATION_BASE:
        raise ValueError("image exceeds application flash")

    metadata = image[METADATA_OFFSET:FLASH_CONFIGURATION_OFFSET]
    if not metadata.startswith(b"wqrb"):
        vector_tail = struct.unpack("<16I", metadata)
        default_handler = vector_tail[0]
        if (
            len(set(vector_tail[:-1])) != 1
            or vector_tail[-1] != 0xFFFFFFFF
            or default_handler & 1 == 0
            or not APPLICATION_BASE <= default_handler - 1 < FLASH_LIMIT
        ):
            raise ValueError("firmware metadata region is occupied")

    flash_configuration = image[
        FLASH_CONFIGURATION_OFFSET : FLASH_CONFIGURATION_OFFSET
        + len(WQR_FLASH_CONFIGURATION)
    ]
    if flash_configuration not in (
        VENDOR_FLASH_CONFIGURATION,
        WQR_FLASH_CONFIGURATION,
    ):
        raise ValueError("firmware flash configuration is unexpected")


def add_checksum(image: bytes) -> bytes:
    validate_layout(image)

    checksummed = bytearray(image)
    METADATA.pack_into(
        checksummed,
        METADATA_OFFSET,
        b"wqrb",
        APPLICATION_BASE,
        len(checksummed),
        0,
        FIRMWARE_VERSION,
        b"\xff" * 12,
        b"\x00" * 32,
    )
    checksummed[
        FLASH_CONFIGURATION_OFFSET : FLASH_CONFIGURATION_OFFSET
        + len(WQR_FLASH_CONFIGURATION)
    ] = WQR_FLASH_CONFIGURATION
    checksum_data = bytes(
        checksummed[:CHECKSUM_OFFSET] + checksummed[CHECKSUM_OFFSET + 4 :]
    )
    struct.pack_into("<I", checksummed, CHECKSUM_OFFSET, crc32_mpeg2(checksum_data))
    return bytes(checksummed)


def validate_checksum(image: bytes) -> None:
    validate_layout(image)

    magic, base, size, checksum, version, reserved, padding = METADATA.unpack_from(
        image, METADATA_OFFSET
    )
    checksum_data = image[:CHECKSUM_OFFSET] + image[CHECKSUM_OFFSET + 4 :]
    flash_configuration = image[
        FLASH_CONFIGURATION_OFFSET : FLASH_CONFIGURATION_OFFSET
        + len(WQR_FLASH_CONFIGURATION)
    ]
    if (
        magic != b"wqrb"
        or base != APPLICATION_BASE
        or size != len(image)
        or checksum != crc32_mpeg2(checksum_data)
        or version != FIRMWARE_VERSION
        or reserved != b"\xff" * len(reserved)
        or padding != b"\x00" * len(padding)
        or flash_configuration != WQR_FLASH_CONFIGURATION
    ):
        raise ValueError("firmware metadata or checksum is invalid")


def main() -> None:
    if len(sys.argv) == 3 and sys.argv[1] == "--verify":
        validate_checksum(Path(sys.argv[2]).read_bytes())
        return
    if len(sys.argv) != 3:
        raise SystemExit(f"usage: {Path(sys.argv[0]).name} INPUT OUTPUT")

    source = Path(sys.argv[1])
    destination = Path(sys.argv[2])
    temporary = destination.with_suffix(destination.suffix + ".tmp")
    temporary.write_bytes(add_checksum(source.read_bytes()))
    temporary.replace(destination)


if __name__ == "__main__":
    main()
