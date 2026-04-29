# 电机节点使用说明

本文档用于说明本目录下 `motor_node` 的使用方法，涵盖三种电机类型、支持的控制模式以及常用的 ROS 2 指令。内容包括：如何启动节点、发布控制命令、订阅状态、使用服务调参以及通过参数服务器动态调节控制参数。

## 1. 电机类型与控制模式概览

| 电机类型 | 驱动示例            | 支持的模式（motor_mode 枚举）                                                                  |
|----------|---------------------|--------------------------------------------------------------------------------------------------|
| CAN      | `drv_can_dm`（达妙） | `IDLE`、`POS`、`VEL`、`TRQ`、`HYBRID`（映射到 MIT 阻抗控制，指令下发前自动切换底层模式）       |
| UART     | `drv_uart_feetech`   | `POS`、`VEL`（伺服/轮式模式），`IDLE` 由零力矩命令实现                                         |
| EtherCAT | `drv_ethercat_jmc`         | `POS`(PP)、`VEL`(PV)、`TRQ`(PT)、`CSP`、`CSV`、`CST`、`HM` 及 `IDLE`                            |

> **提示**：所有模式都由 `motor_node` 统一校验，若某模式不受支持，将跳过该电机的控制并在控制台输出告警。

## 2. 编译说明

在构建 ROS 2 节点之前，请确保 `components/peripherals/motor` 目录下的底层驱动已完成编译并安装，否则 `motor_node` 无法链接或加载驱动插件。完成前置依赖后，可以通过以下两种方式编译本目录：

### 方法一：使用项目统一构建系统
```bash
# 在项目根目录
source build/envsetup.sh
lunch   # 选择合适的方案，例如输入 2

# 切换到本目录后执行模块编译
cd middleware/ros2/peripherals/motor
mm
```
该方式会自动复用项目的交叉编译/依赖配置，并将结果安装到预设输出路径。

### 方法二：本地 colcon 构建
```bash
cd middleware/ros2/peripherals/motor
colcon build
source install/setup.bash
```
适用于快速迭代或本地调试。若需覆盖 ROS 环境，请在执行 `colcon build` 前确保已 `source /opt/ros/<distro>/setup.bash`。

## 3. 启动 motor_node

`motor_node` 由 `peripherals-motor` 包构建。推荐通过 `config/` 目录下的 YAML 文件统一配置不同电机类型，示例如下：

```bash
# 载入 ROS 2 环境（以 Humble 为例）
source /opt/ros/humble/setup.bash

# 载入该工作空间（若使用 colcon 安装在 install/）
source install/setup.bash

# 运行 motor_node（CAN 示例）
ros2 run peripherals_motor_node motor_node \
  --ros-args --params-file config/can_motor_params.yaml
```

### EtherCAT 启动示例
```bash
ros2 run peripherals_motor_node motor_node \
  --ros-args --params-file config/ecat_motor_params.yaml
```

### UART 启动示例
```bash
ros2 run peripherals_motor_node motor_node \
  --ros-args --params-file config/uart_motor_params.yaml
```

如需新增配置，可参考上述 YAML 文件结构复制修改。

### 启动后的使能操作

除 EtherCAT 电机外，节点启动后默认处于禁用状态，需要先调用 `/motor/enable`（`std_srvs/SetBool`）服务，才能正常发布 `/cmd/motor` 指令：

```bash
# 使能
ros2 service call /motor/enable std_srvs/srv/SetBool "{data: true}"

# 失能（强制 IDLE）
ros2 service call /motor/enable std_srvs/srv/SetBool "{data: false}"
```

失能后会发送零力矩/零速度，设备保持初始化状态，重新使能即可恢复。

## 4. 发布电机控制命令

`motor_node` 订阅 `/cmd/motor` 话题（类型 `peripherals::msg::MotorCommandArray`）。每条命令包含 `{id, mode, pos_des, vel_des, trq_des, kp, kd}`。示例：

```bash
ros2 topic pub --once /cmd/motor peripherals/msg/MotorCommandArray "{commands: [
  {id: 1, mode: 4, pos_des: 0.0, vel_des: 0.0, trq_des: 0.0, kp: 5.0, kd: 0.1}
]}"
```

模式说明：
- `mode=0`：IDLE，发送零力矩（MIT）命令
- `mode=1`：位置模式（POS）
- `mode=2`：速度模式（VEL）
- `mode=3`：力矩模式（TRQ）
- `mode=4`：混合/MIT 模式。

