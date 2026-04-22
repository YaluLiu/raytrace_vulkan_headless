#!/usr/bin/env bash

set -euo pipefail

usage() {
  cat <<'EOF'
Usage:
  bash scripts/visual_regression.sh capture
  bash scripts/visual_regression.sh check

Env overrides:
  VIS_DEMO_CMD            Demo command. Default: bash install.sh demo
  VIS_EXPECTED_IMAGES     Space-separated output images. Default: gl_0.png lidar_0.png
  VIS_RESULT_DIR          Result directory. Default: <repo>/result
  VIS_BASELINE_DIR        Baseline directory. Default: <repo>/output/visual_baseline
  VIS_CURRENT_DIR         Current capture root. Default: <repo>/output/visual_current
  VIS_REPORT_DIR          Compare report root. Default: <repo>/output/visual_report
  VIS_SKIP_RUN            Set to 1 to skip running demo and reuse existing result images
  VIS_MAX_MAE             Max MAE threshold. Default: 2.0
  VIS_MAX_RMSE            Max RMSE threshold. Default: 8.0
  VIS_MAX_CHANGED_RATIO   Max changed-pixel ratio threshold. Default: 0.03
  VIS_CHANGE_THRESHOLD    Pixel change threshold. Default: 4
  VIS_DIFF_SCALE          Diff heatmap amplification. Default: 8.0
EOF
}

MODE="${1:-}"
if [[ "${MODE}" != "capture" && "${MODE}" != "check" ]]; then
  usage
  exit 1
fi

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

DEMO_CMD="${VIS_DEMO_CMD:-bash install.sh demo}"
RESULT_DIR="${VIS_RESULT_DIR:-${PROJECT_ROOT}/result}"
BASELINE_DIR="${VIS_BASELINE_DIR:-${PROJECT_ROOT}/output/visual_baseline}"
CURRENT_ROOT="${VIS_CURRENT_DIR:-${PROJECT_ROOT}/output/visual_current}"
REPORT_ROOT="${VIS_REPORT_DIR:-${PROJECT_ROOT}/output/visual_report}"
SKIP_RUN="${VIS_SKIP_RUN:-0}"
MAX_MAE="${VIS_MAX_MAE:-2.0}"
MAX_RMSE="${VIS_MAX_RMSE:-8.0}"
MAX_CHANGED_RATIO="${VIS_MAX_CHANGED_RATIO:-0.03}"
CHANGE_THRESHOLD="${VIS_CHANGE_THRESHOLD:-4}"
DIFF_SCALE="${VIS_DIFF_SCALE:-8.0}"
EXPECTED_IMAGES_RAW="${VIS_EXPECTED_IMAGES:-gl_0.png lidar_0.png}"
TIMESTAMP="$(date +%Y%m%d_%H%M%S)"
CURRENT_DIR="${CURRENT_ROOT}/${TIMESTAMP}"
REPORT_DIR="${REPORT_ROOT}/${TIMESTAMP}"
LATEST_REPORT_DIR="${REPORT_ROOT}/latest"

read -r -a EXPECTED_IMAGES <<< "${EXPECTED_IMAGES_RAW}"

run_demo_if_needed() {
  if [[ "${SKIP_RUN}" == "1" ]]; then
    echo "[visual] VIS_SKIP_RUN=1, skip demo run"
    return
  fi

  echo "[visual] run demo: ${DEMO_CMD}"
  (
    cd "${PROJECT_ROOT}"
    bash -lc "${DEMO_CMD}"
  )
}

collect_current_images() {
  mkdir -p "${CURRENT_DIR}"

  for image in "${EXPECTED_IMAGES[@]}"; do
    local src="${RESULT_DIR}/${image}"
    if [[ ! -f "${src}" ]]; then
      echo "[visual] missing output image: ${src}"
      exit 2
    fi
    cp "${src}" "${CURRENT_DIR}/${image}"
  done
}

capture_baseline() {
  run_demo_if_needed
  collect_current_images

  mkdir -p "${BASELINE_DIR}"
  for image in "${EXPECTED_IMAGES[@]}"; do
    cp "${CURRENT_DIR}/${image}" "${BASELINE_DIR}/${image}"
  done

  cat > "${BASELINE_DIR}/baseline_meta.txt" <<EOF
captured_at=${TIMESTAMP}
demo_cmd=${DEMO_CMD}
images=${EXPECTED_IMAGES_RAW}
EOF

  echo "[visual] baseline updated: ${BASELINE_DIR}"
}

check_regression() {
  for image in "${EXPECTED_IMAGES[@]}"; do
    local baseline="${BASELINE_DIR}/${image}"
    if [[ ! -f "${baseline}" ]]; then
      echo "[visual] baseline missing: ${baseline}"
      echo "[visual] run: bash scripts/visual_regression.sh capture"
      exit 2
    fi
  done

  run_demo_if_needed
  collect_current_images

  mkdir -p "${REPORT_DIR}"

  local failed=0
  for image in "${EXPECTED_IMAGES[@]}"; do
    local baseline="${BASELINE_DIR}/${image}"
    local candidate="${CURRENT_DIR}/${image}"
    local json_out="${REPORT_DIR}/${image%.png}.json"
    local diff_out="${REPORT_DIR}/diff_${image}"

    if ! python3 "${SCRIPT_DIR}/compare_images.py" \
      --baseline "${baseline}" \
      --candidate "${candidate}" \
      --json-output "${json_out}" \
      --diff-output "${diff_out}" \
      --max-mae "${MAX_MAE}" \
      --max-rmse "${MAX_RMSE}" \
      --max-changed-ratio "${MAX_CHANGED_RATIO}" \
      --change-threshold "${CHANGE_THRESHOLD}" \
      --diff-scale "${DIFF_SCALE}"; then
      failed=1
    fi
  done

  rm -rf "${LATEST_REPORT_DIR}"
  cp -a "${REPORT_DIR}" "${LATEST_REPORT_DIR}"

  echo "[visual] reports: ${LATEST_REPORT_DIR}"
  if [[ "${failed}" -ne 0 ]]; then
    echo "[visual] regression check failed"
    exit 1
  fi

  echo "[visual] regression check passed"
}

if [[ "${MODE}" == "capture" ]]; then
  capture_baseline
else
  check_regression
fi
