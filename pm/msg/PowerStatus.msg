std_msgs/Header header
uint8 status          # 0=UNKNOWN, 1=DISCHARGING, 2=CHARGING, 3=FULL, 4=FAULT, 5=SLEEP
float32 percentage    # 电量百分比 0.0-100.0
float32 health        # 健康度 0.0-100.0
uint32 cycle_count
uint32 error_code
uint8 cell_count
float32[] cell_voltages  # 最大16个
