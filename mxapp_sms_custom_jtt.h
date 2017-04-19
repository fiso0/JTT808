#include "others.h"


kal_int32 mx_sms_custom_jtt_handle(kal_uint8 src, kal_uint8 *in, kal_int32 in_len);

//typedef enum _SRV_CMD_TYPE_
//{
//	MXAPP_SRV_JTT_CMD_DOWN_SET_PARA = 0x8103, // 设置终端参数
//
//	MXAPP_SRV_JTT_CMD_INVALID = 0xFFFF
//}SRV_ZXBD_CMD_TYPE;
//
//typedef struct
//{
//	SRV_ZXBD_CMD_TYPE cmd;
//	kal_int32(*handle)(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out);
//}mx_cmd_handle_struct;
//
////终端参数: 参数ID
//typedef enum _SRV_PARA_ID_
//{
//	MXAPP_SRV_JTT_PARA_HEARTBEAT = 0x0001,
//	MXAPP_SRV_JTT_PARA_SRV_IP = 0x0013,
//	MXAPP_SRV_JTT_PARA_SRV_TCP_PORT = 0x0018,
//	MXAPP_SRV_JTT_PARA_LOC_PROP = 0x0095,
//
//	MXAPP_SRV_JTT_PARA_INVALID = 0x0000
//}SRV_PARA_ID;
//
////设置终端参数: 参数ID-处理函数
//typedef struct
//{
//	SRV_PARA_ID para_id;
//	kal_int32(*handle)(kal_uint8 *in, kal_uint32 in_len);
//}mx_set_para_handle_struct;
