# peripherals_led_node

## 接口设计

`components/peripherals/led` 的底层接口更适合做 **订阅发布**，不适合优先做 service server。

原因：

- `led_set_state`、`led_set_brightness`、`led_set_color`、`led_blink`、`led_breath` 都是“设置目标状态”的接口
- 底层接口没有统一返回码语义，也没有需要上层同步等待的动作结果
- `led_tick()` 需要周期调用，说明 LED 控制本质上是一个持续运行的本地状态机
- 当前包内定义了 `peripherals_led_node/msg/LedCommand` 和 `peripherals_led_node/msg/LedState`

因此本节点采用：

- 命令输入：订阅 `peripherals_led_node/msg/LedCommand`
- 状态输出：发布 `peripherals_led_node/msg/LedState`

### 为什么不优先用 service

- service 更适合同步动作和明确返回值，比如 WiFi 连接、5G 拨号、NFC 读写
- LED 命令更像“设置执行器目标状态”，天然更适合 topic
- 上层状态机、行为树、UI、动画节点都可以直接向同一个命令 topic 发布

### 节点行为

- 节点可同时管理多个 LED 实例，用 `led_id` 区分
- 对 `mode=STATIC/BLINK/BREATH` 分别映射到 `led_set_* / led_blink / led_breath`
- 内部按 `tick_period_ms` 周期调用 `led_tick()`
- 按 `publish_period_ms` 周期发布所有 LED 当前状态
- 状态 topic 使用 `transient_local`，新订阅者启动后能立即拿到最近一次状态

## 参数

- `frame_id`：状态消息 `header.frame_id`，默认 `led`
- `command_topic`：命令话题，默认 `led/command`
- `state_topic`：状态话题，默认 `led/state`
- `tick_period_ms`：调用 `led_tick()` 的周期，默认 `50`
- `publish_period_ms`：状态发布周期，默认 `100`
- `publish_on_command`：收到命令后是否立刻发布对应状态，默认 `true`
- `publish_on_startup`：启动后是否立即发布一次全部状态，默认 `true`
- `led_ids`：LED 逻辑 ID 列表
- `transports`：每个 LED 的类型，支持 `generic`、`spi`
- `names`：每个 LED 的实例名
- `generic_sysfs_names`：generic LED 的 sysfs 名称，空字符串表示复用 `names`
- `generic_active_levels`：generic LED 的有效电平，支持 `0/1`
- `spi_dev_paths`：SPI LED 的 spidev 路径
- `spi_num_leds`：SPI 灯带灯珠数量
- `spi_speed_hz`：SPI 频率
- `spi_reset_bytes`：WS2812 reset bytes

参数示例见 [params/led_node.yaml](/home/huanghaiqiang/workspace/k3_robot_sdk/robotics_dev/middleware/ros2/peripherals/led/params/led_node.yaml)。

## 构建前提

先确保以下包已编译并安装到统一前缀：

```bash
./build/build.sh package components/peripherals/led
```

再编译本节点：

```bash
./build/build.sh package middleware/ros2/peripherals/led
```

## 运行

```bash
source /opt/ros/humble/setup.bash
source output/riscv64/staging/setup.bash

ros2 run peripherals_led_node led_node \
  --ros-args \
  --params-file output/riscv64/staging/share/peripherals_led_node/params/led_node.yaml
```

## 典型调用

静态点亮：

```bash
ros2 topic pub /led/command peripherals_led_node/msg/LedCommand \
  "{header: {frame_id: 'led'}, led_id: 0, r: 255, g: 0, b: 0, brightness: 128, mode: 0, period_ms: 0, on_ms: 0, count: 0}" -1
```

闪烁：

```bash
ros2 topic pub /led/command peripherals_led_node/msg/LedCommand \
  "{header: {frame_id: 'led'}, led_id: 0, r: 255, g: 255, b: 255, brightness: 255, mode: 1, period_ms: 1000, on_ms: 200, count: 5}" -1
```

呼吸：

```bash
ros2 topic pub /led/command peripherals_led_node/msg/LedCommand \
  "{header: {frame_id: 'led'}, led_id: 0, r: 0, g: 0, b: 255, brightness: 128, mode: 2, period_ms: 2000, on_ms: 0, count: 0}" -1
```
