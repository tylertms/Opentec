import json
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

FAILURE_SYMBOLS = ("___assert_fail", "_abort", "__exit")
ENTRY_SYMBOL = "_main"
PC_PATTERN = re.compile(r"PC = ([0-9A-Fa-f]+)\s+[0-9A-Fa-f]+\s+(\S+)")
PROFILE_PATTERN = re.compile(
    r"Execution time: .*\((\d+) instructions.*\).*"
    r"Execution cycles: (\d+).*Instruction stalls: (\d+)",
    re.DOTALL,
)
TRACE_PATTERN = re.compile(r"^[0-9A-Fa-f]{8}\s+([0-9A-Fa-f]{6})\s", re.MULTILINE)
SOURCE_PATTERN = re.compile(r"^(.*podium-dd1-dd2-base[\\/].*\.c):(\d+)$")
INSTRUCTION_PATTERN = re.compile(
    r"^\s*([0-9A-Fa-f]+):\s+(?:[0-9A-Fa-f]{2}\s+){3}\s*(\S.*)$"
)
FUNCTION_PATTERN = re.compile(r"^([0-9A-Fa-f]+) <([^>]+)>:$")
CALL_PATTERN = re.compile(r"\br?call\s+0x([0-9A-Fa-f]+)\s+<([^>]+)>")
TRACE_WINDOW_LIMIT = 16


def shim_target(command):
    shim = Path(command).with_suffix(".shim")
    if not shim.is_file():
        return Path(command)
    match = re.search(r'^path = "(.+)"$', shim.read_text(), re.MULTILINE)
    return Path(match.group(1)) if match else Path(command)


def find_xc16_bin():
    compiler = shutil.which("xc16-gcc")
    candidates = []
    if compiler:
        candidates.append(shim_target(compiler).parent)
    candidates.extend(
        (
            Path.home() / "scoop/apps/xc16/current/bin",
            Path.home() / "scoop/apps/xc16/2.10/bin",
        )
    )
    for candidate in candidates:
        if (candidate / "sim30.exe").is_file():
            return candidate
    raise RuntimeError("sim30.exe was not found beside the XC16 toolchain")


def read_symbols(nm, image):
    result = subprocess.run(
        (nm, "-n", image),
        check=True,
        capture_output=True,
        text=True,
        errors="replace",
    )
    symbols = {}
    for line in result.stdout.splitlines():
        fields = line.split()
        if len(fields) >= 3 and fields[-1] in (*FAILURE_SYMBOLS, ENTRY_SYMBOL):
            symbols[fields[-1]] = int(fields[0], 16)
    return symbols


def read_owned_instructions(objdump, image):
    result = subprocess.run(
        (objdump, "-d", "-l", image),
        check=True,
        capture_output=True,
        text=True,
        errors="replace",
    )
    source = None
    line_number = None
    ordinal = 0
    instructions = {}
    for line in result.stdout.splitlines():
        source_match = SOURCE_PATTERN.match(line)
        if source_match:
            source = source_match.group(1).replace("\\", "/")
            line_number = int(source_match.group(2))
            ordinal = 0
            continue
        instruction_match = INSTRUCTION_PATTERN.match(line)
        if (
            not instruction_match
            or source is None
            or "/source/" not in source
            or "/source/platform/" in source
            or source.endswith("/source/firmware.c")
            or "/tests/" in source
        ):
            continue
        address = int(instruction_match.group(1), 16)
        key = f"{source}:{line_number}:{ordinal}"
        instructions[address] = key
        ordinal += 1
    return instructions


def read_test_entries(objdump, image):
    result = subprocess.run(
        (objdump, "-d", "-l", image),
        check=True,
        capture_output=True,
        text=True,
        errors="replace",
    )
    current_function = None
    main_calls = []
    test_functions = set()
    for line in result.stdout.splitlines():
        function_match = FUNCTION_PATTERN.match(line)
        if function_match:
            current_function = function_match.group(2)
            continue
        source_match = SOURCE_PATTERN.match(line)
        if (
            source_match
            and current_function
            and "/tests/" in source_match.group(1).replace("\\", "/")
        ):
            test_functions.add(current_function)
        if current_function == "_main":
            call_match = CALL_PATTERN.search(line)
            if call_match:
                main_calls.append((int(call_match.group(1), 16), call_match.group(2)))
    return [address for address, name in main_calls if name in test_functions]


