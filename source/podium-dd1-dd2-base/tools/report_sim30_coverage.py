import json
import sys
from collections import defaultdict
from pathlib import Path

from run_sim30_test import find_xc16_bin, read_owned_instructions


def main():
    if len(sys.argv) not in (2, 3):
        raise SystemExit(
            "usage: report_sim30_coverage.py COVERAGE_DIRECTORY [REFERENCE_IMAGE]"
        )
    coverage_directory = Path(sys.argv[1])
    owned = set()
    covered = set()
    complete_owned = set()
    complete_covered = set()
    profiles = []
    for result_path in coverage_directory.glob("*.json"):
        result = json.loads(result_path.read_text())
        owned.update(result["owned"])
        covered.update(result["covered"])
        if result["trace_complete"]:
            complete_owned.update(result["owned"])
            complete_covered.update(result["covered"])
        profiles.append(result)
    if len(sys.argv) == 3:
        reference_image = Path(sys.argv[2]).resolve()
        reference_owned = set(
            read_owned_instructions(
                find_xc16_bin() / "xc16-objdump.exe", reference_image
            ).values()
        )
        owned = reference_owned
        covered.intersection_update(reference_owned)
        complete_owned.intersection_update(reference_owned)
        complete_covered.intersection_update(reference_owned)
    missing = owned - covered
    missing_by_file = defaultdict(int)
    for instruction in missing:
        missing_by_file[instruction.rsplit(":", 2)[0]] += 1
    percentage = 100.0 * len(covered) / len(owned) if owned else 0.0
    complete_percentage = (
        100.0 * len(complete_covered) / len(complete_owned) if complete_owned else 0.0
    )
    complete_traces = sum(bool(item["trace_complete"]) for item in profiles)
    executed_instructions = sum(item["instructions_executed"] or 0 for item in profiles)
    print(f"Images: {len(profiles)}")
    print(f"Complete traces: {complete_traces}/{len(profiles)}")
    print(
        "Complete-trace owned instructions: "
        f"{len(complete_covered)}/{len(complete_owned)} "
        f"({complete_percentage:.2f}%)"
    )
    print(
        "All observed owned instructions: "
        f"{len(covered)}/{len(owned)} "
        f"({percentage:.2f}% lower bound)"
    )
    print(f"Executed instructions: {executed_instructions}")
    print(f"Execution cycles: {sum(item['cycles'] or 0 for item in profiles)}")
    print(f"Instruction stalls: {sum(item['stalls'] or 0 for item in profiles)}")
    ranked_missing = sorted(
        missing_by_file.items(), key=lambda item: (-item[1], item[0])
    )
    for source, count in ranked_missing[:20]:
        print(f"{count:5} {source}")
    if len(ranked_missing) > 20:
        print(f"{len(ranked_missing) - 20:5} additional source files")
    return 0 if profiles else 1


if __name__ == "__main__":
    raise SystemExit(main())
