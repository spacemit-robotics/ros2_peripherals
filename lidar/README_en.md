# peripherals_lidar_node

## Introduction

peripherals_lidar_node is a generic ROS2 node for 2D LiDAR sensors. It wraps the unified LiDAR interface (`lidar.h`) provided by `components/peripherals/lidar`, converting low-level point cloud data into standard ROS2 `sensor_msgs/LaserScan` messages. This eliminates the need for separate ROS driver packages for each LiDAR brand/model — a single node supports all registered drivers.

## Features

### Supported

- Publishes standard `sensor_msgs/LaserScan` topic
- Multiple connection modes: serial (serial/uart), Ethernet (tcp/ethernet), simulation (sim)
- Supports YDLIDAR, RPLIDAR, and other drivers registered in `components/peripherals/lidar`
- Configurable scan parameters: scan frequency, angle range, distance range, scan resolution
- Scan direction inversion (`inverted`) and X-axis flip (`flip_x_axis`)
- `start_motor` / `stop_motor` ROS2 service interfaces
- Parameterized launch files and YAML parameter files

### Not Yet Supported

- Angle compensation (`angle_compensate` parameter is reserved)
- 3D LiDAR
- Multiple LiDARs running simultaneously

## Quick Start

### Prerequisites

- ROS2 (Humble or later)
- `components/peripherals/lidar` built and installed (liblidar + lidar.h)
- LiDAR hardware connected (or use `sim` mode for testing)

### Build

**Option 1: Build within the spacemit_robot project**

```bash
cd /path/to/spacemit_robot
source build/envsetup.sh

lunch # Select the appropriate target

m # Full build
```

**Option 2: Standalone build with colcon**

```bash
cd /path/to/spacemit_robot
source build/envsetup.sh
# Make sure the lidar library has been built to output/staging/lib
cd middleware/ros2/peripherals/
colcon build --merge-install --packages-select peripherals_lidar_node
source install/setup.bash
```

### Run Examples

Launch with a launch file (serial mode):

```bash
ros2 launch peripherals_lidar_node lidar_2d.launch.py channel_type:=serial serial_port:=/dev/ttyUSB0 serial_baudrate:=115200 model:=YDLIDAR
```

Launch with a parameter file:

```bash
ros2 run peripherals_lidar_node lidar_2d_node --ros-args --params-file params/lidar_2d.yaml
```

Simulation mode:

```bash
ros2 launch peripherals_lidar_node lidar_2d.launch.py channel_type:=sim
```

## Detailed Usage

> For detailed parameter descriptions and advanced usage, please refer to the official documentation (to be added).

## FAQ

### Q: `lidar.h or liblidar not found` at startup

`components/peripherals/lidar` is not installed or its install path is not in `CMAKE_PREFIX_PATH`. Please build and install the lidar component first.

### Q: Failed to open serial port due to insufficient permissions

Add the current user to the `dialout` group:

```bash
sudo usermod -aG dialout $USER
# Log out and log back in for the change to take effect
```

### Q: Published LaserScan data is all inf

1. Check that the LiDAR power and connection are working properly
2. Verify that the `model` parameter matches the actual LiDAR model
3. Check that the `range_min` / `range_max` values are reasonable

### Q: Scan frequency does not match expectations

The `scan_frequency` parameter is in Hz (default 10.0). The node internally converts it to RPM before passing it to the underlying driver. Please verify that the LiDAR hardware supports the configured rotation speed.

## Versions & Releases

| Version | Date | Description |
|---------|------|-------------|
| 0.0.1 | 2026-02-28 | Initial release, wrapping the unified lidar interface as a ROS2 node |

## Contributing

1. Fork this repository
2. Create a feature branch (`git checkout -b feature/xxx`)
3. Commit your changes (`git commit -m 'Add xxx'`)
4. Push the branch (`git push origin feature/xxx`)
5. Create a Pull Request


## License

Source files in this component are licensed under Apache-2.0 as declared in their headers. The `LICENSE` file in this directory shall prevail.
