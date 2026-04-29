# peripherals_lidar_node

## 项目简介

peripherals_lidar_node 是基于 ROS2 的 2D 激光雷达通用节点，封装了 `components/peripherals/lidar` 提供的统一激光雷达接口（`lidar.h`），将底层点云数据转换为 ROS2 标准 `sensor_msgs/LaserScan` 消息发布。解决了不同品牌/型号激光雷达在 ROS2 环境下需要各自独立 ROS 驱动包的问题，一个节点即可适配所有已支持的雷达型号。

## 功能特性

### 支持的功能

- 发布标准 `sensor_msgs/LaserScan` 话题
- 多种连接方式：串口（serial/uart）、以太网（tcp/ethernet）、仿真（sim）
- 支持 YDLIDAR、RPLIDAR 等已在 `components/peripherals/lidar` 中注册的驱动
- 可配置的扫描参数：扫描频率、角度范围、距离范围、扫描分辨率
- 扫描方向翻转（`inverted`）、X 轴镜像（`flip_x_axis`）
- 提供 `start_motor` / `stop_motor` ROS2 服务接口
- 支持 launch 文件参数化启动与 YAML 参数文件

### 暂不支持

- 角度补偿（`angle_compensate` 参数已预留）
- 3D 激光雷达
- 多雷达同时运行

## 快速开始

### 环境准备

- ROS2（Humble 或更高版本）
- 已构建并安装 `components/peripherals/lidar`（liblidar + lidar.h）
- 激光雷达硬件已连接（或使用 `sim` 模式测试）

### 构建编译

**方式一：在 spacemit_robot 工程内编译**

```bash
cd /path/to/spacemit_robot
source build/envsetup.sh

lunch # 选择合适的target

m # 全量编译
```

**方式二：使用 colcon 独立编译**

```bash
cd /path/to/spacemit_robot
source build/envsetup.sh
# 确保 lidar 底层库已编译到 output/staging/lib
cd middleware/ros2/peripherals/
colcon build --merge-install --packages-select peripherals_lidar_node
source install/setup.bash
```

### 运行示例

使用 launch 文件启动（串口模式）：

```bash
ros2 launch peripherals_lidar_node lidar_2d.launch.py channel_type:=serial serial_port:=/dev/ttyUSB0 serial_baudrate:=115200 model:=YDLIDAR
```

使用参数文件启动：

```bash
ros2 run peripherals_lidar_node lidar_2d_node --ros-args --params-file params/lidar_2d.yaml
```

仿真模式：

```bash
ros2 launch peripherals_lidar_node lidar_2d.launch.py channel_type:=sim
```

## 详细使用

> 详细参数说明与高级用法请参考官方文档（待补充）。

## 常见问题

### Q: 启动时报 `lidar.h or liblidar not found`

`components/peripherals/lidar` 未安装或安装路径不在 `CMAKE_PREFIX_PATH` 中。请先编译安装底层 lidar 组件。

### Q: 串口打开失败，提示权限不足

将当前用户加入 `dialout` 组：

```bash
sudo usermod -aG dialout $USER
# 重新登录后生效
```

### Q: 发布的 LaserScan 数据全是 inf

1. 检查激光雷达电源与连接是否正常
2. 确认 `model` 参数与实际雷达型号匹配
3. 检查 `range_min` / `range_max` 范围是否合理

### Q: 扫描频率不符合预期

`scan_frequency` 参数单位为 Hz（默认 10.0），节点内部会转换为 RPM 传入底层驱动。请确认雷达硬件支持所设定的转速。

## 版本与发布

| 版本 | 日期 | 说明 |
|------|------|------|
| 0.0.1 | 2026-02-28 | 初始版本，封装 lidar 统一接口为 ROS2 节点 |

## 贡献方式

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/xxx`)
3. 提交更改 (`git commit -m 'Add xxx'`)
4. 推送分支 (`git push origin feature/xxx`)
5. 创建 Pull Request


## License

本组件源码文件头声明为 Apache-2.0，最终以本目录 `LICENSE` 文件为准。
