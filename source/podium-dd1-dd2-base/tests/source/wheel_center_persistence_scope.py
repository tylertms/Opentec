import re
from pathlib import Path

SOURCE_PATH = Path(__file__).resolve().parents[2] / "source" / "firmware.c"
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")


def function_body(name):
    match = re.search(
        r"\b" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{", SOURCE, re.DOTALL
    )
    assert match is not None, name
    depth = 0
    for index in range(match.end() - 1, len(SOURCE)):
        if SOURCE[index] == "{":
            depth += 1
        elif SOURCE[index] == "}":
            depth -= 1
            if depth == 0:
                return SOURCE[match.end() : index]
    raise AssertionError(name)


def main():
    body = function_body("capture_current_wheel_center")
    capture = body.index("wheel_position_reference_capture")
    dirty = body.index("base_settings_persistence_mark_dirty", capture)
    save = body.index("save_base_settings", dirty)
    assert body.index("if (!motor_position_ready)") < capture
    assert "if (wheel_position_reference_capture" not in body
    assert capture < dirty < save


if __name__ == "__main__":
    main()
