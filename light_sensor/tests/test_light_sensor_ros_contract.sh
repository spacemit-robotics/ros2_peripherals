#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
module_root="$(cd "$script_dir/.." && pwd)"
fake_tests_dir="$module_root/tests/fakes"
artifact_dir="${SROBOTIS_TEST_ARTIFACT_DIR:-${SROBOTIS_OUTPUT_ROOT:-$module_root/output}/test-artifacts/middleware/ros2/peripherals/light_sensor/${SROBOTIS_TEST_NAME:-light-sensor-ros-contract}}"
log_dir="$artifact_dir/logs"
build_dir="$artifact_dir/build"
mode="${1:-all}"

case "$mode" in
  all|functional|error-paths) ;;
  *) echo "usage: $0 [all|functional|error-paths]" >&2; exit 2 ;;
esac

mkdir -p "$log_dir" "$build_dir"
log_file="$log_dir/light_sensor_ros_${mode//-/_}.log"
cxx="${CXX:-g++}"

{
  echo "[info] module_root=$module_root"
  echo "[info] build_dir=$build_dir"
  echo "[info] cxx=$cxx"
  echo "[info] mode=$mode"

  "$cxx" -std=c++17 -Wall -Wextra -Werror \
    -I"$fake_tests_dir" \
    "$module_root/tests/test_light_sensor_ros_contract.cpp" \
    -o "$build_dir/test_light_sensor_ros_contract"

  "$build_dir/test_light_sensor_ros_contract" "$mode"
} | tee "$log_file"

case "$mode" in
  all) grep -q "light_sensor ros contract test PASSED" "$log_file" ;;
  functional) grep -q "light_sensor ros functional test PASSED" "$log_file" ;;
  error-paths) grep -q "light_sensor ros error paths test PASSED" "$log_file" ;;
esac
