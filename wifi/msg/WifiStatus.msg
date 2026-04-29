std_msgs/Header header
uint8 mode             # 0=UNKNOWN, 1=STATION, 2=AP, 3=STATION_AP
bool connected         # 是否已连接网络（station联网可用）
string ssid            # 当前SSID（若无则空）
int32 rssi_dbm         # 信号强度(dBm)，不可用可置0
uint32 secure          # 安全类型（bitmask/enum，参考wifi_secure）
uint8[6] bssid         # AP BSSID（若不可用全0）
uint8[4] ip_addr       # IPv4（若不可用全0）
