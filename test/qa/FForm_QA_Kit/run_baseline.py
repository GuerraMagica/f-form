#!/usr/bin/env python3
"""Run the F-Form realtime baseline matrix against the real C++ QA renderer."""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from fractions import Fraction
from pathlib import Path

TIME_RATIOS = ("0.5", "24/25", "1.0", "25/24", "1.5", "2.0")
PITCHES = (0.0, 3.0, -3.0, 12.0)
BLOCK_SIZES = (64, 127, 256, 511, 512, 1024)


def ratio_name(value: str) -> str:
    return value.replace("/", "_")


def pitch_name(value: float) -> str:
    return ("minus" if value < 0 else "plus") + str(abs(value)).replace(".", "_")


def run_case(renderer: Path, kit: Path, render_root: Path, report_root: Path,
             source: Path, ratio: str, pitch: float, block_size: int,
             random_partitions: bool) -> dict:
    label = f"sync_mono_t{ratio_name(ratio)}_p{pitch_name(pitch)}_b{block_size}"
    if random_partitions:
        label += "_random"
    render = render_root / f"{label}.wav"
    diagnostics = report_root / f"{label}.diagnostics.json"
    qa_json = report_root / f"{label}.json"
    qa_report = report_root / f"{label}.md"

    renderer_command = [
        str(renderer),
        "--input", str(source),
        "--output", str(render),
        "--diagnostics", str(diagnostics),
        "--time-ratio", str(float(Fraction(ratio))),
        "--pitch-semitones", str(pitch),
        "--block-size", str(block_size),
    ]
    if random_partitions:
        renderer_command.append("--random-partitions")

    render_process = subprocess.run(renderer_command, text=True, capture_output=True)
    if render_process.returncode != 0:
        return {"label": label, "status": "renderer_fail", "stderr": render_process.stderr}

    qa_command = [
        sys.executable,
        str(kit / "fform_qa.py"),
        "analyze",
        "--source", str(source),
        "--render", str(render),
        "--kind", "sync",
        "--ratio", ratio,
        "--pitch-semitones", str(pitch),
        "--json", str(qa_json),
        "--report", str(qa_report),
    ]
    qa_process = subprocess.run(qa_command, text=True, capture_output=True)
    result = json.loads(qa_json.read_text(encoding="utf-8"))
    result["label"] = label
    result["random_partitions"] = random_partitions
    result["block_size"] = block_size
    result["diagnostics_file"] = str(diagnostics)
    result["render_file"] = str(render)
    result["qa_exit_code"] = qa_process.returncode
    return result


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--renderer", type=Path, required=True)
    parser.add_argument("--kit", type=Path, default=Path(__file__).parent)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()

    kit = args.kit.resolve()
    source = kit / "stimuli" / "sync_mono.wav"
    render_root = args.out / "renders"
    report_root = args.out / "reports"
    render_root.mkdir(parents=True, exist_ok=True)
    report_root.mkdir(parents=True, exist_ok=True)

    results = []
    for ratio in TIME_RATIOS:
        for pitch in PITCHES:
            for block_size in BLOCK_SIZES:
                print(f"running ratio={ratio} pitch={pitch:g} block={block_size}", flush=True)
                results.append(run_case(args.renderer, kit, render_root, report_root,
                                        source, ratio, pitch, block_size, False))
            print(f"running ratio={ratio} pitch={pitch:g} random partitions", flush=True)
            results.append(run_case(args.renderer, kit, render_root, report_root,
                                    source, ratio, pitch, 512, True))

    baseline = {
        "commit": args.commit,
        "mode": "realtime_insert_fixed_output",
        "source": str(source),
        "time_ratios": list(TIME_RATIOS),
        "pitch_semitones": list(PITCHES),
        "block_sizes": list(BLOCK_SIZES),
        "cases": len(results),
        "renderer_failures": sum(item.get("status") == "renderer_fail" for item in results),
        "qa_pass": sum(item.get("pass_all") is True for item in results),
        "qa_fail": sum(item.get("pass_all") is False for item in results),
        "results": results,
    }
    (report_root / "baseline_matrix.json").write_text(
        json.dumps(baseline, indent=2, ensure_ascii=False) + "\n", encoding="utf-8"
    )
    print(json.dumps({key: baseline[key] for key in ("cases", "renderer_failures", "qa_pass", "qa_fail")}, indent=2))
    return 0 if baseline["renderer_failures"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
