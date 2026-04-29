std_msgs/Header header
uint8[] uid           # 最大16字节
uint8 uid_len
uint8 tag_type        # 0=UNKNOWN, 1=MIFARE_CLASSIC, ...
int8 rssi_dbm
uint8[] ats           # 最大32字节
uint8 ats_len
