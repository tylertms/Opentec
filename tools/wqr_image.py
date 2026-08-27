import struct
import sys
from pathlib import Path

APPLICATION_BASE = 0xA000
FLASH_END = 0x20000
METADATA_OFFSET = 0x3C0
CHECKSUM_OFFSET = 0x3CC
FLASH_CONFIG_OFFSET = 0x400
METADATA_FORMAT = "<4sIIII12s32s12sI"
METADATA_SIZE = struct.calcsize(METADATA_FORMAT)


def crc32_mpeg2(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for value in data:
        crc ^= value << 24
        for _ in range(8):
            crc = ((crc << 1) ^ (0x04C11DB7 if crc & 0x80000000 else 0)) & 0xFFFFFFFF
    return crc


def package_image(image: bytes) -> bytes:
    if len(image) < METADATA_OFFSET + METADATA_SIZE:
        raise ValueError("image does not contain the metadata region")
    if len(image) > FLASH_END - APPLICATION_BASE:
        raise ValueError("image exceeds application flash")
    metadata_region = image[METADATA_OFFSET:FLASH_CONFIG_OFFSET]
    metadata_words = set(struct.unpack("<16I", metadata_region))
    metadata_words.difference_update((0, 0xFFFFFFFF))
    if metadata_region[:4] != b"wqrb" and (
        len(metadata_words) > 1
        or any(
            word & 1 == 0 or not APPLICATION_BASE <= word - 1 < FLASH_END
            for word in metadata_words
        )
    ):
        raise ValueError("firmware metadata region is occupied")

    packaged = bytearray(image)
    metadata = struct.pack(
        METADATA_FORMAT,
        b"wqrb",
        APPLICATION_BASE,
        len(packaged),
        0,
        0x0064FFFF,
        b"\xff" * 12,
        b"\x00" * 32,
        b"\xff" * 12,
        0xFFFFFBFE,
    )
    packaged[METADATA_OFFSET : METADATA_OFFSET + METADATA_SIZE] = metadata
    checksum_data = bytes(packaged[:CHECKSUM_OFFSET] + packaged[CHECKSUM_OFFSET + 4 :])
    struct.pack_into("<I", packaged, CHECKSUM_OFFSET, crc32_mpeg2(checksum_data))
    return bytes(packaged)


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit(f"usage: {Path(sys.argv[0]).name} INPUT OUTPUT")

    source = Path(sys.argv[1])
    destination = Path(sys.argv[2])
    destination.write_bytes(package_image(source.read_bytes()))


if __name__ == "__main__":
    main()
