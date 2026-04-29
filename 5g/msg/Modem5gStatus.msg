std_msgs/Header header
bool snapshot_ok
int32 snapshot_status_code
string snapshot_message

string device_name
string uart_device
uint32 baud
bool flight_mode_enabled

string manufacturer
string model
string revision
string imei

uint8 power_state     # 0=OFF, 1=ON, 2=RESETTING
uint8 sim_state       # 0=UNKNOWN, 1=ABSENT, 2=READY, 3=PIN_REQUIRED, 4=PUK_REQUIRED, 5=ERROR
string iccid
string imsi
string msisdn

uint8 reg_state       # 0=NOT_REGISTERED, 1=REGISTERED_HOME, 2=SEARCHING, 3=DENIED, 4=UNKNOWN, 5=REGISTERED_ROAMING
uint8 rat             # 0=UNKNOWN, 1=NR5G_SA, 2=NR5G_NSA, 3=LTE, 4=WCDMA, 5=GSM
string mcc
string mnc
uint32 tac
uint32 cell_id
uint16 pci
uint32 arfcn
string band
string operator_name

int16 rssi_dbm
int16 rsrp_dbm
int16 rsrq_db
int16 sinr_db

uint8 data_state      # 0=DISCONNECTED, 1=CONNECTING, 2=CONNECTED
uint8 active_cid
Modem5gPdpContext pdp_context
Modem5gIpInfo ip_info
