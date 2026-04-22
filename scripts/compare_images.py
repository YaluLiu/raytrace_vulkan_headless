#!/usr/bin/env python3

from __future__ import annotations

import argparse
import json
import math
from pathlib import Path

import numpy as np
from PIL import Image


def _load_rgba(path: Path) -> np.ndarray:
    with Image.open(path) as img:
        return np.asarray(img.convert("RGBA"), dtype=np.uint8)


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Compare two rendered images and report regression metrics."
    )
    parser.add_argument("--baseline", required=True, help="Baseline image path")
    parser.add_argument("--candidate", required=True, help="Candidate image path")
    parser.add_argument("--diff-output", required=True, help="Diff heatmap output path")
    parser.add_argument("--json-output", required=True, help="Metrics json output path")
    parser.add_argument("--max-mae", type=float, default=2.0, help="Max allowed MAE [0,255]")
    parser.add_argument("--max-rmse", type=float, default=8.0, help="Max allowed RMSE [0,255]")
    parser.add_argument(
        "--max-changed-ratio",
        type=float,
        default=0.03,
        help="Max allowed ratio of changed pixels [0,1]",
    )
    parser.add_argument(
        "--change-threshold",
        type=int,
        default=4,
        help="Per-pixel max channel delta treated as changed",
    )
    parser.add_argument(
        "--diff-scale",
        type=float,
        default=8.0,
        help="Visual amplification factor for diff heatmap",
    )
    args = parser.parse_args()

    baseline_path = Path(args.baseline)
    candidate_path = Path(args.candidate)
    diff_output = Path(args.diff_output)
    json_output = Path(args.json_output)

    baseline = _load_rgba(baseline_path)
    candidate = _load_rgba(candidate_path)

    if baseline.shape != candidate.shape:
        payload = {
            "baseline": str(baseline_path),
            "candidate": str(candidate_path),
            "pass": False,
            "error": "shape_mismatch",
            "baseline_shape": list(baseline.shape),
            "candidate_shape": list(candidate.shape),
        }
        json_output.parent.mkdir(parents=True, exist_ok=True)
        json_output.write_text(json.dumps(payload, indent=2), encoding="utf-8")
        print(
            f"FAIL {candidate_path.name}: shape mismatch {baseline.shape} vs {candidate.shape}"
        )
        return 2

    diff = np.abs(baseline.astype(np.int16) - candidate.astype(np.int16))
    diff_float = diff.astype(np.float32)

    mae = float(np.mean(diff_float))
    mse = float(np.mean(np.square(diff_float)))
    rmse = float(math.sqrt(mse))
    max_abs_diff = int(np.max(diff))

    per_pixel_max = np.max(diff, axis=2)
    changed_ratio = float(
        np.count_nonzero(per_pixel_max >= args.change_threshold) / per_pixel_max.size
    )

    passed = (
        mae <= args.max_mae
        and rmse <= args.max_rmse
        and changed_ratio <= args.max_changed_ratio
    )

    diff_rgb = np.clip(diff_float[:, :, :3] * args.diff_scale, 0, 255).astype(np.uint8)
    diff_output.parent.mkdir(parents=True, exist_ok=True)
    Image.fromarray(diff_rgb, mode="RGB").save(diff_output)

    payload = {
        "baseline": str(baseline_path),
        "candidate": str(candidate_path),
        "size": {
            "height": int(baseline.shape[0]),
            "width": int(baseline.shape[1]),
            "channels": int(baseline.shape[2]),
        },
        "metrics": {
            "mae": mae,
            "rmse": rmse,
            "max_abs_diff": max_abs_diff,
            "changed_ratio": changed_ratio,
        },
        "thresholds": {
            "max_mae": args.max_mae,
            "max_rmse": args.max_rmse,
            "max_changed_ratio": args.max_changed_ratio,
            "change_threshold": args.change_threshold,
        },
        "artifacts": {
            "diff_heatmap": str(diff_output),
        },
        "pass": passed,
    }
    json_output.parent.mkdir(parents=True, exist_ok=True)
    json_output.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    status = "PASS" if passed else "FAIL"
    print(
        f"{status} {candidate_path.name}: mae={mae:.4f} rmse={rmse:.4f} changed_ratio={changed_ratio:.6f}"
    )

    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
