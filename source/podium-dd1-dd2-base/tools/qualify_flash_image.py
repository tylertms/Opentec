import shutil
import sys
from pathlib import Path

ALLOWED_RANGES = (
    (0x00000000, 0x00000268),
    (0x00000400, 0x00004000),
    (0x0000A000, 0x000AB000),
    (0x01F00008, 0x01F00020),
)

CONFIGURATION_WORDS = {
    0x01F00008: (0xCF, 0xFF),
    0x01F0000C: (0x78, 0xFF),
    0x01F00010: (0x5E, 0xFF),
    0x01F00014: (0x7F, 0xFF),
    0x01F00018: (0xFC, 0xFF),
    0x01F0001C: (0xDE, 0xFF),
}


class FlashImageError(ValueError):
    pass


def decode_record(line, line_number):
    if not line.startswith(":"):
        raise FlashImageError(f"line {line_number}: missing record marker")
    try:
        record = bytes.fromhex(line[1:])
    except ValueError as error:
        raise FlashImageError(
            f"line {line_number}: invalid hexadecimal data"
        ) from error
    if len(record) < 5 or len(record) != record[0] + 5:
        raise FlashImageError(f"line {line_number}: invalid record length")
    if sum(record) & 0xFF:
        raise FlashImageError(f"line {line_number}: invalid checksum")
    address = int.from_bytes(record[1:3], "big")
    return address, record[3], record[4:-1]


def load_image(path):
    memory = {}
    base = 0
    eof = False
    for line_number, raw_line in enumerate(
        path.read_text(encoding="ascii").splitlines(), 1
    ):
        line = raw_line.strip()
        if not line:
            continue
        if eof:
            raise FlashImageError(
                f"line {line_number}: data follows end-of-file record"
            )
        address, record_type, data = decode_record(line, line_number)
        if record_type == 0x00:
            for offset, value in enumerate(data):
                absolute = base + address + offset
                previous = memory.setdefault(absolute, value)
                if previous != value:
                    raise FlashImageError(
                        f"line {line_number}: conflicting data at 0x{absolute:08x}"
                    )
        elif record_type == 0x01:
            if address != 0 or data:
                raise FlashImageError(f"line {line_number}: invalid end-of-file record")
            eof = True
        elif record_type == 0x02:
            if address != 0 or len(data) != 2:
                raise FlashImageError(
                    f"line {line_number}: invalid segment-address record"
                )
            base = int.from_bytes(data, "big") << 4
        elif record_type == 0x04:
            if address != 0 or len(data) != 2:
                raise FlashImageError(
                    f"line {line_number}: invalid linear-address record"
                )
            base = int.from_bytes(data, "big") << 16
        elif record_type not in (0x03, 0x05):
            raise FlashImageError(
                f"line {line_number}: unsupported record type 0x{record_type:02x}"
            )
    if not eof:
        raise FlashImageError("missing end-of-file record")
    return memory


def address_allowed(address):
    return any(start <= address < end for start, end in ALLOWED_RANGES)


def format_ranges(addresses):
    ranges = []
    start = previous = addresses[0]
    for address in addresses[1:]:
        if address != previous + 1:
            ranges.append((start, previous))
            start = address
        previous = address
    ranges.append((start, previous))
    return ", ".join(
        f"0x{start:08x}" if start == end else f"0x{start:08x}-0x{end:08x}"
        for start, end in ranges
    )


def qualify(memory):
    disallowed = sorted(address for address in memory if not address_allowed(address))
    if disallowed:
        raise FlashImageError(
            f"data occupies disallowed ranges: {format_ranges(disallowed)}"
        )
    required_addresses = (0x00000000, 0x00000400, 0x0000A000)
    missing = [address for address in required_addresses if address not in memory]
    if missing:
        raise FlashImageError(
            f"required regions are missing at: {format_ranges(missing)}"
        )
    for address, expected in CONFIGURATION_WORDS.items():
        actual = tuple(memory.get(address + offset) for offset in range(len(expected)))
        if actual != expected:
            rendered = " ".join(
                "--" if value is None else f"{value:02x}" for value in actual
            )
            wanted = " ".join(f"{value:02x}" for value in expected)
            raise FlashImageError(
                f"configuration word at 0x{address:08x} is {rendered}, "
                f"expected {wanted}"
            )


def qualify_file(source, destination):
    destination.unlink(missing_ok=True)
    qualify(load_image(source))
    shutil.copyfile(source, destination)


def main(arguments):
    if len(arguments) != 2:
        raise FlashImageError("expected input and output HEX paths")
    qualify_file(Path(arguments[0]), Path(arguments[1]))


if __name__ == "__main__":
    try:
        main(sys.argv[1:])
    except FlashImageError as error:
        print(f"flash image rejected: {error}", file=sys.stderr)
        raise SystemExit(1) from error
