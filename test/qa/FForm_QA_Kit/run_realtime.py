#!/usr/bin/env python3
"""Realtime-insert QA: fixed frame count, integrity counters, and pitch checks."""
from __future__ import annotations

import argparse
import json
import subprocess
import sys
from fractions import Fraction
from pathlib import Path

import soundfile as sf

sys.path.insert(0, str(Path(__file__).parent))
from fform_qa import evaluate

TIME_RATIOS = ("0.5", "24/25", "1.0", "25/24", "1.5", "2.0")
PITCHES = (0.0, 3.0, -3.0, 12.0)
BLOCK_SIZES = (1, 64, 127, 256, 511, 512, 1024)


def safe(value: float) -> str:
    return ("minus" if value < 0 else "plus") + str(abs(value)).replace(".", "_")


def render(renderer: Path, source: Path, output: Path, diagnostics: Path,
           ratio: str, pitch: float, block: int, random_partitions: bool,
           enabled: bool = True) -> None:
    command = [
        str(renderer), "--input", str(source), "--output", str(output),
        "--diagnostics", str(diagnostics), "--time-ratio", str(float(Fraction(ratio))),
        "--pitch-semitones", str(pitch), "--block-size", str(block),
    ]
    if random_partitions:
        command.append("--random-partitions")
    if not enabled:
        command.append("--disabled")
    completed = subprocess.run(command, text=True, capture_output=True)
    if completed.returncode != 0:
        raise RuntimeError(completed.stderr or completed.stdout)


def check_integrity(diagnostics: dict, input_frames: int, output_frames: int) -> list[str]:
    failures = []
    if output_frames != input_frames:
        failures.append(f"duration {output_frames} != {input_frames}")
    if diagnostics["host_input_frames"] != input_frames:
        failures.append("host input frame count mismatch")
    if diagnostics["direct_input_frames"] != input_frames:
        failures.append("direct input frame count mismatch")
    if diagnostics["engine_requested_input_frames"] != input_frames:
        failures.append("requested input frame count mismatch")
    if diagnostics["engine_consumed_input_frames"] != input_frames:
        failures.append("consumed input frame count mismatch")
    if diagnostics["engine_produced_output_frames"] != input_frames:
        failures.append("produced output frame count mismatch")
    for key in ("fifo_rejected_frames", "underflow_frames", "overflow_events", "non_finite_input_frames"):
        if diagnostics[key] != 0:
            failures.append(f"{key}={diagnostics[key]}")
    return failures


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("--renderer", type=Path, required=True)
    parser.add_argument("--kit", type=Path, default=Path(__file__).parent)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--commit", required=True)
    args = parser.parse_args()

    kit = args.kit.resolve()
    stimuli = kit / "stimuli"
    render_root = args.out / "renders"
    report_root = args.out / "reports"
    render_root.mkdir(parents=True, exist_ok=True)
    report_root.mkdir(parents=True, exist_ok=True)
    sync_source = stimuli / "sync_mono.wav"
    tone_source = stimuli / "tone_440_mono.wav"
    input_data, sample_rate = sf.read(sync_source, always_2d=True)
    input_frames = len(input_data)
    results = []

    for ratio in TIME_RATIOS:
        for pitch in PITCHES:
            for block in BLOCK_SIZES:
                for random_partitions in (False, True):
                    suffix = "random" if random_partitions else "fixed"
                    label = f"sync_t{ratio.replace('/', '_')}_p{safe(pitch)}_b{block}_{suffix}"
                    output = render_root / f"{label}.wav"
                    diagnostics_file = report_root / f"{label}.json"
                    render(args.renderer, sync_source, output, diagnostics_file,
                           ratio, pitch, block, random_partitions)
                    diagnostics = json.loads(diagnostics_file.read_text()) ["diagnostics"]
                    _, output_rate = sf.read(output, always_2d=True)
                    failures = check_integrity(diagnostics, input_frames, len(sf.read(output, always_2d=True)[0]))
                    results.append({"label": label, "ratio": ratio, "pitch": pitch,
                                    "block": block, "random": random_partitions,
                                    "failures": failures, "diagnostics": diagnostics,
                                    "sample_rate": output_rate})

    for pitch in PITCHES:
        label = f"tone_pitch_{safe(pitch)}"
        output = render_root / f"{label}.wav"
        diagnostics_file = report_root / f"{label}.diagnostics.json"
        render(args.renderer, tone_source, output, diagnostics_file, "1.0", pitch, 512, True)
        diagnostics = json.loads(diagnostics_file.read_text())["diagnostics"]
        _, output_rate = sf.read(output, always_2d=True)
        failures = check_integrity(diagnostics, len(sf.read(tone_source, always_2d=True)[0]),
                                   len(sf.read(output, always_2d=True)[0]))
        pitch_result = evaluate(tone_source, output, "tone", 1.0, pitch)
        if not pitch_result["criteria"]["pitch"]["pass"]:
            failures.append(f"pitch_error_cents={pitch_result['metrics'].get('pitch_error_cents')}")
        results.append({"label": label, "ratio": "1.0", "pitch": pitch, "block": 512,
                        "random": True, "failures": failures, "diagnostics": diagnostics,
                        "pitch_result": pitch_result,
                        "sample_rate": output_rate})

    summary = {
        "commit": args.commit,
        "mode": "realtime_insert_fixed_output",
        "cases": len(results),
        "passed": sum(not item["failures"] for item in results),
        "failed": sum(bool(item["failures"]) for item in results),
        "results": results,
    }
    (report_root / "realtime_matrix.json").write_text(json.dumps(summary, indent=2) + "\n")
    print(json.dumps({key: summary[key] for key in ("cases", "passed", "failed")}, indent=2))
    return 0 if summary["failed"] == 0 else 1


if __name__ == "__main__":
    raise SystemExit(main())
