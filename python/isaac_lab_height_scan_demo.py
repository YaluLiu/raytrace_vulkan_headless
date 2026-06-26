#!/usr/bin/env python3
"""Minimal Isaac Lab integration sketch for robot_raster_py height scans.

Run a standalone smoke test after building the extension:

    PYTHONPATH=/path/to/build/python python3 python/isaac_lab_height_scan_demo.py \
        --usd headlessTraining/tests/fixtures/smoke_scene.usda

Inside Isaac Lab, keep one HeightScanProvider alive next to the RayCaster and
call update_isaac_lab_raycaster_hits() from the RayCaster update hook.
"""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path
from typing import Any

import numpy as np

import robot_raster_py as rr


@dataclass
class HeightScanGridParams:
    u_start: float = -0.8
    u_end: float = 0.8
    u_step: float = 0.1
    v_start: float = -0.5
    v_end: float = 0.5
    v_step: float = 0.1
    gravity_direction_ws: tuple[float, float, float] = (0.0, 0.0, -1.0)
    max_range: float = 5.0


def create_provider(
    usd_path: str | Path,
    params: HeightScanGridParams,
    *,
    width: int = 64,
    height: int = 64,
    plugin_search_root: str = "",
) -> rr.HeightScanProvider:
    provider = rr.HeightScanProvider(
        usd_path=str(usd_path),
        width=width,
        height=height,
        plugin_search_root=plugin_search_root,
    )
    provider.set_height_scan_params(
        u_start=params.u_start,
        u_end=params.u_end,
        u_step=params.u_step,
        v_start=params.v_start,
        v_end=params.v_end,
        v_step=params.v_step,
        gravity_direction_ws=params.gravity_direction_ws,
        max_range=params.max_range,
    )
    return provider


def update_isaac_lab_raycaster_hits(provider: rr.HeightScanProvider, sensor: Any, env_ids: Any = None) -> None:
    """Replace an Isaac Lab RayCaster's ray hit points with robot_raster hits.

    Expected Isaac Lab fields:
      sensor.data.pos_w: torch tensor shaped (num_envs, 3)
      sensor.data.quat_w: torch tensor shaped (num_envs, 4), quaternion order wxyz
      sensor._data.ray_hits_w: torch tensor shaped (num_envs, num_rays, 3)
      sensor.device: target torch device
    """
    import torch

    target_env_ids = slice(None) if env_ids is None else env_ids
    positions = sensor.data.pos_w[target_env_ids].detach().cpu().numpy().astype(np.float32, copy=False)
    quaternions_wxyz = sensor.data.quat_w[target_env_ids].detach().cpu().numpy().astype(np.float32, copy=False)

    hits_np = provider.compute_from_sensor_poses(positions, quaternions_wxyz)
    hits = torch.as_tensor(hits_np, device=sensor.device, dtype=sensor._data.ray_hits_w.dtype)
    sensor._data.ray_hits_w[target_env_ids] = hits


def _run_standalone_smoke(args: argparse.Namespace) -> None:
    params = HeightScanGridParams(
        u_start=args.u_start,
        u_end=args.u_end,
        u_step=args.u_step,
        v_start=args.v_start,
        v_end=args.v_end,
        v_step=args.v_step,
        max_range=args.max_range,
    )
    provider = create_provider(
        args.usd,
        params,
        width=args.width,
        height=args.height,
        plugin_search_root=args.plugin_search_root,
    )
    positions = np.array([[args.x, args.y, args.z]], dtype=np.float32)
    quaternions_wxyz = np.array([[1.0, 0.0, 0.0, 0.0]], dtype=np.float32)
    hits = provider.compute_from_sensor_poses(positions, quaternions_wxyz)
    print(f"hits shape={hits.shape} dtype={hits.dtype}")
    print(hits[0, : min(5, hits.shape[1])])


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="robot_raster_py Isaac Lab height scan demo")
    parser.add_argument("--usd", required=True, help="USD scene path used as static robot_raster geometry")
    parser.add_argument("--width", type=int, default=64, help="Offscreen render width")
    parser.add_argument("--height", type=int, default=64, help="Offscreen render height")
    parser.add_argument("--plugin-search-root", default="", help="Optional renderer plugin/resource search root")
    parser.add_argument("--u-start", type=float, default=-0.1)
    parser.add_argument("--u-end", type=float, default=0.1)
    parser.add_argument("--u-step", type=float, default=0.1)
    parser.add_argument("--v-start", type=float, default=-0.1)
    parser.add_argument("--v-end", type=float, default=0.1)
    parser.add_argument("--v-step", type=float, default=0.1)
    parser.add_argument("--max-range", type=float, default=10.0)
    parser.add_argument("--x", type=float, default=0.0)
    parser.add_argument("--y", type=float, default=0.0)
    parser.add_argument("--z", type=float, default=2.0)
    return parser.parse_args()


if __name__ == "__main__":
    _run_standalone_smoke(_parse_args())
