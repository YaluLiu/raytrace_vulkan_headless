#!/usr/bin/env python3
"""Render a batched ``[n, h, w]`` depth image from a USD scene."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np

import robot_raster_py as rr


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--usd", required=True, type=Path)
    parser.add_argument("--cameras", type=int, default=2)
    parser.add_argument("--width", type=int, default=64)
    parser.add_argument("--height", type=int, default=48)
    parser.add_argument("--camera-path", default="")
    parser.add_argument("--plugin-search-root", default="")
    parser.add_argument(
        "--normalized",
        action="store_true",
        help="return Vulkan [0,1] depth instead of linear scene-unit depth",
    )
    args = parser.parse_args()

    provider = rr.DepthCameraProvider(
        usd_path=str(args.usd),
        camera_count=args.cameras,
        width=args.width,
        height=args.height,
        camera_path=args.camera_path,
        plugin_search_root=args.plugin_search_root,
    )

    positions = np.zeros((args.cameras, 3), dtype=np.float32)
    positions[:, 1] = 1.0
    positions[:, 2] = 3.0
    positions[:, 0] = np.linspace(-0.5, 0.5, args.cameras)

    # Camera convention: local -Z points forward and local +Y points up.
    quaternions_wxyz = np.zeros((args.cameras, 4), dtype=np.float32)
    quaternions_wxyz[:, 0] = 1.0

    depth = provider.compute_from_camera_poses(
        positions, quaternions_wxyz, linearize=not args.normalized
    )
    print(
        f"depth shape={depth.shape} dtype={depth.dtype} "
        f"contiguous={depth.flags.c_contiguous} "
        f"range=({depth.min():.6g}, {depth.max():.6g})"
    )


if __name__ == "__main__":
    main()
