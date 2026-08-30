import importlib.util
import tempfile
import unittest
from pathlib import Path

MODULE_PATH = Path(__file__).parents[2] / "tools" / "qualify_flash_image.py"
SPEC = importlib.util.spec_from_file_location("qualify_flash_image", MODULE_PATH)
QUALIFIER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(QUALIFIER)


def record(address, record_type, data=b""):
    body = bytes((len(data), address >> 8, address & 0xFF, record_type)) + data
    return ":" + (body + bytes((-sum(body) & 0xFF,))).hex().upper()


def valid_image(extra_records=()):
    records = [
        record(0, 4, bytes((0, 0))),
        record(0, 0, bytes((1, 2, 3, 4))),
        record(0x0400, 0, bytes((5, 6, 7, 8))),
        record(0xA000, 0, bytes((9, 10, 11, 12))),
        *extra_records,
        record(0, 4, bytes((1, 0xF0))),
    ]
    for address, value in QUALIFIER.CONFIGURATION_WORDS.items():
        records.append(record(address & 0xFFFF, 0, bytes((*value, 0, 0))))
    records.append(record(0, 1))
    return "\n".join(records) + "\n"


class FlashImageQualificationTest(unittest.TestCase):
    def qualify(self, image):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / "input.hex"
            destination = Path(directory) / "output.hex"
            source.write_text(image, encoding="ascii")
            QUALIFIER.qualify_file(source, destination)
            return destination.read_text(encoding="ascii")

    def test_accepts_reference_memory_contract(self):
        image = valid_image()
        self.assertEqual(self.qualify(image), image)

    def test_rejects_reserved_primary_gap(self):
        image = valid_image((record(0x4000, 0, bytes((1, 2, 3, 4))),))
        with self.assertRaisesRegex(QUALIFIER.FlashImageError, "0x00004000-0x00004003"):
            self.qualify(image)

    def test_rejects_auxiliary_flash(self):
        image = valid_image(
            (
                record(0, 4, bytes((0, 0xFF))),
                record(0x8000, 0, bytes((1, 2, 3, 4))),
                record(0, 4, bytes((0, 0))),
            )
        )
        with self.assertRaisesRegex(QUALIFIER.FlashImageError, "0x00ff8000-0x00ff8003"):
            self.qualify(image)

    def test_rejects_corrupt_record(self):
        image = valid_image().replace(":0400000001020304F2", ":0400000001020304F3")
        with self.assertRaisesRegex(QUALIFIER.FlashImageError, "invalid checksum"):
            self.qualify(image)


if __name__ == "__main__":
    unittest.main()