def run_simulator(
    simulator, image, symbols, trace_enabled, trace_entry=None, trace_windows=1
):
    commands = ["LD dspic33epsuper", f"LC {image}"]
    commands.extend(
        f"BS 0x{symbols[name]:x}" for name in FAILURE_SYMBOLS if name in symbols
    )
    if trace_enabled and ENTRY_SYMBOL in symbols:
        commands.extend((f"BS 0x{symbols[ENTRY_SYMBOL]:x}", "RP", "E"))
        commands.append(f"BC 0x{symbols[ENTRY_SYMBOL]:x}")
        if trace_entry is not None:
            commands.append(f"PS 0x{trace_entry:x}")
        for _ in range(trace_windows):
            commands.extend(("TO", "E", "DT", "DC", "TF"))
    else:
        commands.extend(("RP", "E", "DC"))
    commands.extend(("DP", "Q"))
    environment = os.environ.copy()
    for name in (
        "ASAN_OPTIONS",
        "MALLOC_PERTURB_",
        "MESON_EXE_WRAPPER",
        "MSAN_OPTIONS",
        "UBSAN_OPTIONS",
    ):
        environment.pop(name, None)
    return subprocess.run(
        (simulator,),
        input="\n".join(commands) + "\n",
        capture_output=True,
        text=True,
        errors="replace",
        env=environment,
        timeout=60,
    )


def trace_completed(output):
    return any(
        mnemonic.lower() == "reset" for _, mnemonic in PC_PATTERN.findall(output)
    )


def collect_trace(simulator, image, symbols, trace_entry=None):
    result = run_simulator(simulator, image, symbols, True, trace_entry)
    output = result.stdout + result.stderr
    if "Trace Buffer Full" not in output:
        return output
    result = run_simulator(
        simulator,
        image,
        symbols,
        True,
        trace_entry,
        TRACE_WINDOW_LIMIT,
    )
    return result.stdout + result.stderr


def write_coverage(directory, image, traced, trace_outputs, profile_output, owned):
    profile = PROFILE_PATTERN.search(profile_output)
    primary_output = trace_outputs[0]
    payload = {
        "image": str(image),
        "owned": sorted(set(owned.values())),
        "covered": sorted({owned[address] for address in traced if address in owned}),
        "instructions_executed": int(profile.group(1)) if profile else None,
        "cycles": int(profile.group(2)) if profile else None,
        "stalls": int(profile.group(3)) if profile else None,
        "trace_records": len(traced),
        "trace_segments": len(trace_outputs),
        "complete_segments": sum(trace_completed(item) for item in trace_outputs),
        "trace_halted": "Trace Buffer Full" in primary_output,
        "trace_complete": trace_completed(primary_output),
    }
    directory.mkdir(parents=True, exist_ok=True)
    destination = directory / f"{image.stem}.json"
    destination.write_text(json.dumps(payload, indent=2) + "\n")


def main():
    if len(sys.argv) < 2:
        raise SystemExit("usage: run_sim30_test.py IMAGE [ARGUMENT ...]")
    if len(sys.argv) > 2:
        raise SystemExit("sim30 test executables do not accept arguments")
    image = Path(sys.argv[1]).resolve()
    xc16_bin = find_xc16_bin()
    symbols = read_symbols(xc16_bin / "xc16-nm.exe", image)
    if not any(name in symbols for name in FAILURE_SYMBOLS):
        return 0
    coverage_value = os.environ.get("SIM30_COVERAGE_DIR")
    owned = (
        read_owned_instructions(xc16_bin / "xc16-objdump.exe", image)
        if coverage_value
        else {}
    )
    result = run_simulator(xc16_bin / "sim30.exe", image, symbols, False)
    output = result.stdout + result.stderr
    final_pc = PC_PATTERN.findall(output)
    passed = result.returncode == 0 and final_pc and final_pc[-1][1].lower() == "reset"
    if coverage_value and passed:
        trace_output = collect_trace(xc16_bin / "sim30.exe", image, symbols)
        trace_outputs = [trace_output]
        traced = {int(address, 16) for address in TRACE_PATTERN.findall(trace_output)}
        if "Trace Buffer Full" in trace_output:
            for entry in read_test_entries(xc16_bin / "xc16-objdump.exe", image):
                segment_output = collect_trace(
                    xc16_bin / "sim30.exe", image, symbols, trace_entry=entry
                )
                trace_outputs.append(segment_output)
                traced.update(
                    int(address, 16)
                    for address in TRACE_PATTERN.findall(segment_output)
                )
        write_coverage(
            Path(coverage_value), image, traced, trace_outputs, output, owned
        )
    if not passed:
        sys.stderr.write(output)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
