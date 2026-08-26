import struct
import sys
from pathlib import Path

APPLICATION_BASE = 0xA000
FLASH_END = 0x20000
METADATA_OFFSET = 0x3C0
CHECKSUM_OFFSET = 0x3CC
METADATA_FORMAT = "<4sIIII12s32s12sI"
METADATA_SIZE = struct.calcsize(METADATA_FORMAT)
ELF_HEADER = struct.Struct("<16sHHIIIIIHHHHHH")
ELF_PROGRAM_HEADER = struct.Struct("<IIIIIIII")
ELF_LOAD_SEGMENT = 1
ELF_EXECUTABLE = 2
ELF_ARM_MACHINE = 40


def crc32_mpeg2(data: bytes) -> int:
    crc = 0xFFFFFFFF
    for value in data:
        crc ^= value << 24
        for _ in range(8):
            crc = ((crc << 1) ^ (0x04C11DB7 if crc & 0x80000000 else 0)) & 0xFFFFFFFF
    return crc


def elf_image(executable: bytes) -> bytes:
    if len(executable) < ELF_HEADER.size:
        raise ValueError("ELF header is truncated")

    header = ELF_HEADER.unpack_from(executable)
    identity = header[0]
    program_offset = header[5]
    program_size = header[9]
    program_count = header[10]
    if (
        identity[:7] != b"\x7fELF\x01\x01\x01"
        or header[1] != ELF_EXECUTABLE
        or header[2] != ELF_ARM_MACHINE
    ):
        raise ValueError("input is not a little-endian ARM ELF32 executable")
    if program_size < ELF_PROGRAM_HEADER.size:
        raise ValueError("ELF program header is too small")
    if program_offset + program_size * program_count > len(executable):
        raise ValueError("ELF program table is truncated")

    segments: list[tuple[int, bytes]] = []
    image_end = APPLICATION_BASE
    for index in range(program_count):
        program = ELF_PROGRAM_HEADER.unpack_from(
            executable, program_offset + index * program_size
        )
        (
            segment_type,
            file_offset,
            virtual_address,
            physical_address,
            file_size,
            memory_size,
        ) = program[:6]
        if segment_type != ELF_LOAD_SEGMENT or file_size == 0:
            continue
        if file_size > memory_size or file_offset + file_size > len(executable):
            raise ValueError("ELF load segment is invalid")

        load_address = physical_address or virtual_address
        segment_end = load_address + file_size
        if segment_end <= APPLICATION_BASE or load_address >= FLASH_END:
            continue
        if segment_end > FLASH_END:
            raise ValueError("ELF load segment exceeds application flash")

        image_address = max(load_address, APPLICATION_BASE)
        source_offset = file_offset + image_address - load_address
        segment_data = executable[source_offset : file_offset + file_size]
        segments.append((image_address, segment_data))
        image_end = max(image_end, segment_end)

    if not segments:
        raise ValueError("ELF contains no application flash data")

    image = bytearray(b"\xff" * (image_end - APPLICATION_BASE))
    for address, data in segments:
        offset = address - APPLICATION_BASE
        image[offset : offset + len(data)] = data
    return bytes(image)


def patch_image(image: bytes) -> bytes:
    if len(image) < METADATA_OFFSET + METADATA_SIZE:
        raise ValueError("image does not contain the metadata region")
    if len(image) > FLASH_END - APPLICATION_BASE:
        raise ValueError("image exceeds application flash")

    patched = bytearray(image)
    metadata = struct.pack(
        METADATA_FORMAT,
        b"wqrb",
        APPLICATION_BASE,
        len(patched),
        0,
        0x0064FFFF,
        b"\xff" * 12,
        b"\x00" * 32,
        b"\xff" * 12,
        0xFFFFFBFE,
    )
    patched[METADATA_OFFSET : METADATA_OFFSET + METADATA_SIZE] = metadata
    checksum_data = bytes(patched[:CHECKSUM_OFFSET] + patched[CHECKSUM_OFFSET + 4 :])
    struct.pack_into("<I", patched, CHECKSUM_OFFSET, crc32_mpeg2(checksum_data))
    return bytes(patched)


def main() -> None:
    if len(sys.argv) != 3:
        raise SystemExit(f"usage: {Path(sys.argv[0]).name} INPUT OUTPUT")

    source = Path(sys.argv[1])
    destination = Path(sys.argv[2])
    source_data = source.read_bytes()
    image = (
        elf_image(source_data) if source_data.startswith(b"\x7fELF") else source_data
    )
    destination.write_bytes(patch_image(image))


if __name__ == "__main__":
    main()
