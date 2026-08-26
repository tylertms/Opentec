import struct

from tools.wqr_checksum import (
    CHECKSUM_OFFSET,
    METADATA_FORMAT,
    METADATA_OFFSET,
    crc32_mpeg2,
    patch_image,
)


def test_crc() -> None:
    assert crc32_mpeg2(b"123456789") == 0x0376E6E7


def test_metadata() -> None:
    image = bytes(index & 0xFF for index in range(0x500))
    patched = patch_image(image)
    metadata = struct.unpack_from(METADATA_FORMAT, patched, METADATA_OFFSET)
    checksum = struct.unpack_from("<I", patched, CHECKSUM_OFFSET)[0]
    checksum_data = patched[:CHECKSUM_OFFSET] + patched[CHECKSUM_OFFSET + 4 :]

    assert len(patched) == len(image)
    assert metadata[0] == b"wqrb"
    assert metadata[1] == 0xA000
    assert metadata[2] == len(image)
    assert metadata[3] == checksum
    assert metadata[4] == 0x0064FFFF
    assert metadata[5] == b"\xff" * 12
    assert metadata[6] == b"\x00" * 32
    assert metadata[7] == b"\xff" * 12
    assert metadata[8] == 0xFFFFFBFE
    assert checksum == crc32_mpeg2(checksum_data)


if __name__ == "__main__":
    test_crc()
    test_metadata()
