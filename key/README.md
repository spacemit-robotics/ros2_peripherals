# peripherals_key_node

## 接口设计

`components/peripherals/key` 底层是典型的异步事件模型：

- GPIO 电平变化由后台线程持续扫描
- 上层关注的是按下、释放、单击、双击、长按、连发等离散事件
- 事件出现时间不确定，且可能连续发生

因此 ROS 2 接口的主通道适合使用 **话题**，不适合以服务作为核心接口：

- **话题更合适**：按键事件天然是推送式、异步、可广播的，多个消费者都可能需要同时订阅
- **服务不合适**：服务是请求-响应模型，更适合“执行动作”或“查询一次状态”；用服务轮询按键会丢边沿事件，也会增加延迟和轮询开销

本节点采用：

- 事件输出：`peripherals_key_node/msg/KeyEvent`
- 默认话题：`/key/events`

如果后续需要“查询当前配置”或“动态启停某个按键”，可以再补充服务接口；但这不应该替代事件话题。

## 参数

支持一次注册多个按键，数组下标一一对应：

- `event_topic`：事件发布话题，默认 `key/events`
- `frame_id`：消息头 `frame_id`，默认 `key`
- `publish_period_ms`：事件队列刷出周期，默认 `10`
- `key_ids`：逻辑按键 ID 数组
- `gpio_nums`：GPIO 编号数组
- `active_lows`：是否低电平有效，`1/0`
- `long_press_mss`：长按阈值数组，单位 ms
- `double_click_mss`：双击阈值数组，单位 ms
- `key_names`：仅用于日志打印

参数示例见 [params/key_node.yaml](/home/huanghaiqiang/docker_cross_test/robotic_sdk_not_git/middleware/ros2/peripherals/key/params/key_node.yaml)。

注意：ROS 2 参数文件顶层键必须匹配实际节点名。本节点构造时使用的是 `key_node`，不是包名 `peripherals_key_node`。

如果要一次注册多个按键，需要把 `key_ids`、`gpio_nums`、`active_lows`、`long_press_mss`、`double_click_mss` 写成等长数组；`key_names` 如果提供，也必须和它们等长。数组下标一一对应，例如第 `i` 个按键会组合使用：
注意，如果不需要检查双击事件，请把double_click_mss参数设为0，否则会等待超时后，再返回单击事件

- `key_ids[i]`
- `gpio_nums[i]`
- `active_lows[i]`
- `long_press_mss[i]`
- `double_click_mss[i]`
- `key_names[i]`

多按键配置示例：

```yaml
key_node:
  ros__parameters:
    event_topic: "key/events"
    frame_id: "key"
    publish_period_ms: 10

    key_ids:          [0,        1,         2]
    gpio_nums:        [83,       84,        85]
    active_lows:      [1,        1,         0]
    long_press_mss:   [1500,     2000,      1000]
    double_click_mss: [300,      300,       250]
    key_names:        ["power",  "reset",   "mode"]
```

对应关系如下：

- 第 1 个键：`key_id=0 gpio=83 active_low=1 long_press_ms=1500 double_click_ms=300 name=power`
- 第 2 个键：`key_id=1 gpio=84 active_low=1 long_press_ms=2000 double_click_ms=300 name=reset`
- 第 3 个键：`key_id=2 gpio=85 active_low=0 long_press_ms=1000 double_click_ms=250 name=mode`

约束如下：

- `key_ids` 不能为空，且每项必须 `>= 0`
- `gpio_nums` 每项必须 `> 0`
- `long_press_mss` 和 `double_click_mss` 每项必须 `>= 0`
- `key_names` 可以省略；省略时节点会自动生成默认名字
- 任一数组长度不一致时，节点会启动失败

## 事件映射

底层 `key_event_t` 到 `peripherals_key_node/msg/KeyEvent.event_type` 的映射如下：

- `KEY_EV_PRESSED` -> `1`
- `KEY_EV_RELEASED` -> `2`
- `KEY_EV_CLICK` -> `3`
- `KEY_EV_DOUBLE_CLICK` -> `4`
- `KEY_EV_LONG_PRESS` -> `5`
- `KEY_EV_HOLD_REPEAT` -> `8`

## 构建前提

先确保以下包已编译并安装到统一前缀：

```bash
./build/build.sh package components/peripherals/key
```

再编译本节点：

```bash
./build/build.sh package middleware/ros2/peripherals/key
```

## 运行

```bash
source /opt/ros/humble/setup.bash
source output/riscv64/staging/setup.bash

ros2 run peripherals_key_node key_node \
  --ros-args \
  --params-file output/riscv64/staging/share/peripherals_key_node/params/key_node.yaml
```

## 测试验证

### 1. 组件层验证

先验证底层组件本身可用：

```bash
./build/build.sh package components/peripherals/key
sudo output/riscv64/staging/bin/test_key
```

确认物理按键操作时能看到底层打印的按下、释放、单击、双击、长按、连发。

### 2. ROS 2 话题验证

启动节点后执行：

```bash
ros2 topic echo /key/events
```

依次做以下动作并观察输出：

1. 短按一次，应依次看到 `event_type=1`、`2`，随后 `3`
2. 快速按两次，应看到第二次释放后出现 `event_type=4`
3. 长按超过阈值，应出现 `event_type=5`
4. 持续按住，应周期性出现 `event_type=8`

### 3. 多节点订阅验证

按键事件是广播型接口，建议同时开两个订阅端：

```bash
ros2 topic echo /key/events
ros2 topic hz /key/events
```

确认多个订阅者都能收到相同事件，说明接口模型适合话题广播。

### 4. 回归检查

建议至少确认以下内容：

- 参数数组长度不一致时节点应启动失败
- GPIO 号错误时节点应明确报错退出
- 未授予 `/dev/gpiochip*` 权限时节点应启动失败
- 编译后安装目录中存在：
  - `share/peripherals_key_node/package.sh`
  - `lib/peripherals_key_node/key_node`
