#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
module_root="$(cd "$script_dir/.." && pwd)"
artifact_dir="${SROBOTIS_TEST_ARTIFACT_DIR:-${SROBOTIS_OUTPUT_ROOT:-$PWD/output}/test/pr/middleware__ros2__peripherals__lidar/modules/middleware__ros2__peripherals__lidar/${SROBOTIS_TEST_NAME:-lidar-invalid-channel-type}}"
log_dir="$artifact_dir/logs"
log_file="$artifact_dir/lidar-invalid-channel-type.log"

mkdir -p "$log_dir"
exec > >(tee "$log_file") 2>&1

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

binary="$staging_root/lib/peripherals_lidar_node/lidar_2d_node"
run_log="$artifact_dir/lidar_invalid_channel_type.run.log"
run_timeout_s="${LIDAR_INVALID_CHANNEL_TIMEOUT_S:-20}"

{
    echo "[info] module_root=$module_root"
    echo "[info] artifact_dir=$artifact_dir"
    echo "[info] staging_root=$staging_root"
    echo "[info] binary=$binary"
    echo "[info] timeout_s=$run_timeout_s"

    [[ -x "$binary" ]]

    set +e
    timeout "${run_timeout_s}s" "$binary" --ros-args -p channel_type:=bad-channel >"$run_log" 2>&1
    status=$?
    set -e

    cat "$run_log"
    echo "[info] exit_status=$status"

    if [[ $status -eq 124 ]]; then
        echo "[error] node hung instead of failing fast for invalid channel type"
        exit 1
    fi

    if [[ $status -eq 0 ]]; then
        echo "[error] expected invalid channel type to fail"
        exit 1
    fi

    grep -q "Unsupported channel_type: bad-channel" "$run_log"
    grep -q "lidar_2d_node exception" "$run_log"

    echo "ALL TESTS PASSED: invalid-channel-type"
}