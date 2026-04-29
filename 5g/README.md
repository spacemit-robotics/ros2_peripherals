# peripherals_5g_node

## 接口设计

`components/peripherals/5g` 的底层接口不是单纯的“状态流”，而是典型的 **同步命令接口**：

- 上电、下电、复位、飞行模式、偏好 RAT、PDP 配置、拨号、AT 透传
- 每个调用都有明确入参、执行过程和返回码

同时它又提供一组适合被持续观测的状态查询：

- 基础信息、SIM 信息、注册状态、信号强度、数据连接状态、IP 信息

因此 5G 的 ROS 2 接口最合适的方式不是纯 `topic`，也不是纯 `service`，而是 **混合模式**：

- **service server**：承载控制面动作，返回明确成功/失败和底层状态码
- **topic publisher**：周期发布当前 5G 状态快照，供上层状态机、UI、诊断模块订阅

本节点采用：

- 状态输出：`peripherals_5g_node/msg/Modem5gStatus`
- 控制服务：
  - `peripherals_5g_node/srv/Modem5gTrigger`：`power_on / power_off / reset`
  - `peripherals_5g_node/srv/Modem5gSetFlightMode`
  - `peripherals_5g_node/srv/Modem5gSetPreferRat`
  - `peripherals_5g_node/srv/Modem5gSetPdpContext`
  - `peripherals_5g_node/srv/Modem5gGetPdpContext`
  - `peripherals_5g_node/srv/Modem5gDataCall`
  - `peripherals_5g_node/srv/Modem5gSendAt`

### 为什么不做成纯发布订阅

- `power_on`、`reset`、`set_pdp_context`、`data_start` 这类动作都需要明确知道有没有执行成功
- 底层 API 本身就是函数调用并直接返回 `MODEM_5G_STATUS_*`
- 如果改成“发一条命令 topic 再等结果 topic”，上层会平白引入配对、超时、幂等和重试复杂度

### 为什么也不能只做 service

- 注册状态、信号、数据连接状态通常会被多个模块同时消费
- 持续状态更适合被周期发布，而不是让每个消费者各自轮询服务
- `transient_local` 状态话题可以让新订阅者启动后立即拿到最近一次快照

## 参数

- `name`：底层实例名，默认 `MR880A:mr880a0`
- `uart_device`：AT 串口，默认 `auto`
- `baud`：UART 波特率，默认 `9600`
- `frame_id`：状态消息 `header.frame_id`，默认 `modem_5g`
- `status_topic`：状态话题，默认 `modem_5g/status`
- `power_on_service`：上电服务名，默认 `modem_5g/power_on`
- `power_off_service`：下电服务名，默认 `modem_5g/power_off`
- `reset_service`：复位服务名，默认 `modem_5g/reset`
- `flight_mode_service`：飞行模式服务名，默认 `modem_5g/set_flight_mode`
- `prefer_rat_service`：偏好 RAT 服务名，默认 `modem_5g/set_prefer_rat`
- `set_pdp_service`：设置 PDP 服务名，默认 `modem_5g/set_pdp_context`
- `get_pdp_service`：获取 PDP 服务名，默认 `modem_5g/get_pdp_context`
- `data_call_service`：拨号/断开服务名，默认 `modem_5g/data_call`
- `send_at_service`：AT 透传服务名，默认 `modem_5g/send_at`
- `publish_on_startup`：启动后是否立即发布一次状态，默认 `true`
- `status_period_ms`：状态发布周期，默认 `2000`
- `default_cid`：默认 PDP CID，默认 `1`
- `default_pdp_type`：默认 PDP 类型，默认 `3`（`IPV4V6`）
- `default_apn`：默认 APN
- `default_username`：默认用户名
- `default_password`：默认密码
- `power_on_on_startup`：启动后是否主动上电，默认 `false`
- `apply_default_pdp_on_startup`：启动时是否按默认参数写入 PDP，默认 `false`
- `start_data_on_startup`：启动后是否自动拨号，默认 `false`
- `at_timeout_ms`：AT 透传默认超时，默认 `2000`
- `at_response_max_bytes`：AT 响应缓冲区大小，默认 `1024`

参数示例见 [params/modem_5g_node.yaml](/home/huanghaiqiang/workspace/k3_robot_sdk/robotics_dev/middleware/ros2/peripherals/5g/params/modem_5g_node.yaml)。

## 构建前提

先确保以下包已编译并安装到统一前缀：

```bash
./build/build.sh package components/peripherals/5g
```

再编译本节点：

```bash
./build/build.sh package middleware/ros2/peripherals/5g
```

## 运行

```bash
source /opt/ros/humble/setup.bash
source output/riscv64/staging/setup.bash

ros2 run peripherals_5g_node modem_5g_node \
  --ros-args \
  --params-file output/riscv64/staging/share/peripherals_5g_node/params/modem_5g_node.yaml
```

## 典型调用

启动后查看状态：

```bash
ros2 topic echo /modem_5g/status
```

设置 PDP：

```bash
ros2 service call /modem_5g/set_pdp_context peripherals_5g_node/srv/Modem5gSetPdpContext \
  "{cid: 1, pdp_type: 3, apn: 'ctnet', username: '', password: ''}"
```

启动数据连接：

```bash
ros2 service call /modem_5g/data_call peripherals_5g_node/srv/Modem5gDataCall \
  "{cid: 1, start: true}"
```

发送自定义 AT 指令：

```bash
ros2 service call /modem_5g/send_at peripherals_5g_node/srv/Modem5gSendAt \
  "{command: 'AT+CSQ', timeout_ms: 2000}"
```
