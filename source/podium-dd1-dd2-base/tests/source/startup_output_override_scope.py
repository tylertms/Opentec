import re
from pathlib import Path

SOURCE_PATH = Path(__file__).resolve().parents[2] / "source" / "firmware.c"
SOURCE = SOURCE_PATH.read_text(encoding="utf-8")


def function_body(name):
    match = re.search(
        r"\b" + re.escape(name) + r"\s*\([^;{}]*\)\s*\{",
        SOURCE,
        re.DOTALL,
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


def test_startup_override_uses_completion_service():
    body = function_body("apply_motor_startup_output_override")
    assert body.count("motor_startup_output_override_write(identity)") == 1
    assert "platform_aux_bus_start_write" not in body
    assert "for (;;)" not in body
    failure = body.index("if (!applied)")
    assert body.index("runtime_tuning_profile.drift_compensation = override->drift_mode", failure) > failure
    assert body.index("runtime_tuning_profile.natural_damper = override->natural_damper", failure) > failure


if __name__ == "__main__":
    test_startup_override_uses_completion_service()
