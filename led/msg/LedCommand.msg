std_msgs/Header header
uint32 led_id
uint8 r
uint8 g
uint8 b
uint8 brightness      # 0-255
uint8 mode            # 0=STATIC, 1=BLINK, 2=BREATH
uint16 period_ms      # 闪烁/呼吸周期
uint16 on_ms          # 闪烁时开启时长
uint8 count           # 重复次数，0=无限
