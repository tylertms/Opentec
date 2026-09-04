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


def test_fallback_update_compares_response_images():
    body = function_body("apply_fallback_tuning")
    snapshot = "TuningProfile previous_profile = automatic_tuning_profile;"
    apply_call = "usb_fallback_tuning_apply("
    response_call = "usb_tuning_profile_service_request_response_if_changed("
    assert body.count(snapshot) == 1
    assert body.count(response_call) == 1
    assert body.index(snapshot) < body.index(apply_call) < body.index(response_call)
    assert "&previous_profile, &automatic_tuning_profile" in body


if __name__ == "__main__":
    test_fallback_update_compares_response_images()
