#!/usr/bin/env python3

# Copyright 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
#
# SPDX-License-Identifier: Apache-2.0

import math
import sys

import yaml


def main() -> int:
    params_file = sys.argv[1]
    with open(params_file, "r", encoding="utf-8") as file:
        data = yaml.safe_load(file)

    params = data["lidar_2d_node"]["ros__parameters"]
    assert params["channel_type"] == "serial"
    assert params["topic_name"] == "scan"
    assert params["frame_id"] == "laser_frame"
    assert params["serial_port"] == "/dev/ttyUSB0"
    assert params["serial_baudrate"] == 230400
    assert math.isclose(float(params["scan_frequency"]), 10.0)
    assert math.isclose(float(params["angle_min"]), -3.14159, rel_tol=0.0, abs_tol=1e-5)
    assert math.isclose(float(params["angle_max"]), 3.14159, rel_tol=0.0, abs_tol=1e-5)
    assert math.isclose(float(params["range_min"]), 0.1)
    assert math.isclose(float(params["range_max"]), 30.0)
    assert params["inverted"] is False
    assert params["flip_x_axis"] is False
    assert params["angle_compensate"] is False
    assert int(params["scan_bins"]) == 720
    assert int(params["return_mode"]) == 0
    print("PARAM_FILE_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
