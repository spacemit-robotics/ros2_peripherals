# 模式常量定义
uint8 MODE_IDLE=0      # 空闲（禁用力矩输出）
uint8 MODE_POS=1       # 位置模式
uint8 MODE_VEL=2       # 速度模式
uint8 MODE_TRQ=3       # 力矩模式
uint8 MODE_HYBRID=4    # MIT 阻抗混合控制（CAN 电机）
uint8 MODE_CSP=5       # 周期同步位置模式（EtherCAT）
uint8 MODE_CSV=6       # 周期同步速度模式（EtherCAT）
uint8 MODE_CST=7       # 周期同步力矩模式（EtherCAT）
uint8 MODE_HM=8        # 回零模式（EtherCAT）

uint32 id              # 电机ID
uint8 mode             # 控制模式，取值见上方 MODE_* 常量
float64 pos_des        # 期望位置 (rad)
float64 vel_des        # 期望速度 (rad/s)
float64 trq_des        # 期望力矩 (Nm)
float64 kp             # 刚度增益 (HYBRID) / 加速度 (EtherCAT PP)
float64 kd             # 阻尼增益 (HYBRID) / 减速度 (EtherCAT PP)


