#!/bin/sh
set -eu

repo_root=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
simulator="$repo_root/build-sim/rockboxui"

if [ ! -x "$simulator" ]; then
    echo "Error: build the simulator before running the E2E reproduction." >&2
    exit 2
fi

python3 - "$simulator" "$repo_root/build-sim" <<'PY'
import csv
import json
import os
from pathlib import Path
import signal
import subprocess
import sys

simulator = Path(sys.argv[1])
build = Path(sys.argv[2])
simdisk = build / "simdisk"
states = [
    simdisk / ".crazypod" / "miniapp-data" / app / "state.bin"
    for app in ("game2048", "capability-lab")
]
saved = {
    path: path.read_bytes() if path.exists() else None
    for path in states
}
environment = os.environ.copy()
environment["CRAZYPOD_SIM_MINIAPP_REPRO"] = "1"
environment["SDL_AUDIODRIVER"] = "dummy"
target_cycles = int(
    environment.get("CRAZYPOD_SIM_MINIAPP_REPRO_CYCLES", "5")
)
timeout_seconds = int(
    environment.get("CRAZYPOD_SIM_MINIAPP_REPRO_TIMEOUT", "90")
)
output = simdisk / ".crazypod" / "repro"
generated = (
    output / "summary.json",
    output / "trace.csv",
    output / "frame-crc.csv",
    output / "environment.txt",
    output / "runner-summary.json",
)


def write_runner_summary(status, **details):
    output.mkdir(parents=True, exist_ok=True)
    payload = {"status": status, **details}
    (output / "runner-summary.json").write_text(
        json.dumps(payload, indent=2, sort_keys=True) + "\n",
        encoding="utf-8",
    )


def fail(status, exit_code, **details):
    write_runner_summary(status, **details)
    rendered = " ".join(
        f"{key}={value}" for key, value in details.items()
    )
    print(
        f"CrazyPod Mini App E2E result: {status} {rendered}",
        file=sys.stderr,
    )
    raise SystemExit(exit_code)

try:
    for path in generated:
        if path.exists():
            path.unlink()
    try:
        result = subprocess.run(
            [str(simulator)],
            cwd=build,
            env=environment,
            check=False,
            timeout=timeout_seconds,
        )
    except subprocess.TimeoutExpired:
        fail(
            "HANG",
            124,
            timeoutSeconds=timeout_seconds,
            expectedCycles=target_cycles,
        )
    if result.returncode < 0:
        signal_number = -result.returncode
        fail(
            "CRASH",
            128 + signal_number,
            signal=signal.Signals(signal_number).name,
            expectedCycles=target_cycles,
        )

    if not (output / "summary.json").exists():
        fail(
            "HARNESS_ERROR",
            2,
            processExit=result.returncode,
            reason="missing-summary",
        )
    summary = json.loads(
        (output / "summary.json").read_text(encoding="utf-8")
    )
    if result.returncode != 0 or summary["status"] != "PASS":
        reason = summary.get("reason", "unknown")
        performance_reasons = {
            "ui-heartbeat-gap",
            "2048-input-max-latency",
            "2048-input-p95-latency",
            "game2048-exit-prompt",
            "capability-lab-exit-prompt",
        }
        fail(
            "LATENCY_FAIL"
            if reason in performance_reasons
            else "FUNCTION_FAIL",
            result.returncode if result.returncode > 0 else 2,
            reason=reason,
            completedCycles=summary.get("cycles", 0),
            expectedCycles=target_cycles,
        )
    if summary["cycles"] != target_cycles:
        raise SystemExit(
            "Error: expected "
            f"{target_cycles} cycles, got {summary['cycles']}"
        )
    if summary["moveSamples"] < target_cycles * 8:
        raise SystemExit(
            "Error: too few visible 2048 move samples."
        )

    with (output / "trace.csv").open(
        encoding="utf-8", newline=""
    ) as source:
        rows = list(csv.DictReader(source))
    required = {
        "game2048-first-frame",
        "move-post",
        "exit-prompt-present",
        "game2048-exit-list-frame",
        "capability-lab-first-frame",
        "capability-lab-wheel-frame",
        "capability-lab-page-frame",
        "capability-lab-back-frame",
        "finish-pass",
    }
    phases = {row["phase"] for row in rows}
    missing = sorted(required - phases)
    if missing:
        raise SystemExit(
            f"Error: trace does not cover required phases: {missing}"
        )
    frame_crcs = {
        row["framebuffer_crc"]
        for row in rows
        if row["phase"] == "action-present"
    }
    if len(frame_crcs) < 10:
        raise SystemExit(
            "Error: E2E run did not present enough distinct frames."
        )
    if max(int(row["button_queue"]) for row in rows) <= 0:
        raise SystemExit(
            "Error: no evidence that input entered button_queue."
        )

    write_runner_summary(
        "PASS",
        cycles=summary["cycles"],
        moveSamples=summary["moveSamples"],
        moveP95Ms=summary["moveP95Ms"],
        worstCycleP95Ms=summary["worstCycleP95Ms"],
        moveMaxMs=summary["moveMaxMs"],
        heartbeatMaxMs=summary["heartbeatMaxMs"],
        allPhaseHeartbeatMaxMs=summary["allPhaseHeartbeatMaxMs"],
    )
    print(
        "CrazyPod Mini App E2E path verified: "
        f"cycles={summary['cycles']} "
        f"move_samples={summary['moveSamples']} "
        f"p95={summary['moveP95Ms']}ms "
        f"worst_cycle_p95={summary['worstCycleP95Ms']}ms "
        f"max={summary['moveMaxMs']}ms "
        f"heartbeat={summary['heartbeatMaxMs']}ms "
        f"all_phase_heartbeat={summary['allPhaseHeartbeatMaxMs']}ms"
    )
finally:
    for path, content in saved.items():
        if content is None:
            if path.exists():
                path.unlink()
        else:
            path.parent.mkdir(parents=True, exist_ok=True)
            path.write_bytes(content)
PY
