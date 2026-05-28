#!/usr/bin/env python3

# Copyright 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
#
# SPDX-License-Identifier: Apache-2.0

import importlib.util
import sys


def normalize_key_part(part):
    text = getattr(part, "text", None)
    if text is not None:
        return text
    return str(part)


def normalize_value_part(part):
    variable_name = getattr(part, "variable_name", None)
    if variable_name is not None:
        return normalize_value(variable_name)
    text = getattr(part, "text", None)
    if text is not None:
        return text
    return str(part)


def normalize_key(key):
    if isinstance(key, (list, tuple)):
        return "".join(normalize_key_part(part) for part in key)
    return normalize_key_part(key)


def normalize_value(value):
    if isinstance(value, (list, tuple)):
        return "".join(normalize_value_part(part) for part in value)
    return normalize_value_part(value)


def main() -> int:
    launch_file = sys.argv[1]
    spec = importlib.util.spec_from_file_location("lidar_launch", launch_file)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)

    desc = module.generate_launch_description()
    node = None
    default_values = {}

    for entity in desc.entities:
        cls_name = entity.__class__.__name__
        if cls_name == "DeclareLaunchArgument":
            default_values[entity.name] = normalize_value(entity.default_value)
        elif cls_name == "Node":
            node = entity

    expected_defaults = {
        "channel_type": "serial",
        "model": "YDLIDAR",
        "serial_port": "/dev/ttyUSB0",
        "serial_baudrate": "230400",
        "tcp_ip": "192.168.0.7",
        "tcp_port": "2368",
        "frame_id": "laser_frame",
        "topic_name": "scan",
        "scan_frequency": "10.0",
        "angle_min": "-3.14159",
        "angle_max": "3.14159",
        "range_min": "0.1",
        "range_max": "30.0",
        "inverted": "false",
        "flip_x_axis": "false",
        "scan_bins": "720",
    }

    for key, expected in expected_defaults.items():
        assert default_values.get(key) == expected, (
            f"unexpected default for {key}: {default_values.get(key)!r}"
        )

    assert node is not None, "Node action missing"
    parameters = getattr(node, "_Node__parameters")
    assert len(parameters) == 1, f"unexpected parameter blocks: {len(parameters)}"

    parameter_map = {
        normalize_key(key): normalize_value(value)
        for key, value in parameters[0].items()
    }

    for key in expected_defaults:
        assert parameter_map.get(key) == key, (
            f"parameter {key} is not wired to LaunchConfiguration({key}); "
            f"got {parameter_map.get(key)!r}"
        )

    print("LAUNCH_OVERRIDES_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