EtherCAT 还扩展支持 `5=CSP`, `6=CSV`, `7=CST`, `8=HM`

UART feettech 只支持 `1=POS`, `2=VEL`



## 5. 订阅电机状态

节点将状态发布至 `/motor/state`（`peripherals::msg::MotorStateArray`），包含 `id、pos、vel、trq、temp、error_flags` 等字段：

```bash
ros2 topic echo /motor/state
```

默认发布频率由 `state_publish_hz` 控制（默认 100 Hz）。若在 `cmd_timeout_ms`（默认 200 ms）内未收到新指令，节点会进行安全停车：保留当前模式但将速度与力矩归零。

## 6. 电机寄存器参数服务

### 写参数 `/motor/set_param`
- 服务类型：`peripherals/srv/MotorSetParam`
- 请求字段：`motor_id`, `reg_address`, `value`
- CLI 示例：
  ```bash
  ros2 service call /motor/set_param peripherals/srv/MotorSetParam \
    "{motor_id: 1, reg_address: 16, value: 1.0}"
  ```
- 底层通过 `motor_set_paras` 将 `reg_address` 作为 `uintptr_t` 传给驱动，驱动内部会校验权限与模式。

### 读参数 `/motor/get_param`
- 服务类型：`peripherals/srv/MotorGetParam`
- CLI 示例：
  ```bash
  ros2 service call /motor/get_param peripherals/srv/MotorGetParam \
    "{motor_id: 1, reg_address: 16}"
  ```
- 返回 `value` 和 `message`，达妙 CAN 驱动会根据权限表限制可访问寄存器。

#### EtherCAT `reg_address` 打包规则

EtherCAT CoE 寄存器同时包含 Index、SubIndex 以及读写数据长度。节点不拆解 `reg_address`，而是直接将其透传给底层驱动。因此在调用上述服务时，请将三个字段打包成一个 32-bit 数值：

```
reg_address = (data_len_bytes << 24) | (subindex << 16) | index
```

- `index`：低 16 位，十六进制表示如 `0x6064`
- `subindex`：第 16~23 位，范围 `0-255`
- `data_len_bytes`：第 24~31 位，表示本次读写的数据字节数

示例：读取位置反馈（Index `0x6064`，SubIndex `0x00`，4 字节）可传入 `reg_address = (4 << 24) | (0 << 16) | 0x6064 = 0x04006064`。写入 Profile Velocity（Index `0x6081`，SubIndex `0x00`，4 字节）则为 `0x04006081`。只要 ROS 侧调用保持一致的打包方式，底层驱动即可正确解析。

set param 同理

> **提示**：可通过脚本循环调用服务，方便批量调参。

## 7. 通过 ros2 param set 动态调参

`motor_node` 对外暴露控制增益等参数，可运行时修改：

```bash
# 动态调整 Kp
ros2 param set /motor_node pid_kp "[6.0, 6.0, 6.0]"

# EtherCAT 更新 profile 速度（下次发指令时生效）
ros2 param set /motor_node profile_vel "[120000.0, 120000.0]"
```

某些参数（如 motor_type、driver_name、motor_iface 等）被设为只读，节点会拒绝修改请求并返回错误。

## 8. 指令超时与安全停车

- `cmd_timeout_ms`（默认 200 ms）用于检测指令超时，超过后立即触发安全停车。
- 安全停车保持原模式，仅将 `vel_des`、`trq_des` 清零，避免 EtherCAT/达妙在模式切换时出现收敛震荡。

## 9. 配置文件

`config/` 目录提供预配置的参数文件：

- `can_motor_params.yaml`：达妙 CAN 配置（含 MIT 参数）。
- `uart_motor_params.yaml`：飞特 UART 配置。
- `ecat_motor_params.yaml`：EtherCAT JMC 的周期、Profile 速度/加速度设定。

可通过如下命令加载：
```bash
ros2 run peripherals_motor_node motor_node --ros-args --params-file config/can_motor_params.yaml
```

## 10. 常见问题

1. **电机不动**：确认 `/motor/enable` 为 true，并查看控制台告警。
2. **模式不支持**：检查 `motor_type` 与驱动是否实现该 `mode`。
3. **驱动初始化失败**：确认硬件连接无误（CAN 是否 up、UART 是否可访问、EtherCAT 主站是否运行）。
4. **读写寄存器失败**：达妙某些寄存器只读，驱动内有权限表限制。

---

通过本指南，可快速了解每种电机的可用模式、命令发布方式、状态订阅流程以及多种调参手段，帮助运维人员高效调试 `motor_node`。
