# peripherals_wifi_node

## 接口设计

`components/peripherals/wifi` 底层能力本质上分成两类：

- **命令式操作**：连接、断开、扫描，这些都是一次调用一次返回
- **状态快照**：当前是否联网、当前 SSID、IP、BSSID 等，需要持续对外可见

因此 WiFi 的 ROS 2 接口不适合做成纯 topic，也不适合做成纯 service，更合理的是 **混合模式**：

- **service**：处理 `connect / disconnect / scan` 这类命令
- **topic**：周期发布当前 WiFi 状态，必要时广播扫描结果

本节点采用：

- 状态输出：`peripherals_wifi_node/msg/WifiStatus`
- 扫描结果输出：`peripherals_wifi_node/msg/WifiScanResults`
- 连接服务：`peripherals_wifi_node/srv/WifiConnect`
- 断开服务：`std_srvs/srv/Trigger`
- 扫描服务：`peripherals_wifi_node/srv/WifiScan`

### 为什么这是更合理的实现方式

- **连接/断开/扫描适合 service**
  这些动作都需要明确输入参数和执行结果，例如密码错误、设备未准备好、扫描条目数限制等，service 更自然。
- **状态适合 topic**
  当前 WiFi 是否已连接通常不止一个节点关心，做成周期状态话题后，上层状态机、UI、诊断模块都能直接订阅。
- **断开后仍能拿到状态**
  这是本实现的关键点。节点不是依赖 `wifi_sta_get_info()` 判断状态，因为该接口在断开后会返回失败；节点改为以 `wifi_get_state()` 为主做状态判断，只在“已连接”时再补充 `wifi_sta_get_info()` 的 SSID/IP/BSSID 细节。这样断网后仍会持续发布 `connected=false` 的状态快照。
- **状态话题使用 transient_local**
  新订阅者即使在 WiFi 断开之后才启动，也能立即拿到最近一次发布的“已断开”状态，而不必等下一个周期。

当前实现聚焦 **STA（station）模式**。原因是现有 ROS IDL 只定义了站点侧连接和状态语义，尚未覆盖 AP/SoftAP 的完整配置与客户端列表。如果后续要暴露 AP 管理，建议补专门的消息与服务，而不是把 AP 语义塞进当前接口。

## 参数

- `frame_id`：状态与扫描消息的 `header.frame_id`，默认 `wifi`
- `status_topic`：状态话题，默认 `wifi/status`
- `scan_topic`：扫描结果话题，默认 `wifi/scan_results`
- `connect_service`：连接服务名，默认 `wifi/connect`
- `disconnect_service`：断开服务名，默认 `wifi/disconnect`
- `scan_service`：扫描服务名，默认 `wifi/scan`
- `enable_on_startup`：启动时是否自动 `wifi_on(WIFI_MODE_STATION)`，默认 `true`
- `auto_reconnect`：启动后是否尝试配置底层自动重连，默认 `true`
- `status_period_ms`：状态发布周期，默认 `2000`
- `scan_period_ms`：周期扫描间隔，默认 `0`，表示关闭周期扫描
- `scan_on_startup`：启动后是否立即扫描一次并发布，默认 `false`
- `scan_max_results`：单次最多发布/返回多少条扫描结果，默认 `32`，合法范围 `1..256`
- `scan_ssid`：节点默认扫描过滤条件，默认空字符串表示不过滤；当 `WifiScan.srv` 里的 `ssid` 为空时使用这里的值

参数示例见 [params/wifi_node.yaml](/home/huanghaiqiang/docker_cross_test/robotic_sdk_not_git/middleware/ros2/peripherals/wifi/params/wifi_node.yaml)。

## 构建前提

先确保以下包已经编译并安装到统一前缀：

```bash
./build/build.sh package components/peripherals/wifi
```

再编译本节点：

```bash
./build/build.sh package middleware/ros2/peripherals/wifi
```

如果你本地 `output/.../staging` 里已经存在旧版共享 `peripherals` 接口安装产物，请先清理旧安装结果或重装当前 `middleware/ros2/peripherals/wifi` 包，避免 `WifiConnect.srv` 仍然命中旧类型。

