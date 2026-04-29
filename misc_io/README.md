# peripherals_misc_io_node

## 接口设计

`components/peripherals/misc_io` 同时覆盖两类能力：

- **输入类 IO**：开关、光耦、普通传感器触点，这类信号会异步变化，需要持续上报边沿事件和当前状态
- **输出类 IO**：蜂鸣器、继电器、普通 GPIO 开关，这类信号需要接收上层控制命令，同时也需要对外暴露当前状态

因此 `misc_io` 不适合做成纯 service，也不适合只做单向 topic。更合适的是以 **发布/订阅为主**：

- **命令输入**：订阅 `peripherals_misc_io_node/msg/MiscIoCommand`
- **状态输出**：发布 `peripherals_misc_io_node/msg/MiscIoState`
- **事件输出**：发布 `peripherals_misc_io_node/msg/MiscIoEvent`

选择 topic 主导而不是 service 的原因：

- 输入 IO 的边沿变化天然是异步事件，service 轮询会丢事件
- 输出 IO 往往是简单布尔控制，可能频繁切换，用命令话题比反复调 service 更自然
- 状态需要广播给多个消费者，topic 更适合

当前实现采用：

- 命令话题：`misc_io/command`
- 状态话题：`misc_io/state`
- 事件话题：`misc_io/events`

其中：

- `MiscIoCommand` 只对 **输出类 IO** 生效
- `MiscIoEvent` 只对 **输入类 IO** 发布
- `MiscIoState` 对输入和输出都会周期发布

状态话题使用 `transient_local`，这样新订阅者启动后可以立即拿到最近一次状态快照。

## 参数

本节点支持一次注册多个 IO，以下数组参数按下标一一对应：

- `io_ids`：逻辑 IO ID，必须唯一
- `types`：设备类型，`0=GENERIC 1=BUZZER 2=RELAY 3=SWITCH 4=SENSOR`
- `dirs`：方向，`0=INPUT 1=OUTPUT`
- `active_logics`：有效电平，`0=ACTIVE_LOW 1=ACTIVE_HIGH`
- `debounce_mss`：去抖时间，单位 ms；主要对输入 IO 有意义
- `chip_names`：GPIO 芯片名，例如 `gpiochip0`
- `line_offsets`：GPIO line offset
- `consumers`：传给 libgpiod 的 consumer 名字，可选
- `io_names`：仅用于日志打印，可选

通用参数：

- `command_topic`：命令订阅话题，默认 `misc_io/command`
- `state_topic`：状态发布话题，默认 `misc_io/state`
- `event_topic`：事件发布话题，默认 `misc_io/events`
- `frame_id`：消息头 `frame_id`，默认 `misc_io`
- `publish_period_ms`：状态发布周期，默认 `100`

参数示例见 [params/misc_io_node.yaml](/home/huanghaiqiang/docker_cross_test/robotic_sdk_not_git/middleware/ros2/peripherals/misc_io/params/misc_io_node.yaml)。

约束如下：

- `io_ids` 不能为空，且每项必须 `>= 0`
- `io_ids` 必须唯一
- `types` 取值必须在 `0..4`
- `dirs` 取值必须是 `0` 或 `1`
- `active_logics` 取值必须是 `0` 或 `1`
- `debounce_mss` 取值必须在 `0..65535`
- `chip_names` 不能为空
- `line_offsets` 必须 `>= 0`
- `consumers` 和 `io_names` 可以省略；如果提供，长度必须和 `io_ids` 一致

## 消息语义

### 1. 命令消息

`peripherals_misc_io_node/msg/MiscIoCommand`

- `io_id`：目标 IO
- `active`：期望状态，`true=有效/开启`

注意：如果 `io_id` 对应的是输入 IO，节点会忽略这条命令。

### 2. 状态消息

`peripherals_misc_io_node/msg/MiscIoState`

- `io_id`：逻辑 IO
- `type`：设备类型
- `dir`：方向
- `active`：当前状态，已经按 `active_logic` 解释，不是原始电平

### 3. 事件消息

`peripherals_misc_io_node/msg/MiscIoEvent`

- `io_id`：逻辑 IO
- `event=0`：ACTIVE
- `event=1`：INACTIVE

只有输入 IO 会发布事件；输出 IO 变化请看状态话题。

## 构建前提

先确保以下包已编译并安装到统一前缀：

```bash
./build/build.sh package components/peripherals/misc_io
```

再编译本节点：

```bash
./build/build.sh package middleware/ros2/peripherals/misc_io
```

## 运行

```bash
source /opt/ros/humble/setup.bash
source output/riscv64/staging/setup.bash

ros2 run peripherals_misc_io_node misc_io_node \
  --ros-args \
  --params-file output/riscv64/staging/share/peripherals_misc_io_node/params/misc_io_node.yaml
```

## 测试验证

### 1. 组件层验证

先验证底层组件本身可用：

```bash
./build/build.sh package components/peripherals/misc_io
sudo output/riscv64/staging/bin/test_misc_io
```

如果要测试输出或主动读取，可根据 `test/test_misc_io.c` 里的示例切换到 `test_set_io()` 或 `test_get_io()`。

### 2. 状态话题验证

启动节点后执行：

```bash
ros2 topic echo /misc_io/state
```

应持续看到每个已注册 IO 的状态消息。输出 IO 在收到命令后，状态应更新；输入 IO 在外部电平变化后，状态也应变化。

### 3. 输入事件验证

如果配置了输入 IO，执行：

```bash
ros2 topic echo /misc_io/events
```

手动触发输入电平变化，应看到：

- 进入有效态时发布 `event=0`
- 离开有效态时发布 `event=1`

### 4. 输出命令验证

如果配置了输出 IO，执行：

```bash
ros2 topic pub --once /misc_io/command peripherals_misc_io_node/msg/MiscIoCommand "{io_id: 1, active: true}"
ros2 topic pub --once /misc_io/command peripherals_misc_io_node/msg/MiscIoCommand "{io_id: 1, active: false}"
```

然后继续观察：

```bash
ros2 topic echo /misc_io/state
```

确认对应 `io_id` 的 `active` 随命令切换。

### 5. 回归检查

建议至少确认以下内容：

- 参数数组长度不一致时节点应启动失败
- `io_ids` 重复时节点应启动失败
- GPIO 号错误时节点应明确报错退出
- 输入 IO 能同时发布 `ACTIVE` 和 `INACTIVE` 事件
- 输出 IO 收到命令后状态能及时更新
- 新订阅者能立刻拿到最近一次 `misc_io/state`
