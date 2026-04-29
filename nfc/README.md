# peripherals_nfc_node

## 接口设计

`components/peripherals/nfc` 底层同时包含两类能力：

- **标签检测**：是否有卡靠近读卡器、当前卡的 UID/类型等，这类信息天然是异步出现的
- **块读写**：按块读取或写入数据，这类操作天然是请求-响应模型

因此 NFC 的 ROS 2 接口不适合做成“纯 service”或“纯 topic”，更合适的是 **混合模式**：

- **话题**：用于广播“检测到标签”这类异步事件
- **服务**：用于 `poll/read/write` 这类一次性调用

本节点采用：

- 检测输出：`peripherals_nfc_node/msg/NfcTagInfo`
- 默认话题：`/nfc/tag_detected`
- 轮询服务：`/nfc/poll`
- 读块服务：`/nfc/read_block`
- 写块服务：`/nfc/write_block`

设计取舍如下：

- **为什么不是纯 service**
  检测到标签通常是多个节点都关心的广播事件，只靠服务轮询会增加上层复杂度，也不利于多个订阅方共享同一结果。
- **为什么不是纯 topic**
  `read_block` / `write_block` 这类操作需要明确的请求参数和返回结果，服务比话题更自然，也更容易表达错误信息。
- **为什么还保留 `/nfc/poll`**
  有些流程明确需要“现在立刻测一次有没有卡”，例如上层状态机或测试脚本；这时服务比等被动话题更合适。

## 参数

- `transport`：底层总线类型，`i2c` / `spi` / `uart`
- `driver`：底层驱动名；I2C SI512 默认填 `SI512`
- `name`：设备实例名，仅用于组成底层实例名和日志
- `device`：设备路径；例如 `/dev/i2c-5`
- `i2c_addr`：I2C 地址，默认 `0x28`
- `cs_pin`：SPI 片选脚，默认 `0`
- `baud`：UART 波特率，默认 `115200`
- `frame_id`：消息头 `frame_id`，默认 `nfc`
- `tag_topic`：标签检测话题，默认 `nfc/tag_detected`
- `poll_service`：轮询服务名，默认 `nfc/poll`
- `read_service`：读块服务名，默认 `nfc/read_block`
- `write_service`：写块服务名，默认 `nfc/write_block`
- `auto_poll_enabled`：是否启用后台轮询发布标签话题，默认 `true`
- `poll_period_ms`：后台轮询周期，默认 `100`
- `poll_timeout_ms`：每次轮询调用 `nfc_poll()` 的超时，默认 `50`
- `publish_duplicates`：同一张卡持续贴住时是否重复发布，默认 `false`

参数示例见 [params/nfc_node.yaml](/home/huanghaiqiang/docker_cross_test/robotic_sdk_not_git/middleware/ros2/peripherals/nfc/params/nfc_node.yaml)。

注意：

- 本节点默认 I2C 驱动名是 `SI512`，因此底层实际传给 `nfc_alloc_i2c()` 的名字是 `SI512:nfc0`
- `write_block` 必须传入 **16 字节**，因为当前底层 `nfc_write_block()` 就是按 16 字节块实现的
- `publish_duplicates=false` 时，同一张卡一直不离开读卡器，只会首次检测时发一次话题；拿开后再贴回才会重新发

## 构建前提

先确保以下包已经编译并安装到统一前缀：

```bash
./build/build.sh package components/peripherals/nfc
```

再编译本节点：

```bash
./build/build.sh package middleware/ros2/peripherals/nfc
```

## 运行

```bash
source /opt/ros/humble/setup.bash
source output/riscv64/staging/setup.bash

ros2 run peripherals_nfc_node nfc_node \
  --ros-args \
  --params-file output/riscv64/staging/share/peripherals_nfc_node/params/nfc_node.yaml
```

## 测试验证

### 1. 组件层验证

先验证底层组件本身可用：

```bash
./build/build.sh package components/peripherals/nfc
sudo output/riscv64/staging/bin/test_nfc_i2c /dev/i2c-5 0x28 4
```

确认底层能正常检测标签，并且读写返回值符合预期。

### 2. 话题验证

启动节点后执行：

```bash
ros2 topic echo /nfc/tag_detected
```

将卡片贴近读卡器，应看到 `uid`、`uid_len`、`tag_type` 等字段。

### 3. 服务验证

轮询一次是否有标签：

```bash
ros2 service call /nfc/poll peripherals_nfc_node/srv/NfcPoll "{timeout_ms: 200}"
```

读取 4 号块：

```bash
ros2 service call /nfc/read_block peripherals_nfc_node/srv/NfcReadBlock "{block_addr: 4}"
```

写入 4 号块：

```bash
ros2 service call /nfc/write_block peripherals_nfc_node/srv/NfcWriteBlock "{block_addr: 4, data: [1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16]}"
```

### 4. 回归检查

建议至少确认以下内容：

- 未授予设备节点权限时，节点应启动失败
- 驱动名错误时，节点应明确报错退出
- `write_block.data` 不是 16 字节时，服务应返回失败
- 同一张卡持续贴住时，默认只发布一次 `tag_detected`
