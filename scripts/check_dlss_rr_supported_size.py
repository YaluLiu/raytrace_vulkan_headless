#!/usr/bin/env python3
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def require(path: str, needle: str, message: str) -> None:
    text = (ROOT / path).read_text(encoding="utf-8")
    if needle not in text:
        raise SystemExit(f"{path}: {message}")


def main() -> None:
    require(
        "headless/dlss/dlss_rr.hpp",
        "queryOptimalSettings",
        "DlssRR should expose an NGX optimal-size query before creating DLSSD features",
    )
    require(
        "headless/dlss/dlss_rr.cpp",
        "NVSDK_NGX_Parameter_DLSSDOptimalSettingsCallback",
        "DlssRR should call the DLSSD optimal settings callback from capability parameters",
    )
    require(
        "headless/hello_vulkan.cpp",
        "queryOptimalSettings",
        "HelloVulkan should size RR render targets from NGX-supported dimensions",
    )
    require(
        "headless/dlss/dlss_rr.cpp",
        'formatDimensions("render"',
        "DLSSD feature creation failures should include dimensions for diagnosis",
    )


if __name__ == "__main__":
    main()
