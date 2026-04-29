# peripherals_light_sensor_node

## 接口设计

`components/peripherals/light_sensor` 底层是典型的同步采样模型：

- 节点主动调用 `light_sensor_poll()` 获取当前光照值
- 返回的是某一时刻的连续物理量，不是离散事件
- 上层通常关心的是“持续的光照数据流”，而不是一次性命令交互

因此 ROS 2 接口更适合使用 **话题**，而不是以 service 作为主接口：

- **话题更合适**：光照值本质上是连续传感器数据，适合按固定频率发布，多个消费者也能直接共享同一数据流
- **服务不适合做主接口**：如果用服务读取，使用方就需要自己轮询，既增加复杂度，也不符合 ROS 里传感器数据的常见模型

本节点采用：

- 数据输出：`sensor_msgs/msg/Illuminance`
- 默认话题：`/light_sensor/lux`

当前节点不额外提供 service。若后续需要“即时读取一次”或“动态修改采样频率”，可以再补充服务或参数回调，但不应替代主话题。

## 参数

- `driver`：底层驱动名，默认 `W1160`
- `name`：设备实例名，默认 `als0`
- `device`：I2C 设备路径，默认 `/dev/i2c-5`
- `i2c_addr`：I2C 地址，默认 `0x48`
- `frame_id`：消息头 `frame_id`，默认 `light_sensor`
- `topic_name`：输出话题名，默认 `light_sensor/lux`
- `poll_hz`：采样与发布频率，默认 `5.0`
- `variance`：消息里的测量方差，默认 `0.0`

参数示例见 [params/light_sensor_node.yaml](/home/huanghaiqiang/docker_cross_test/robotic_sdk_not_git/middleware/ros2/peripherals/light_sensor/params/light_sensor_node.yaml)。

注意：

- 节点会把 `driver` 和 `name` 组合成底层实例名，例如默认组合后是 `W1160:als0`
- `poll_hz` 必须大于 0
- 当前节点只封装了 I2C 光照传感器路径，对应底层公开 API 也只有 `light_sensor_alloc_i2c()`

## 构建前提

先确保以下包已经编译并安装到统一前缀：

```bash
./build/build.sh package components/peripherals/light_sensor
```

再编译本节点：

```bash
./build/build.sh package middleware/ros2/peripherals/light_sensor
```

## 运行

```bash
source /opt/ros/humble/setup.bash
source output/riscv64/staging/setup.bash

ros2 run peripherals_light_sensor_node light_sensor_node \
  --ros-args \
  --params-file output/riscv64/staging/share/peripherals_light_sensor_node/params/light_sensor_node.yaml
```

## 测试验证

### 1. 组件层验证

先验证底层组件本身可用：

```bash
./build/build.sh package components/peripherals/light_sensor
sudo output/riscv64/staging/bin/test_light_sensor_i2c
```

确认能持续打印 lux 数值。

### 2. ROS 2 话题验证

启动节点后执行：

```bash
ros2 topic echo /light_sensor/lux
```

应能看到 `sensor_msgs/msg/Illuminance`，其中 `illuminance` 字段为当前光照值。

### 3. 多节点订阅验证

建议同时开两个订阅端：

```bash
ros2 topic echo /light_sensor/lux
ros2 topic hz /light_sensor/lux
```

确认多个订阅者都能收到同样的数据流，并且发布频率与 `poll_hz` 基本一致。

### 4. 回归检查

建议至少确认以下内容：

- 设备路径错误时节点应启动失败
- 驱动名错误时节点应明确报错退出
- `poll_hz <= 0` 时节点应启动失败
- 编译安装后目录中存在：
  - `share/peripherals_light_sensor_node/package.sh`
  - `lib/peripherals_light_sensor_node/light_sensor_node`
