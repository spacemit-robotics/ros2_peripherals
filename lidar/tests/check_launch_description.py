#!/usr/bin/env python3

# Copyright 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
#
# SPDX-License-Identifier: Apache-2.0

import importlib.util
import sys


def main() -> int:
    launch_file = sys.argv[1]
    spec = importlib.util.spec_from_file_location("lidar_launch", launch_file)
    module = importlib.util.module_from_spec(spec)
    assert spec.loader is not None
    spec.loader.exec_module(module)

    desc = module.generate_launch_description()
    entities = list(desc.entities)

    assert len(entities) == 17, f"unexpected entity count: {len(entities)}"

    declare_names = []
    node = None
    for entity in entities:
        cls_name = entity.__class__.__name__
        if cls_name == "DeclareLaunchArgument":
            declare_names.append(entity.name)
        elif cls_name == "Node":
            node = entity

    expected_args = {
        "channel_type", "model", "serial_port", "serial_baudrate", "tcp_ip",
        "tcp_port", "frame_id", "topic_name", "scan_frequency", "angle_min",
        "angle_max", "range_min", "range_max", "inverted", "flip_x_axis",
        "scan_bins",
    }
    assert set(declare_names) == expected_args, f"launch args mismatch: {declare_names}"
    assert node is not None, "Node action missing"
    assert getattr(node, "_Node__package") == "peripherals_lidar_node"
    assert getattr(node, "_Node__node_executable") == "lidar_2d_node"
    assert getattr(node, "_Node__node_name") == "lidar_2d_node"

    parameters = getattr(node, "_Node__parameters")
    assert len(parameters) == 1, f"unexpected parameter blocks: {len(parameters)}"
    parameter_map = parameters[0]
    def normalize_key_part(part):
        text = getattr(part, "text", None)
        if text is not None:
            return text
        return str(part)

    normalized_keys = set()
    for key in parameter_map.keys():
        if isinstance(key, tuple):
            normalized_keys.add("".join(normalize_key_part(part) for part in key))
        else:
            normalized_keys.add(normalize_key_part(key))
    for key in expected_args:
        assert key in normalized_keys, f"missing node parameter: {key}; got {sorted(normalized_keys)}"

    print("LAUNCH_DESCRIPTION_OK")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
