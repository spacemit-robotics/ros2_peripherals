# IMU ROS2 Node

## 项目简介

本组件提供 IMU（惯性测量单元）传感器的 ROS2/FastDDS 数据发布节点，用于将真实 IMU 硬件的数据（加速度、角速度、姿态四元数）通过 DDS 话题发布，供机器人导航、姿态估计等上层应用使用。

## 功能特性

**支持：**
- UART 串口接口的 IMU 传感器（如 CMP10A）
- 命令行参数配置 IMU 型号、设备路径、波特率、发布频率、话题名、DDS域ID
- 可配置的数据发布频率（1-1000Hz）
- FastDDS 话题发布（默认话题名：`rt/imu`）
- 共享内存（SHM）+ UDP 双传输层支持，提升本地通信性能
- BEST_EFFORT QoS 配置，适合高频传感器数据
- 共享库架构，可被其他项目复用


## 快速开始

### 环境准备

- ROS2 Humble 或更高版本
- FastDDS 库
- 已编译的 `imu` 底层库（来自 `components/peripherals/imu`）

### 构建编译

**方式一：在 spacemit_robot 工程内编译**

```bash
cd /path/to/spacemit_robot
source build/envsetup.sh
# 确保 imu 底层库已编译到 output/staging/lib
cd middleware/ros2/peripherals/imu
mkdir -p build && cd build
cmake .. && make
```

**方式二：使用 colcon 独立编译**

```bash
mkdir -p ~/imu_ws/src
# 将 imu 底层库和本节点放入 src 目录
cd ~/imu_ws
colcon build --merge-install
source install/setup.bash
```

### 运行示例

```bash
# 使用默认参数（CMP10A, /dev/ttyUSB1, 9600, 50Hz, rt/imu, domain 0）
ros2 run peripherals_imu_node imu_uart_node

# 指定参数
ros2 run peripherals_imu_node imu_uart_node -n "CMP10A:cmp10a_imu" -d /dev/ttyUSB0 -b 115200 -r 100 -t "rt/imu" -i 0

# 查看帮助
ros2 run peripherals_imu_node imu_uart_node -h
```

**命令行参数：**

| 参数 | 说明 | 默认值 |
|------|------|--------|
| `-n <name>` | IMU 名称（格式：`型号:驱动名`） | `CMP10A:cmp10a_imu` |
| `-d <device>` | 串口设备路径 | `/dev/ttyUSB1` |
| `-b <baud>` | 波特率 | `9600` |
| `-r <rate>` | 发布频率（Hz，范围：1-1000） | `50` |
| `-t <topic>` | DDS 话题名 | `rt/imu` |
| `-i <id>` | DDS 域 ID | `0` |
| `-h` | 显示帮助信息 | - |

## 架构说明

本节点采用模块化设计：

1. **共享库** (`peripherals_imu_node_lib.so`)：
   - 包含 FastDDS 类型定义（Time, Vector3, Quaternion, Header, Imu）
   - 提供 `ImuPublisher` 和 `ImuSubscriber` 类
   - 支持 SHM（共享内存）+ UDP 双传输层
   - 配置 BEST_EFFORT 可靠性和 VOLATILE 持久性 QoS
   - 仅暴露 `include/` 目录的公共接口（`MyImuPublisher.h`, `MyImuSubscriber.h`）

2. **可执行文件** (`imu_uart_node`)：
   - IMU UART 设备读取逻辑
   - 数据发布到 DDS 话题
   - 支持命令行参数配置

**其他项目集成：**

其他 ROS2 包可以通过链接 `peripherals_imu_node_lib` 来复用 IMU DDS 通信能力：

```cmake
find_package(peripherals_imu_node REQUIRED)
target_link_libraries(your_target peripherals_imu_node_lib)
```

## 详细使用

详细使用文档请参考官方文档。

串口IMU最高发布频率和波特率一般通过厂商提供的上位机软件设置，请确保示例程序和上位机中的设置一致

## 常见问题

**Q: 编译时提示 `找不到 -limu`**

A: 确保 `imu` 底层库已编译，并且库文件路径正确。使用 colcon 编译时，确保 `imu` 包先于本包编译完成。

**Q: 编译时提示 `imu.h: 没有那个文件或目录`**

A: CMakeLists.txt 会自动从以下位置查找 imu 库：
1. `CMAKE_INSTALL_PREFIX`（colcon --merge-install 模式）
2. `AMENT_PREFIX_PATH` 环境变量
3. 本地相对路径（in-tree 构建）

请确保至少有一个路径包含 `imu.h` 头文件。

**Q: 运行时提示 `Failed to allocate IMU device`**

A: 检查串口设备是否存在，以及是否有访问权限：
```bash
ls -l /dev/ttyUSB0
sudo chmod 666 /dev/ttyUSB0
# 或将用户加入 dialout 组
sudo usermod -aG dialout $USER
```

**Q: 如何适配新的 IMU 型号？**

A: 需要在 `components/peripherals/imu` 中添加对应的驱动实现，然后通过 `-n` 参数指定新的 IMU 名称。

## 版本与发布

| 版本 | 日期 | 变更内容 |
|------|------|----------|
| v1.1.0 | 2026-03-10 | 重构为共享库架构，添加 SHM+UDP 传输支持，支持可配置话题名和域ID |
| v1.0.0 | 2026-02-12 | 初始版本，支持 UART 接口 IMU |

## 贡献方式

欢迎提交 Issue 和 Pull Request。

## License

本组件源码文件头声明为 Apache-2.0，最终以本目录 `LICENSE` 文件为准。
