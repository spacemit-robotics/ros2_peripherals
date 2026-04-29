# peripherals_pm_node

## 接口设计

`components/peripherals/pm` 和 `key` 的模型不一样。

- `key` 是标准异步离散事件源，核心是“某个事件发生了”，所以纯话题最合适。
- `pm` 更像“持续状态 + 少量控制命令”：
  - 电池/供电状态是连续快照，多个节点都可能订阅
  - 电源开关是明确的一次性控制动作，需要请求-响应结果

因此 `pm` 的 ROS 2 接口更合理的方式是 **混合模式**：

- 状态输出：`peripherals_pm_node/msg/PowerStatus`
- 控制输入：`peripherals_pm_node/srv/SetPowerSwitch`

## 为什么不是照搬 key 的纯事件实现

底层 `pm` 虽然提供了 `pm_set_callback`，但当前语义和 `key` 的事件回调并不等价：

- 当前回调只覆盖“充放电状态变化”这一类事件，不是完整状态快照
- 当前 `GENERIC` 驱动会自己开线程轮询 `/sys/class/power_supply/*`
- `ADC-Linux` 和 `INA219` 驱动目前仍是占位实现，回调/采样能力并不统一

如果 ROS 2 节点完全依赖回调：

- 无法稳定获得完整状态快照
- 驱动间行为不一致
- 很难给新订阅者持续提供最新状态

所以本节点采用 **统一轮询发布状态**，并把 **开关控制** 暴露为 service。这种方式比 key 的“回调入队再刷 topic”更适合 `pm`。

## 驱动选择建议

当前最合理的默认实现是 `generic`：

- `generic` 直接读取 Linux `power_supply` 节点，当前代码路径最完整
- 对于只关心“是否在充电 + 电量百分比来源节点”这一类平台，落地成本最低
- `adc` / `ina219` 在底层组件中仍有明显 TODO，更适合作为后续扩展入口，不建议现在作为主实现

因此本节点默认参数就是 `driver=generic`，同时保留 `adc` / `ina219` 参数入口，避免后续驱动完善时再改 ROS 2 外部接口。

## 参数

- `driver`：驱动类型，支持 `generic`、`adc`、`ina219`，也兼容 `ADC-Linux` / `INA219` 风格写法
- `name`：底层实例名，默认 `main_batt`
- `frame_id`：状态消息 `header.frame_id`，默认 `power`
- `status_topic`：状态话题，默认 `power/status`
- `switch_service`：电源开关服务名，默认 `power/set_switch`
- `poll_hz`：状态采样/发布频率，默认 `1.0`
- `publish_on_startup`：启动后立即发布一次状态，默认 `true`
- `charger_node`：`generic` 驱动使用的充电状态节点名，默认 `ip2317-charger`
- `capacity_node`：`generic` 驱动使用的电量节点名，默认 `cw-bat`
- `device`：`adc` 或 `ina219` 驱动使用的设备路径
- `i2c_addr`：`ina219` I2C 地址，默认 `64` (`0x40`)
- `adc_scale`：`adc` 驱动分压比例，默认 `11.0`
- `capacity_mah`、`max_voltage`、`min_voltage`、`warn_voltage`、`crit_voltage`、`max_temp`：透传到底层 `pm_config`
- `switch_channels`：service 的通道号到字符串通道名的映射数组，例如 `["main_5v", "aux_12v"]`

参数示例见 [params/pm_node.yaml](/home/huanghaiqiang/docker_cross_test/robotic_sdk_not_git/middleware/ros2/peripherals/pm/params/pm_node.yaml)。

注意：

- 节点名是 `pm_node`，参数文件顶层键必须写成 `pm_node`
- `SetPowerSwitch.srv` 的 `channel` 是 `uint8`，本节点通过 `switch_channels[channel]` 找到底层通道名
- 如果 `switch_channels` 没配，service 仍然存在，但会返回“channel 未配置”
- 当前 `peripherals_pm_node/msg/PowerStatus` 已经能表达 `status`、`percentage`、`health`、`cycle_count`、`error_code` 和单体电压，但**仍然不能完整承载底层 `pm_state` 的 voltage/current/power/temperature**

如果后续业务明确需要电压、电流、功率、温度等全量电池数据，建议继续扩展 `PowerStatus.msg` 或新增专门的电池状态消息；否则 ROS 2 层仍会丢字段。

## 构建前提

先确保以下包已编译并安装到统一前缀：

```bash
./build/build.sh package components/peripherals/pm
```

再编译本节点：

```bash
./build/build.sh package middleware/ros2/peripherals/pm
```

## 运行

```bash
source /opt/ros/humble/setup.bash
source output/riscv64/staging/setup.bash

ros2 run peripherals_pm_node pm_node \
  --ros-args \
  --params-file output/riscv64/staging/share/peripherals_pm_node/params/pm_node.yaml
```

## 测试验证

### 1. 组件层验证

优先验证底层 `generic` 驱动：

```bash
./build/build.sh package components/peripherals/pm
output/riscv64/staging/bin/test_pm_generic ip2317-charger cw-bat
```

观察是否能持续打印充电状态与容量，并在插拔电源时看到 callback 输出变化。

### 2. ROS 2 状态话题验证

启动节点后执行：

```bash
ros2 topic echo /power/status
```

然后做插拔电源测试：

1. 未充电时应看到 `status=1`（DISCHARGING）
2. 插入充电器后应切到 `status=2`（CHARGING）
3. `generic` 驱动下应能同时看到 `percentage` 跟随底层 `capacity` 节点变化，例如 `98.0`
4. 如果底层节点读取失败，节点应打印 `pm_get_state failed`

### 3. 电源开关服务验证

只有底层驱动真正实现了 `pm_switch_set`，并且参数里配置了 `switch_channels`，这个 service 才能成功。

调用示例：

```bash
ros2 service call /power/set_switch peripherals_pm_node/srv/SetPowerSwitch "{channel: 0, enable: true}"
```

结果判定：

- 如果底层支持，会返回 `success=true`
- 当前 `generic` 驱动下，预期会返回失败，因为底层 `pm_switch_set` 目前是 `-ENOSYS`

这个失败是符合当前底层实现现状的，不是 ROS 2 封装本身的问题。

### 4. 推荐回归项

建议至少确认以下内容：

- `poll_hz <= 0` 时节点应启动失败
- `driver=generic` 但 `charger_node` / `capacity_node` 为空时应启动失败
- `driver=ina219` 且 `i2c_addr` 越界时应启动失败
- `switch_channels` 未配置时 service 应明确返回失败原因
- 编译安装后目录中存在：
  - `share/peripherals_pm_node/package.sh`
  - `lib/peripherals_pm_node/pm_node`
