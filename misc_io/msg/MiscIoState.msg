std_msgs/Header header
uint32 io_id
uint8 type             # 0=GENERIC, 1=BUZZER, 2=RELAY, 3=SWITCH, 4=SENSOR
uint8 dir              # 0=INPUT, 1=OUTPUT
bool active            # 当前状态（按“active逻辑”解释）
