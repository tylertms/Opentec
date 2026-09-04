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


def service_calls(body):
    return list(re.finditer(r"\bservice_profile_save\s*\(", body))


def test_startup_call_counts():
    expected_counts = {
        "run_motor_startup_centering": 1,
        "run_wheel_startup_status_transaction": 0,
        "run_wheel_startup_status_memory": 0,
        "run_wheel_startup_discovery": 1,
        "initialize_startup_console_usb": 0,
        "run_led_pattern_startup_sequence": 0,
        "initialize_startup_usb": 0,
    }
    for name, expected in expected_counts.items():
        assert len(service_calls(function_body(name))) == expected, name
    assert len(service_calls(function_body("main"))) == 2
    assert len(re.findall(r"\bservice_profile_save\s*\(", SOURCE)) == 5


def test_startup_call_order():
    centering = function_body("run_motor_startup_centering")
    assert centering.index("force_output_scale_apply") < centering.index(
        "service_profile_save"
    )

    discovery = function_body("run_wheel_startup_discovery")
    power_index = discovery.index("service_profile_save")
    scan_tail_index = discovery.index(
        "deadline_ms = platform_time_ms() + MOTOR_STARTUP_BUTTON_SCAN_FINISH_MS"
    )
    assert power_index < scan_tail_index
    assert discovery[scan_tail_index:].count("service_profile_save") == 0

    main = function_body("main")
    assert main.index("service_profile_save(0)") < main.index("initialize_startup_usb")
    assert main.index("service_profile_save(now_ms)") < main.index("usb_device_service")


def test_documented_reference_scope():
    for address in ("0x037f3a", "0x03801e", "0x0380b8", "0x0386fe"):
        assert address in SOURCE[: SOURCE.index("static void service_profile_save")]
    assert "status transaction, scan tail" in SOURCE
    assert "status-memory exchange" in SOURCE
    assert "retained-console selection" in SOURCE


def main():
    test_startup_call_counts()
    test_startup_call_order()
    test_documented_reference_scope()


if __name__ == "__main__":
    main()
