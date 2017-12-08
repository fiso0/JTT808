
#if 1
#ifndef __MXAPP_SRVINTERACTION_JTT_H__
#define __MXAPP_SRVINTERACTION_JTT_H__

#include "others.h"

typedef enum _SRV_CMD_TYPE_
{
	MXAPP_SRV_JTT_CMD_UP_ACK = 0x0001, // 终端通用应答
	MXAPP_SRV_JTT_CMD_UP_HEARTBEAT = 0x0002, // 终端心跳
	MXAPP_SRV_JTT_CMD_UP_REGISTER = 0x0100, // 终端注册
	MXAPP_SRV_JTT_CMD_UP_DEREGISTER = 0x0003, // 终端注销
	MXAPP_SRV_JTT_CMD_UP_AUTHORIZE = 0x0102, // 终端鉴权
	MXAPP_SRV_JTT_CMD_UP_QUERY_PARA_ACK = 0x0104, // 查询终端参数应答
	MXAPP_SRV_JTT_CMD_UP_QUERY_PROP_ACK = 0x0107, // 查询终端属性应答
	MXAPP_SRV_JTT_CMD_UP_LOC_REPORT = 0x0200, // 位置信息汇报
	MXAPP_SRV_JTT_CMD_UP_LOC_QUERY_ACK = 0x0201, // 位置信息查询应答
	MXAPP_SRV_JTT_CMD_UP_EVENT_REPORT = 0x0301, // 事件报告
	
	MXAPP_SRV_JTT_CMD_UP_BATTERY = 0x0F01, // 电量信息
	MXAPP_SRV_JTT_CMD_UP_CARD_LOGIN = 0x0F02, // 卡片登入

	MXAPP_SRV_JTT_CMD_DOWN_ACK = 0x8001, // 平台通用应答
	MXAPP_SRV_JTT_CMD_DOWN_REGISTER_ACK = 0x8100, // 终端注册应答
	MXAPP_SRV_JTT_CMD_DOWN_SET_PARA = 0x8103, // 设置终端参数
	MXAPP_SRV_JTT_CMD_DOWN_QUERY_PARA = 0x8104, // 查询终端参数
	MXAPP_SRV_JTT_CMD_DOWN_CONTROL = 0x8105, // 终端控制
	MXAPP_SRV_JTT_CMD_DOWN_QUERY_SPEC_PARA = 0x8106, // 查询指定终端参数
	MXAPP_SRV_JTT_CMD_DOWN_QUERY_PROP = 0x8107, // 查询终端属性
	MXAPP_SRV_JTT_CMD_DOWN_LOC_QUERY = 0x8201, // 位置信息查询
	MXAPP_SRV_JTT_CMD_DOWN_EVENT_SET = 0x8301, // 事件设置
#ifdef __WHMX_CALL_SUPPORT__
	MXAPP_SRV_JTT_CMD_DOWN_CALL_MONITOR = 0x8400, // 电话回拨
	MXAPP_SRV_JTT_CMD_DOWN_PHONEBOOK = 0x8401, // 设置电话本
#endif
#ifdef __WHMX_JTT_FENCE_SUPPORT__
#endif

	MXAPP_SRV_JTT_CMD_INVALID = 0xFFFF
}SRV_ZXBD_CMD_TYPE;

typedef struct
{
	SRV_ZXBD_CMD_TYPE cmd;
	kal_int32(*handle)(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out);
}mx_cmd_handle_struct;


//终端参数: 参数ID
typedef enum _SRV_PARA_ID_
{
	MXAPP_SRV_JTT_PARA_HEARTBEAT = 0x0001,
	MXAPP_SRV_JTT_PARA_SRV_IP = 0x0013,
	MXAPP_SRV_JTT_PARA_SRV_TCP_PORT = 0x0018,
	MXAPP_SRV_JTT_PARA_MONITOR_NUM = 0x0048,
	MXAPP_SRV_JTT_PARA_LOC_TYPE = 0x0094,
	MXAPP_SRV_JTT_PARA_LOC_PROP = 0x0095,

	/*自定义*/
	MXAPP_SRV_JTT_PARA_BATTERY = 0xF001,

	MXAPP_SRV_JTT_PARA_INVALID = 0x0000
}SRV_PARA_ID;

//设置终端参数: 参数ID-处理函数
typedef struct
{
	SRV_PARA_ID para_id;
	kal_int32(*handle)(kal_uint8 *in, kal_uint32 in_len);
}mx_set_para_handle_struct;

//查询终端参数: 参数ID-处理函数
typedef struct
{
	SRV_PARA_ID para_id;
	kal_int32(*handle)(kal_uint8 *out/*, kal_uint32 out_len*/);
}mx_get_para_handle_struct;

// 终端联网进度
typedef struct
{
	kal_uint8 status; // 初始:0 鉴权中:1 注销中:2 注册中:3 鉴权成功:4
	kal_uint16 flow;
}mx_login_status;

typedef void(*mx_srv_cb)(void *);


// new: 事件ID
typedef enum _SRV_EVENT_ID_
{
	MXAPP_SRV_JTT_EVENT_LOW_POWER = 0xF1, // 低电
	MXAPP_SRV_JTT_EVENT_NO_POWER = 0xF2, // 无电
	MXAPP_SRV_JTT_EVENT_INVALID = 0xFF
}SRV_EVENT_ID;


/*新接口*/
kal_int32 mx_srv_receive_handle_jtt(kal_uint8 src, kal_uint8 *in, kal_int32 in_len);
kal_int32 mx_srv_heartbeat_jtt(void);
kal_int32 mx_srv_loc_report_jtt(void);
kal_int32 mx_srv_ack_jtt(kal_uint8 *para, kal_uint32 para_len);
kal_int32 mx_srv_send_battery_jtt(kal_bool poweroff);
kal_int32 mx_srv_send_card_login_jtt(void);
kal_int32 mx_srv_config_nv_write(kal_uint8 item);
kal_int32 mx_srv_config_nv_read(void);
void mx_srv_auth_code_clear(void);
kal_int32 mxapp_srvinteraction_send_event(SRV_EVENT_ID evt);


/*原接口*/
void mxapp_srvinteraction_connect(kal_int32 s32Level);
void srvinteraction_bootup_location_request(void);
void mxapp_srvinteraction_first_location(void);

kal_int32 mxapp_srvinteraction_uploader_pos_mode(void);
kal_int32 mxapp_srvinteraction_uploader_config(void);
kal_int32 mxapp_srvinteraction_uploader_batt_info(void);
kal_int32 mxapp_srvinteraction_send_battery_warning(void); 

kal_uint8 mx_pos_mode_set(kal_uint8 mode);
kal_uint8 mx_pos_period_set(kal_uint32 period_min);

void mxapp_srvinteraction_sos(void);

void mxapp_srvinteraction_locate_and_poweroff(void); // 5%低电关机

kal_bool mxapp_srvinteraction_if_connected(void); // 2016-6-22
kal_bool mxapp_srvinteraction_is_connected(void);

#if defined(__WHMX_CALL_SUPPORT__)
extern kal_uint8 mx_srv_cmd_location_status_get(void);
extern void mx_srv_cmd_location_status_set(kal_uint8 ret);
#endif


#endif
#endif

