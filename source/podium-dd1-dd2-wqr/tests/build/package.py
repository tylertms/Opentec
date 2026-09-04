import struct
import sys
from pathlib import Path

APPLICATION_BASE = 0xA000
METADATA_OFFSET = 0x3C0
CHECKSUM_OFFSET = 0x3CC
FLASH_CONFIGURATION_OFFSET = 0x400
FIRMWARE_VERSION = 0x0064FFFF
WQR_FLASH_CONFIGURATION = b"\xff" * 12 + b"\xfe\xfb\xff\xff"
NORMALIZED_INTERRUPTS = (16, 31, 32, 33)


def crc32_mpeg2(data):
    crc = 0xFFFFFFFF
    for value in data:
        crc ^= value << 24
        for _ in range(8):
            crc = (
                ((crc << 1) ^ 0x04C11DB7) & 0xFFFFFFFF if crc & 0x80000000 else crc << 1
            )
    return crc


def parse_hex(path):
    memory = {}
    upper_address = 0
    start_address = None
    lines = [
        line.strip() for line in Path(path).read_text().splitlines() if line.strip()
    ]

    for index, line in enumerate(lines):
        assert line.startswith(":")
        record = bytes.fromhex(line[1:])
        length = record[0]
        address = int.from_bytes(record[1:3], "big")
        record_type = record[3]
        data = record[4:-1]
        assert len(record) == length + 5
        assert len(data) == length
        assert sum(record) & 0xFF == 0

        if record_type == 0:
            absolute = upper_address + address
            for offset, value in enumerate(data):
                assert absolute + offset not in memory
                memory[absolute + offset] = value
        elif record_type == 1:
            assert length == 0
            assert index == len(lines) - 1
        elif record_type == 4:
            assert length == 2
            assert address == 0
            upper_address = int.from_bytes(data, "big") << 16
        elif record_type == 3:
            assert length == 4
            assert address == 0
            assert start_address is None
            start_address = int.from_bytes(data, "big")
        elif record_type == 5:
            raise AssertionError("linear start records are not accepted")
        else:
            raise AssertionError(f"unsupported Intel HEX record type {record_type}")

    assert lines
    assert bytes.fromhex(lines[-1][1:])[3] == 1
    assert start_address == APPLICATION_BASE
    return memory


def main():
    binary = Path(sys.argv[2]).read_bytes()
    memory = parse_hex(sys.argv[1])
    addresses = sorted(memory)

    assert addresses == list(range(APPLICATION_BASE, APPLICATION_BASE + len(binary)))
    assert bytes(memory[address] for address in addresses) == binary

    magic, base, size, checksum, version, reserved, padding = struct.unpack_from(
        "<4sIIII12s32s", binary, METADATA_OFFSET
    )
    checksum_data = binary[:CHECKSUM_OFFSET] + binary[CHECKSUM_OFFSET + 4 :]
    assert magic == b"wqrb"
    assert base == APPLICATION_BASE
    assert size == len(binary)
    assert checksum == crc32_mpeg2(checksum_data)
    assert version == FIRMWARE_VERSION
    assert reserved == b"\xff" * len(reserved)
    assert padding == bytes(len(padding))
    assert struct.unpack_from("<I", binary, 0x2FC)[0] == 0xFFFFFFFF
    assert binary[0x300:METADATA_OFFSET] == bytes(METADATA_OFFSET - 0x300)
    default_handler = struct.unpack_from("<I", binary, 4 * 4)[0]
    for interrupt in NORMALIZED_INTERRUPTS:
        assert (
            struct.unpack_from("<I", binary, (16 + interrupt) * 4)[0] == default_handler
        )
    assert (
        binary[
            FLASH_CONFIGURATION_OFFSET : FLASH_CONFIGURATION_OFFSET
            + len(WQR_FLASH_CONFIGURATION)
        ]
        == WQR_FLASH_CONFIGURATION
    )


if __name__ == "__main__":
    main()
