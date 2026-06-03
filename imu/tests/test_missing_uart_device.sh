#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
module_root="$(cd "$script_dir/.." && pwd)"
artifact_dir="${SROBOTIS_TEST_ARTIFACT_DIR:-${SROBOTIS_OUTPUT_ROOT:-$PWD/output}/test/pr/middleware__ros2__peripherals__imu/modules/middleware__ros2__peripherals__imu/${SROBOTIS_TEST_NAME:-imu-missing-uart-device}}"
log_dir="$artifact_dir/logs"

mkdir -p "$log_dir"

staging_root="${SROBOTIS_OUTPUT_STAGING:-}"
if [[ -z "$staging_root" ]]; then
    echo "[error] SROBOTIS_OUTPUT_STAGING is not set"
    exit 1
fi

setup_script="$staging_root/setup.sh"
if [[ ! -f "$setup_script" ]]; then
    echo "[error] setup script not found: $setup_script"
    exit 1
fi

set +u
# shellcheck source=/dev/null
source "$setup_script"
set -u

binary="$staging_root/lib/peripherals_imu_node/imu_uart_node"
run_log="$artifact_dir/imu_missing_uart_device.run.log"
invalid_port="/dev/spacemit-imu-ci-missing"

{
    echo "[info] module_root=$module_root"
    echo "[info] artifact_dir=$artifact_dir"
    echo "[info] staging_root=$staging_root"
    echo "[info] binary=$binary"
    echo "[info] invalid_port=$invalid_port"

    [[ -x "$binary" ]]
    [[ ! -e "$invalid_port" ]]

    set +e
    timeout 20s "$binary" -d "$invalid_port" >"$run_log" 2>&1
    status=$?
    set -e

    cat "$run_log"
    echo "[info] exit_status=$status"

    if [[ $status -eq 124 ]]; then
        echo "[error] node hung instead of failing fast for missing UART device"
        exit 1
    fi

    if [[ $status -eq 0 ]]; then
        echo "[error] expected missing UART device to fail"
        exit 1
    fi

    grep -Eq "Failed to allocate IMU device|Failed to initialize IMU|not found|No such file|cannot|failed" "$run_log"

    echo "ALL TESTS PASSED: missing-uart-device"
} | tee "$log_dir/imu_missing_uart_device.log"