## 运行

```bash
source /opt/ros/humble/setup.bash
source output/riscv64/staging/setup.bash

ros2 run peripherals_wifi_node wifi_node \
  --ros-args \
  --params-file output/riscv64/staging/share/peripherals_wifi_node/params/wifi_node.yaml
```

如果这个节点管理的正是当前 ROS 2 正在使用的网络接口（例如 `wlan0`），断开 WiFi 后 DDS 发现可能随接口一起丢失，表现为本机再执行 `ros2 topic list` 只能看到 `/rosout` 和 `/parameter_events`。这种场景建议在启动节点前手动限制到本地回环：

```bash
export ROS_LOCALHOST_ONLY=1
ros2 run peripherals_wifi_node wifi_node \
  --ros-args \
  --params-file output/riscv64/staging/share/peripherals_wifi_node/params/wifi_node.yaml
```

同时建议本机执行 `ros2 topic echo`、`ros2 topic list`、`ros2 service call` 的终端也导出同样的环境变量，让 CLI 在 `wlan0` 断开后仍能通过 `lo` 继续发现并读取 `/wifi/status`。代价是该节点不会再被其他主机通过网络发现。

## 测试验证

### 1. 组件层验证

先验证底层组件本身可用：

```bash
./build/build.sh package components/peripherals/wifi
output/riscv64/staging/bin/test_wifi_demo scan
output/riscv64/staging/bin/test_wifi_demo info
```

如果要直接验证底层连接流程：

```bash
output/riscv64/staging/bin/test_wifi_demo connect <ssid> [password]
output/riscv64/staging/bin/test_wifi_demo disconnect
output/riscv64/staging/bin/test_wifi_demo state
```

其中最后一条很重要，用来确认底层在断开后仍能返回 `sta_state=DISCONNECTED`。

### 2. 状态话题验证

启动节点后执行：

```bash
ros2 topic echo /wifi/status
```

如果当前已联网，应看到：

- `connected=true`
- `ssid` 为当前网络
- `ip_addr` 为有效 IPv4

如果当前未联网或手动断开，应看到：

- `connected=false`
- `ssid` 为空字符串
- `ip_addr` 为 `0.0.0.0`

### 3. 扫描验证

先监听扫描结果：

```bash
ros2 topic echo /wifi/scan_results
```

再触发一次扫描：

```bash
ros2 service call /wifi/scan peripherals_wifi_node/srv/WifiScan "{ssid: '', max_results: 0}"
```

说明：

- `ssid: ''` 表示使用节点默认过滤条件
- `max_results: 0` 表示使用节点参数里的 `scan_max_results`

服务会直接返回结果，同时节点也会向 `/wifi/scan_results` 发布同一批结果。

### 4. 连接验证

连接指定 WiFi：

```bash
ros2 service call /wifi/connect peripherals_wifi_node/srv/WifiConnect "{ssid: '<ssid>', password: '<password>', bssid: [0, 0, 0, 0, 0, 0]}"
```

观察：

```bash
ros2 topic echo /wifi/status
```

确认状态变为：

- `connected=true`
- `ssid` 为目标网络

### 5. 断开后状态验证

执行断开：

```bash
ros2 service call /wifi/disconnect std_srvs/srv/Trigger "{}"
```

然后继续观察：

```bash
ros2 topic echo /wifi/status
```

应看到节点继续正常发布状态，并切换为：

- `connected=false`
- `ssid=''`
- `ip_addr=[0, 0, 0, 0]`

如果重新开一个终端在断开后再执行一次 `ros2 topic echo /wifi/status`，也应该能立刻收到最近一次状态，这是 `transient_local` 的效果。

### 6. 回归检查

建议至少确认以下内容：

- `nmcli` 或 NetworkManager 不可用时节点应启动失败
- `status_period_ms <= 0` 时节点应启动失败
- `scan_period_ms < 0` 时节点应启动失败
- `scan_max_results <= 0` 或 `> 256` 时节点应启动失败
- 连接失败后节点仍能继续发布状态
- 断开连接后节点仍能正常返回并发布 `connected=false`
