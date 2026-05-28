#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
module_root="$(cd "$script_dir/.." && pwd)"
artifact_dir="${SROBOTIS_TEST_ARTIFACT_DIR:-${SROBOTIS_OUTPUT_ROOT:-$PWD/output}/test/pr/middleware__ros2__peripherals__lidar/modules/middleware__ros2__peripherals__lidar/${SROBOTIS_TEST_NAME:-lidar-launch-override-contract}}"
log_dir="$artifact_dir/logs"
check_overrides_script="$script_dir/check_launch_overrides.py"

mkdir -p "$log_dir"

package_name="peripherals_lidar_node"
staging_root="${SROBOTIS_OUTPUT_STAGING:-}"
setup_script="$staging_root/setup.sh"

if [[ -z "$staging_root" ]]; then
    echo "[error] SROBOTIS_OUTPUT_STAGING is not set"
    exit 1
fi

if [[ ! -f "$setup_script" ]]; then
    echo "[error] setup script not found: $setup_script"
    exit 1
fi

set +u
# shellcheck source=/dev/null
source "$setup_script"
set -u

package_share_dir="$staging_root/share/$package_name"
launch_file="$package_share_dir/launch/lidar_2d.launch.py"

{
    echo "[info] module_root=$module_root"
    echo "[info] artifact_dir=$artifact_dir"
    echo "[info] staging_root=$staging_root"
    echo "[info] launch_file=$launch_file"

    [[ -f "$launch_file" ]]
    [[ -f "$check_overrides_script" ]]

    python3 "$check_overrides_script" "$launch_file"

    echo "ALL TESTS PASSED: launch-override-contract"
} | tee "$log_dir/lidar_launch_override_contract.log"