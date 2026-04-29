# Copyright 2026 SpacemiT (Hangzhou) Technology Co. Ltd.
#
# SPDX-License-Identifier: Apache-2.0

"""
Launch file for peripherals_lidar_node.
"""
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node

def generate_launch_description():
    return LaunchDescription([
        DeclareLaunchArgument(
            'channel_type',
            default_value='serial',
            description='Channel type: serial, tcp, or sim'
        ),
        DeclareLaunchArgument(
            'model',
            default_value='',
            description='LiDAR model name (driver name)'
        ),
        DeclareLaunchArgument(
            'serial_port',
            default_value='/dev/ttyUSB0',
            description='Serial port for UART connection'
        ),
        DeclareLaunchArgument(
            'serial_baudrate',
            default_value='230400',
            description='Serial baudrate'
        ),
        DeclareLaunchArgument(
            'tcp_ip',
            default_value='192.168.0.7',
            description='TCP IP address'
        ),
        DeclareLaunchArgument(
            'tcp_port',
            default_value='2368',
            description='TCP port'
        ),
        DeclareLaunchArgument(
            'frame_id',
            default_value='laser_frame',
            description='Frame ID for laser scan'
        ),
        DeclareLaunchArgument(
            'topic_name',
            default_value='scan',
            description='Topic name for laser scan'
        ),
        DeclareLaunchArgument(
            'scan_frequency',
            default_value='10.0',
            description='Scan frequency in Hz'
        ),
        DeclareLaunchArgument(
            'angle_min',
            default_value='-3.14159',
            description='Minimum angle in radians'
        ),
        DeclareLaunchArgument(
            'angle_max',
            default_value='3.14159',
            description='Maximum angle in radians'
        ),
        DeclareLaunchArgument(
            'range_min',
            default_value='0.1',
            description='Minimum range in meters'
        ),
        DeclareLaunchArgument(
            'range_max',
            default_value='30.0',
            description='Maximum range in meters'
        ),
        DeclareLaunchArgument(
            'inverted',
            default_value='false',
            description='Invert scan direction'
        ),
        DeclareLaunchArgument(
            'flip_x_axis',
            default_value='false',
            description='Flip X axis'
        ),
        DeclareLaunchArgument(
            'scan_bins',
            default_value='720',
            description='Number of scan bins'
        ),
        Node(
            package='peripherals_lidar_node',
            executable='lidar_2d_node',
            name='lidar_2d_node',
            output='screen',
            parameters=[{
                'channel_type': LaunchConfiguration('channel_type'),
                'model': LaunchConfiguration('model'),
                'serial_port': LaunchConfiguration('serial_port'),
                'serial_baudrate': LaunchConfiguration('serial_baudrate'),
                'tcp_ip': LaunchConfiguration('tcp_ip'),
                'tcp_port': LaunchConfiguration('tcp_port'),
                'frame_id': LaunchConfiguration('frame_id'),
                'topic_name': LaunchConfiguration('topic_name'),
                'scan_frequency': LaunchConfiguration('scan_frequency'),
                'angle_min': LaunchConfiguration('angle_min'),
                'angle_max': LaunchConfiguration('angle_max'),
                'range_min': LaunchConfiguration('range_min'),
                'range_max': LaunchConfiguration('range_max'),
                'inverted': LaunchConfiguration('inverted'),
                'flip_x_axis': LaunchConfiguration('flip_x_axis'),
                'scan_bins': LaunchConfiguration('scan_bins'),
            }],
        ),
    ])
