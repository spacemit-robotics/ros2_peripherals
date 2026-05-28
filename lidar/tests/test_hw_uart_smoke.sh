#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
module_root="$(cd "$script_dir/.." && pwd)"
artifact_dir="${SROBOTIS_TEST_ARTIFACT_DIR:-${SROBOTIS_OUTPUT_ROOT:-$PWD/output}/test/pr/middleware__ros2__peripherals__lidar/modules/middleware__ros2__peripherals__lidar/${SROBOTIS_TEST_NAME:-lidar-hardware-uart-smoke}}"
log_dir="$artifact_dir/logs"
log_file="$log_dir/lidar_hardware_uart_smoke.log"
node_log="$artifact_dir/lidar_hardware_uart_node.log"
scan_log="$artifact_dir/lidar_hardware_uart_scan.log"

lidar_test_model="${LIDAR_TEST_MODEL:-YDLIDAR}"
lidar_test_dev_path="${LIDAR_TEST_DEV_PATH:-/dev/ttyUSB0}"
lidar_test_baud="${LIDAR_TEST_BAUD:-230400}"
lidar_test_topic="${LIDAR_TEST_TOPIC:-/scan}"
lidar_test_frame="${LIDAR_TEST_FRAME_ID:-laser_frame}"
lidar_test_timeout="${LIDAR_TEST_TIMEOUT_S:-30}"

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
node_pid=""

cleanup() {
    if [[ -n "$node_pid" ]] && kill -0 "$node_pid" 2>/dev/null; then
        kill "$node_pid" 2>/dev/null || true
        wait "$node_pid" 2>/dev/null || true
    fi
}
trap cleanup EXIT

echo "[info] module_root=$module_root"
echo "[info] artifact_dir=$artifact_dir"
echo "[info] staging_root=$staging_root"
echo "[info] binary=$binary"
echo "[info] model=$lidar_test_model dev=$lidar_test_dev_path baud=$lidar_test_baud"
echo "[info] topic=$lidar_test_topic frame_id=$lidar_test_frame timeout_s=$lidar_test_timeout"

[[ -x "$binary" ]]
[[ -e "$lidar_test_dev_path" ]]

"$binary" --ros-args \
    -p channel_type:=serial \
    -p model:="$lidar_test_model" \
    -p serial_port:="$lidar_test_dev_path" \
    -p serial_baudrate:="$lidar_test_baud" \
    -p topic_name:="${lidar_test_topic#/}" \
    -p frame_id:="$lidar_test_frame" >"$node_log" 2>&1 &
node_pid=$!
echo "[info] node_pid=$node_pid"

if ! timeout "${lidar_test_timeout}s" ros2 topic echo --once "$lidar_test_topic" sensor_msgs/msg/LaserScan >"$scan_log" 2>&1; then
    echo "[error] failed to receive one LaserScan message from $lidar_test_topic"
    echo "[info] node log:"
    cat "$node_log" || true
    echo "[info] scan log:"
    cat "$scan_log" || true
    exit 1
fi

cat "$scan_log"

grep -q "frame_id: $lidar_test_frame" "$scan_log"
grep -q "angle_min:" "$scan_log"
grep -q "angle_max:" "$scan_log"
grep -q "range_min:" "$scan_log"
grep -q "range_max:" "$scan_log"
grep -q "ranges:" "$scan_log"

echo "ALL TESTS PASSED: hardware-uart-smoke"