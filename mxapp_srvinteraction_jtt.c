#include "mxapp_srv_jtt.h"
#include "mxapp_srvinteraction_jtt.h"

/********************
以下内容放入.c
********************/
#define	MXAPP_ONENET_BUFF_MAX	(384)
#define AUTH_CODE_LEN 20 // 鉴权码长度 要保证最后为0
#define CELL_NUMBER_LEN 6 // 手机号长度
#define	FRAME_FLAG_SYMBOL	0x7E // 标识位

#define	PHONE_NO_LEN		MX_APP_CALL_NUM_MAX_LEN

static kal_uint8 g_s8TmpBuf[MXAPP_ONENET_BUFF_MAX];
static kal_uint8 g_s8RxBuf[MXAPP_ONENET_BUFF_MAX];
static kal_uint8 g_s8TxBuf[MXAPP_ONENET_BUFF_MAX];

static kal_int8 g_s8Numfamily[PHONE_NO_LEN];

static kal_uint8 auth_code[AUTH_CODE_LEN]; // 鉴权码
static kal_uint8 auth_code_len; // 鉴权码长度（默认0）
static kal_uint8 cell_number[CELL_NUMBER_LEN]; // TODO: （需考虑如何获得）终端手机号，[0]保存最高位

static kal_uint8 shutdown_is_active = 0;//系统正在关机
static kal_uint16 serial_no_up; // 上行消息流水号
static kal_uint16 serial_no_down = 0; // 下行消息流水号

mx_srv_cb mx_srv_send_handle_callback = NULL;

static nvram_ef_mxapp_pos_mode_t mx_pos_mode;

/*
	0x00 - 普通上传
	0x0C - 关机
	0x0D - 开机（初始值）
	0x10 - 点名上传（位置信息查询）
	0x11 - 短信透传
	大于0x80 - 报警求助
*/
static kal_uint8 pos_info_type = 0x0D;	/*初始值表示开机*/

kal_uint8 mx_pos_info_type_set(kal_uint8 alarm)
{
	// 关机 和 sos 状态优先级高，处于这2个状态的时候，其他状态忽略
	if ((alarm != 0x00) && (alarm != 0x0C) && ((pos_info_type >= 0x80) || (pos_info_type == 0x0C))) return 0;/*sos important*//*shutdown power important*/

	pos_info_type = alarm;

	return 1;
}

kal_uint8 mx_pos_info_type_get(void)
{
	return pos_info_type;
}

// 设置终端心跳发送间隔
// 返回: 0-成功
kal_int32 mx_srv_set_para_heartbeat(kal_uint8 *in, kal_uint32 in_len)
{
	kal_uint32 heartbeat = 0; // unit: s

	if (!in || in_len != 4) return -1;

	heartbeat |= in[0];
	heartbeat <<= 8;
	heartbeat |= in[1];
	heartbeat <<= 8;
	heartbeat |= in[2];
	heartbeat <<= 8;
	heartbeat |= in[3];

	// TODO: 设置终端心跳发送间隔

	return 0;
}

// 设置监听电话号码
// 返回: 0-成功
kal_int32 mx_srv_set_para_monitor_num(kal_uint8 *in, kal_uint32 in_len)
{
	kal_char *num = g_s8Numfamily;

	if (!in || in_len == 0) return -1;

	kal_mem_set(num, 0, sizeof(g_s8Numfamily));
	kal_mem_cpy(num, in, in_len);

#if defined(__WHMX_CALL_SUPPORT__)
	mxapp_call_save_number(MX_RELATION_MGR, num);
#elif defined(__WHMX_ADMIN_SUPPORT__)
	mxapp_admin_number_save(num);
#endif

	return 0;
}

// 设置定位数据上传方式
// 返回: 0-成功
// 参考mx_srv_cmd_handle_down_pos_interval_min
kal_int32 mx_srv_set_para_loc_type(kal_uint8 *in, kal_uint32 in_len)
{
	kal_uint8 loc_type = -1;

	if (!in || in_len != 1) return -1;

	loc_type = in[0];
	if (loc_type == 0 || loc_type == 1) // 0-省电模式 1-标准模式
	{
		mx_pos_mode.dev_mode = loc_type;
		mx_srv_config_nv_write();
		mx_pos_mode_set(mx_pos_mode.dev_mode);
	}

	return 0;
}

// 设置定位数据上传设置
// 返回: 0-成功
// 参考mx_srv_cmd_handle_down_period
kal_int32 mx_srv_set_para_loc_prop(kal_uint8 *in, kal_uint32 in_len)
{
	kal_uint32 loc_prop = -1; // unit: s
	kal_uint16 period_min = 0; // unit: min

	if (!in || in_len != 4) return -1;

	loc_prop |= in[0];
	loc_prop <<= 8;
	loc_prop |= in[1];
	loc_prop <<= 8;
	loc_prop |= in[2];
	loc_prop <<= 8;
	loc_prop |= in[3];

	period_min = loc_prop / 60;

	if (mx_pos_mode.dev_mode == 1) // 1-标准模式
	{
		mx_pos_mode.custom_period = period_min;
		mx_srv_config_nv_write();
		mx_location_period_change(period_min);
	}

	return 0;
}

static mx_set_para_handle_struct set_para_func[] =
{
	{ MXAPP_SRV_JTT_SET_PARA_HEARTBEAT, mx_srv_set_para_heartbeat }, // 终端心跳发送间隔
	{ MXAPP_SRV_JTT_SET_PARA_MONITOR_NUM, mx_srv_set_para_monitor_num }, // 监听电话号码
	{ MXAPP_SRV_JTT_SET_PARA_LOC_TYPE, mx_srv_set_para_loc_type }, // 定位数据上传方式
	{ MXAPP_SRV_JTT_SET_PARA_LOC_PROP, mx_srv_set_para_loc_prop }, // 定位数据上传设置

	{ MXAPP_SRV_JTT_SET_PARA_INVALID, NULL }
};

// 终端通用应答
// in: 待发送消息体内容（应答流水号、应答ID、结果）
static kal_int32 mx_srv_cmd_handle_up_ack(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	kal_int32 len_l = -1;
	kal_uint8 *dat_l = out;

	if (dat_l == NULL) return len_l;

	if (in&&in_len) // 必须有输入参数
	{
		kal_mem_cpy(dat_l, in, in_len);
		len_l = in_len;
	} 
	else
	{
		return len_l;
	}

	mxapp_trace("%s:len_l(%d=0x%x)", __FUNCTION__, len_l, len_l);

	return len_l;
}

// 终端心跳
static kal_int32 mx_srv_cmd_handle_up_heartbeat(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	kal_int32 len_l = -1;
	kal_uint8 *dat_l = out;
	kal_uint32 i = 0, len = 0;

	if (dat_l == NULL) return len_l;

	if (in&&in_len)
	{
		kal_mem_cpy(dat_l, in, in_len);
		len_l = in_len;
	}
	else
	{
		/*消息体为空*/
		len_l = 0;
		dat_l[len_l] = '\0';
	}

	mxapp_trace("%s:len_l(%d=0x%x)", __FUNCTION__, len_l, len_l);
	for (i = 0; i < len_l;)
	{
		mxapp_trace("%s\r\n", &dat_l[i]);
		i += 127;
	}

	return len_l;
}

// 终端注册
// 示例：7E 0100002B0000000000000000 002A006F31323334354D585431363038000000000000000000000000004D58543136303801415A31323334 40 7E
static kal_int32 mx_srv_cmd_handle_up_register(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	kal_uint16 province = 42;
	kal_uint16 city = 111;
	kal_uint8 manufac[] = "12345";
	kal_uint8 dev_type[] = "MXT1608";
	kal_uint8 dev_id[] = "MXT1608";
	kal_uint8 license_color = 1;
	kal_char license[] = "AZ1234";

	kal_int32 len_l = -1;
	kal_uint8 *dat_l = out;
	kal_uint32 i = 0, len = 0;

	if (dat_l == NULL) return len_l;

	if (in&&in_len)
	{
		kal_mem_cpy(dat_l, in, in_len);
		len_l = in_len;
	}
	else
	{
		len_l = 0;

		/*province*/
		dat_l[len_l++] = (province >> 8) & 0xFF;
		dat_l[len_l++] = province & 0xFF;

		/*city*/
		dat_l[len_l++] = (city >> 8) & 0xFF;
		dat_l[len_l++] = city & 0xFF;

		/*manufac*/
		snprintf(&dat_l[len_l], 5, "%s\0", manufac);
		len_l += 5;

		/*dev_type*/
		len = strlen(dev_type);
		snprintf(&dat_l[len_l], 20, "%s\0", dev_type);
		for (i = len; i < 20; i++)
			dat_l[len_l + i] = 0; // 后面填0
		len_l += 20;

		/*dev_id*/
		len = strlen(dev_id);
		snprintf(&dat_l[len_l], 7, "%s\0", dev_id);
		for (i = len; i < 7; i++)
			dat_l[len_l + i] = 0; // 后面填0
		len_l += 7;

		/*license_color*/
		dat_l[len_l++] = license_color;

		/*license*/
		len = strlen(license);
		snprintf(&dat_l[len_l], len, "%s\0", license);
		len_l += len;

		dat_l[len_l] = '\0';
	}

	mxapp_trace("%s:len_l(%d=0x%x)", __FUNCTION__, len_l, len_l);
	for (i = 0; i < len_l;)
	{
		mxapp_trace("%s\r\n", &dat_l[i]);
		i += 127;
	}

	return len_l;
}

// 终端注销
static kal_int32 mx_srv_cmd_handle_up_deregister(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	// TODO: 终端注销
}

// 终端鉴权
static kal_int32 mx_srv_cmd_handle_up_authorize(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	kal_int32 len_l = -1;
	kal_uint8 *dat_l = out;
	kal_uint32 i = 0, len = 0;

	if (dat_l == NULL) return len_l;

	if (in&&in_len)
	{
		kal_mem_cpy(dat_l, in, in_len);
		len_l = in_len;
	}
	else
	{
		/*鉴权码*/
		memcpy(dat_l, auth_code, strlen(auth_code));
		len_l = strlen(auth_code);

		dat_l[len_l] = '\0';
	}

	mxapp_trace("%s:len_l(%d=0x%x)", __FUNCTION__, len_l, len_l);
	for (i = 0; i < len_l;)
	{
		mxapp_trace("%s\r\n", &dat_l[i]);
		i += 127;
	}

	return len_l;
}

// 查询终端参数应答
static kal_int32 mx_srv_cmd_handle_up_query_para_ack(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	// TODO: 查询终端参数应答
}

// 查询终端属性应答
static kal_int32 mx_srv_cmd_handle_up_query_prop_ack(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	// TODO: 查询终端属性应答
}

// 位置信息汇报
static kal_int32 mx_srv_cmd_handle_up_loc_report(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	kal_uint32 alarm = 0;
	kal_uint32 status = 0;
	kal_uint32 lat = 0;
	kal_uint32 lon = 0;
	kal_uint16 alt = 50;
	kal_uint16 speed = 10;
	kal_uint16 course = 180;
	
	kal_int32 len_l = -1;
	kal_uint8 *dat_l = out;
	kal_uint32 i = 0, len = 0;

	if (dat_l == NULL) return len_l;

	if (in&&in_len)
	{
		kal_mem_cpy(dat_l, in, in_len);
		len_l = in_len;
	}
	else
	{
		len_l = 0;

		/*alarm*/
		dat_l[len_l++] = (alarm >> 24) & 0xFF;
		dat_l[len_l++] = (alarm >> 16) & 0xFF;
		dat_l[len_l++] = (alarm >> 8) & 0xFF;
		dat_l[len_l++] = alarm & 0xFF;

		/*status*/
		dat_l[len_l++] = (status >> 24) & 0xFF;
		dat_l[len_l++] = (status >> 16) & 0xFF;
		dat_l[len_l++] = (status >> 8) & 0xFF;
		dat_l[len_l++] = status & 0xFF;

		/*lat & lon*/
		ST_MX_LOCATION_INFO *pos_info;
#if 0
		pos_info = mx_location_get_latest_position();
		if ((pos_info->valid) && (pos_info->type == LOCATION_TYPE_GPS))
		{
			lat = mx_base_coordinates2double(&(pos_info->info.gps.lat[0])) * 1000000;
			lon = mx_base_coordinates2double(&(pos_info->info.gps.lon[0])) * 1000000;
		}
#else
		lat = 30000000;
		lon = 114000000;
#endif

		/*lat*/
		dat_l[len_l++] = (lat >> 24) & 0xFF;
		dat_l[len_l++] = (lat >> 16) & 0xFF;
		dat_l[len_l++] = (lat >> 8) & 0xFF;
		dat_l[len_l++] = lat & 0xFF;

		/*lon*/
		dat_l[len_l++] = (lon >> 24) & 0xFF;
		dat_l[len_l++] = (lon >> 16) & 0xFF;
		dat_l[len_l++] = (lon >> 8) & 0xFF;
		dat_l[len_l++] = lon & 0xFF;

		/*alt*/
		dat_l[len_l++] = (alt >> 8) & 0xFF;
		dat_l[len_l++] = alt & 0xFF;

		/*speed*/
		dat_l[len_l++] = (speed >> 8) & 0xFF;
		dat_l[len_l++] = speed & 0xFF;

		/*course*/
		dat_l[len_l++] = (course >> 8) & 0xFF;
		dat_l[len_l++] = course & 0xFF;

		/*time - BCD code*/
		MYTIME stCurrentTime;
#if 0
		DTGetRTCTime(&stCurrentTime);
#else
		stCurrentTime.nYear = 2017;
		stCurrentTime.nMonth = 4;
		stCurrentTime.nDay = 1;
		stCurrentTime.nHour = 17;
		stCurrentTime.nMin = 15;
		stCurrentTime.nSec = 33;
#endif
		stCurrentTime.nYear -= 2000;
		dat_l[len_l++] = ((stCurrentTime.nYear / 10) << 4) + (stCurrentTime.nYear % 10);
		dat_l[len_l++] = ((stCurrentTime.nMonth / 10) << 4) + (stCurrentTime.nMonth % 10);
		dat_l[len_l++] = ((stCurrentTime.nDay / 10) << 4) + (stCurrentTime.nDay % 10);
		dat_l[len_l++] = ((stCurrentTime.nHour / 10) << 4) + (stCurrentTime.nHour % 10);
		dat_l[len_l++] = ((stCurrentTime.nMin / 10) << 4) + (stCurrentTime.nMin % 10);
		dat_l[len_l++] = ((stCurrentTime.nSec / 10) << 4) + (stCurrentTime.nSec % 10);
		//len_l += 6;

		dat_l[len_l] = '\0';
	}

	mxapp_trace("%s:len_l(%d=0x%x)", __FUNCTION__, len_l, len_l);
	for (i = 0; i < len_l;)
	{
		mxapp_trace("%s\r\n", &dat_l[i]);
		i += 127;
	}

	return len_l;
}

// 位置信息查询应答
static kal_int32 mx_srv_cmd_handle_up_loc_query_ack(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	// TODO: 位置信息查询应答
}

// 事件报告
static kal_int32 mx_srv_cmd_handle_up_event_report(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	// TODO: 事件报告
}


// 平台通用应答
// 返回值: 0-成功/确认 1-失败 2-消息有误 3-不支持 4-报警处理确认
static kal_int32 mx_srv_cmd_handle_down_ack(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	kal_uint8 *dat_l = in;
	kal_uint8 tmp;

	kal_uint16 ack_flow = 0; // 应答流水号
	kal_uint16 ack_id = 0; // 应答ID
	kal_int32 ret = -1; // 结果

	if ((dat_l == NULL) || (in_len != 5)) return -1; // 平台通用应答 消息体长度in_len应为5
	mxapp_trace("%s:cmd(%x) in_len(%d)", __FUNCTION__, cmd, in_len);

	/*应答流水号*/
	tmp = *dat_l++;
	ack_flow += tmp << 8;
	tmp = *dat_l++;
	ack_flow += tmp;

	/*应答ID*/
	tmp = *dat_l++;
	ack_id += tmp << 8;
	tmp = *dat_l++;
	ack_id += tmp;

	/*结果: 0-成功/确认 1-失败 2-消息有误 3-不支持 4-报警处理确认*/
	tmp = *dat_l++;
	ret = tmp;
	
	return ret;
}

// 终端注册应答
// 返回值: 0-成功 1-车辆已被注册 2-数据库中无该车辆 3-终端已被注册 4-数据库中无该终端
static kal_int32 mx_srv_cmd_handle_down_register_ack(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	kal_uint8 *dat_l = in;
	kal_uint8 tmp;

	kal_uint16 flow = 0; // 应答流水号
	kal_int32 ret = -1; // 结果
	kal_uint8 *auth = auth_code; // 鉴权码

	if ((dat_l == NULL) || (in_len == 0)) return -1;
	mxapp_trace("%s:cmd(%x) in_len(%d)", __FUNCTION__, cmd, in_len);

	/*应答流水号*/
	tmp = *dat_l++;
	flow += tmp << 8;
	tmp = *dat_l++;
	flow += tmp;

	/*结果: 0-成功 1-车辆已被注册 2-数据库中无该车辆 3-终端已被注册 4-数据库中无该终端*/
	tmp = *dat_l++;
	ret = tmp;

	if (ret == 0) // 成功
	{
		/*鉴权码*/
		auth_code_len = in_len - 3; // 鉴权码实际长度
		kal_mem_cpy(auth, dat_l, auth_code_len);
	}

	return ret;
}

// 设置终端参数
// 返回: 0-成功
static kal_int32 mx_srv_cmd_handle_down_set_para(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	kal_uint8 *dat_l = in;
	kal_uint32 tmp;
	kal_uint8 *buf_tx = g_s8TxBuf;

	kal_uint8 para_num = 0; // 参数总数
	kal_uint8 para_idx = 0;

	kal_uint32 para_id = 0; // 参数ID
	kal_uint8 para_len = 0; // 参数长度
	kal_uint8 *para = NULL; // 参数值

	mx_set_para_handle_struct *func_l = NULL;
	kal_uint32 idx = 0;
	kal_int32 ret = -1;

	para_num = *dat_l++;
	while (para_idx < para_num)
	{
		/*参数ID*/
		para_id = 0;
		tmp = *dat_l++;
		para_id |= (tmp << 24);
		tmp = *dat_l++;
		para_id |= (tmp << 16);
		tmp = *dat_l++;
		para_id |= (tmp << 8);
		tmp = *dat_l++;
		para_id |= tmp;

		/*参数长度*/
		tmp = *dat_l++;
		para_len = tmp;

		/*参数值*/
		para = dat_l;
		dat_l += para_len;

		/*查找参数ID对应设置函数*/
		idx = 0;
		while (set_para_func[idx].para_id != MXAPP_SRV_JTT_SET_PARA_INVALID)
		{
			if (set_para_func[idx].para_id == para_id)
			{
				func_l = &set_para_func[idx];
				break;
			}
			idx++;
		}
		if (!func_l)
		{
			mxapp_trace("--->>mx_srv_cmd_handle_down_set_para(para_id[%04x] undef!!!!)\n", para_id);
		}
		else
		{
			mxapp_trace("--->>mx_srv_cmd_handle_down_set_para([%d]=%04x,%04x)\n", idx, func_l->para_id, para_id);
		}

		/*执行参数ID对应设置函数*/
		if ((func_l) && (func_l->handle))
		{
			ret = func_l->handle(para, para_len);
		}

		if (ret != 0) // 某一次失败则立即退出
		{
			return ret;
		}

		para_idx++; 
	}

	return ret; // 所有设置均成功
}

// 查询终端参数
static kal_int32 mx_srv_cmd_handle_down_query_para(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	// TODO: 查询终端参数
}

// 查询指定终端参数
static kal_int32 mx_srv_cmd_handle_down_query_spec_para(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	// TODO: 查询指定终端参数
}

// 查询终端属性
static kal_int32 mx_srv_cmd_handle_down_query_prop(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	// TODO: 查询终端属性
}

// TODO: 位置信息查询
static kal_int32 mx_srv_cmd_handle_down_loc_query(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	/*消息体为空*/
	kal_int32 ret = 0;

	mx_pos_info_type_set(0x10); // 位置信息查询
	srvinteraction_location_request(0);

	return ret;
}

// 事件设置
static kal_int32 mx_srv_cmd_handle_down_event_set(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	// TODO: 事件设置
}

static mx_cmd_handle_struct cmd_func[] =
{
	{ MXAPP_SRV_JTT_CMD_UP_ACK, mx_srv_cmd_handle_up_ack }, // 终端通用应答
	{ MXAPP_SRV_JTT_CMD_UP_HEARTBEAT, mx_srv_cmd_handle_up_heartbeat }, // 终端心跳
	{ MXAPP_SRV_JTT_CMD_UP_REGISTER, mx_srv_cmd_handle_up_register }, // 终端注册
	{ MXAPP_SRV_JTT_CMD_UP_DEREGISTER, mx_srv_cmd_handle_up_deregister }, // 终端注销
	{ MXAPP_SRV_JTT_CMD_UP_AUTHORIZE, mx_srv_cmd_handle_up_authorize }, // 终端鉴权
	{ MXAPP_SRV_JTT_CMD_UP_QUERY_PARA_ACK, mx_srv_cmd_handle_up_query_para_ack }, // 查询终端参数应答
	{ MXAPP_SRV_JTT_CMD_UP_QUERY_PROP_ACK, mx_srv_cmd_handle_up_query_prop_ack }, // 查询终端属性应答
	{ MXAPP_SRV_JTT_CMD_UP_LOC_REPORT, mx_srv_cmd_handle_up_loc_report }, // 位置信息汇报
	{ MXAPP_SRV_JTT_CMD_UP_LOC_QUERY_ACK, mx_srv_cmd_handle_up_loc_query_ack }, // 位置信息查询应答
	{ MXAPP_SRV_JTT_CMD_UP_EVENT_REPORT, mx_srv_cmd_handle_up_event_report }, // 事件报告

	{ MXAPP_SRV_JTT_CMD_DOWN_ACK, mx_srv_cmd_handle_down_ack }, // 平台通用应答
	{ MXAPP_SRV_JTT_CMD_DOWN_REGISTER_ACK, mx_srv_cmd_handle_down_register_ack }, // 终端注册应答
	{ MXAPP_SRV_JTT_CMD_DOWN_SET_PARA, mx_srv_cmd_handle_down_set_para }, // 设置终端参数
	{ MXAPP_SRV_JTT_CMD_DOWN_QUERY_PARA, mx_srv_cmd_handle_down_query_para }, // 查询终端参数
	{ MXAPP_SRV_JTT_CMD_DOWN_QUERY_SPEC_PARA, mx_srv_cmd_handle_down_query_spec_para }, // 查询指定终端参数
	{ MXAPP_SRV_JTT_CMD_DOWN_QUERY_PROP, mx_srv_cmd_handle_down_query_prop }, // 查询终端属性
	{ MXAPP_SRV_JTT_CMD_DOWN_LOC_QUERY, mx_srv_cmd_handle_down_loc_query }, // 位置信息查询
	{ MXAPP_SRV_JTT_CMD_DOWN_EVENT_SET, mx_srv_cmd_handle_down_event_set }, // 事件设置
#ifdef __WHMX_CALL_SUPPORT__
	{ MXAPP_SRV_JTT_CMD_DOWN_CALL_MONITOR, mx_srv_cmd_handle_down_CALL_MONITOR }, // 电话回拨
	{ MXAPP_SRV_JTT_CMD_DOWN_PHONEBOOK, mx_srv_cmd_handle_down_PHONEBOOK }, // 设置电话本
#endif
#ifdef __WHMX_JTT_FENCE_SUPPORT__
#endif
	{ MXAPP_SRV_JTT_CMD_INVALID, NULL }
};





// 转义处理：返回处理后长度
static kal_int32 mx_srv_data_escape(kal_uint8 *dat, kal_uint32 in_len)
{
	kal_uint32 idx, buf_idx;
	kal_uint8 buf[MXAPP_ONENET_BUFF_MAX];
	kal_uint8 temp;

	buf_idx = 0;
	for (idx = 0; idx < in_len; idx++)
	{
		temp = dat[idx];
		if (temp == 0x7e)
		{
			buf[buf_idx++] = 0x7d;
			buf[buf_idx++] = 0x02;
		}
		else if (temp == 0x7d)
		{
			buf[buf_idx++] = 0x7d;
			buf[buf_idx++] = 0x01;
		}
		else
		{
			buf[buf_idx++] = temp;
		}
	}

	kal_mem_cpy(dat, buf, buf_idx);
	return buf_idx;
}

// 发送消息 数据处理
// 格式: 标识位+消息头+消息体+校验码+标识位
// 流程: 消息封装——>计算填充校验码——>转义
static kal_int32 mx_srv_data_package(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *in, kal_uint32 in_len, kal_uint8 *out)
{
	kal_int32 ret = 0;
	kal_uint32 idx = 0;
	kal_uint8 *buf_l = out;
	kal_uint16 cmd_l = cmd;
	kal_uint8 cs = 0; // 校验码
	kal_uint16 buf_len = 0; // 消息头+消息体+校验码长度

	if ((in == NULL) || (out == NULL)) return -1;

	/*标识位*/
	buf_l[0] = FRAME_FLAG_SYMBOL;

	/*消息头-START*/
	/*消息ID*/
	buf_l[1] = (cmd_l >> 8);
	buf_l[2] = cmd_l & 0xFF;

	/*消息体属性（长度）*/
	buf_l[3] = (in_len >> 8);
	buf_l[4] = in_len & 0xFF;

	/*终端手机号*/
	for (idx = 0; idx < 6; idx++)
	{
		buf_l[5 + idx] = cell_number[idx]; // 高位在前
	}
	
	/*消息流水号（从0开始循环累加）*/
	buf_l[11] = (serial_no_up >> 8);
	buf_l[12] = serial_no_up & 0xFF;
	serial_no_up++;

	/*消息包封装项-无*/
	/*消息头-END*/

	/*消息体*/
	if (in_len != 0)
	{
		kal_mem_cpy(&buf_l[13], in, in_len);
	}

	/*校验码*/
	cs = buf_l[1];
	for (idx = 2; idx < (13 + in_len); idx++)
	{
		cs ^= buf_l[idx];
	}
	buf_l[13 + in_len] = cs;

	/*转义*/
	buf_len = 13 + in_len;
	buf_len = mx_srv_data_escape(&buf_l[1], buf_len);

	/*标识位*/
	buf_l[buf_len + 1] = FRAME_FLAG_SYMBOL;

	ret = buf_len + 2; // 消息总长度

	mxapp_trace("--->>mx_srv_data_package(cmd=%04x,msg_len=%d,len=%d,cs=0x%x,next_flow_no=%d)\n", cmd_l, buf_len, ret, cs, serial_no_up);
	return ret;
}

// 终端发送消息至平台
static kal_int32 mx_srv_send_handle(SRV_ZXBD_CMD_TYPE cmd, kal_uint8 *para, kal_uint32 para_len)
{
	static kal_uint32 send_len_bak = 0;
	mx_cmd_handle_struct *func_l = NULL;
	kal_uint8 *buf_tmp = g_s8TmpBuf;
	kal_uint8 *buf_tx = g_s8TxBuf;
	kal_uint32 idx = 0;
	kal_int32 ret = 0;

	if (shutdown_is_active) return -1;

	// get cmd-function pointer
	while (cmd_func[idx].cmd != MXAPP_SRV_JTT_CMD_INVALID)
	{
		if (cmd_func[idx].cmd == cmd)
		{
			func_l = &cmd_func[idx];
			break;
		}
		idx++;
	}

	// no corresponding cmd-function
	if (!func_l)
	{
		mxapp_trace("--->>mx_srv_send_handle(cmd[%04x] undef!!!!)\n", cmd);

		return -1;
	}

	mxapp_trace("--->>mx_srv_send_handle([%d]=%04x,%04x)\n", idx, func_l->cmd, cmd);

	//	if(KAL_TRUE == mx_srv_call_connected()) return -1;

	if (func_l->handle)
	{
		kal_mem_set(buf_tmp, 0, sizeof(g_s8TmpBuf));

		// execute cmd-function handler
		ret = func_l->handle(cmd, para, para_len, buf_tmp);

		if (ret > 0)
		{
			kal_mem_set(buf_tx, 0, sizeof(g_s8TxBuf));
			ret = mx_srv_data_package(cmd, buf_tmp, ret, buf_tx); // formatting
			send_len_bak = ret;
			if (/*(g_s32FirstConnect == 0)&&*/(ret > 0))
			{
#if !defined(__WHMX_SERVER_NO_SMS__)
#if defined(__WHMX_CALL_SUPPORT__)
				if (mxapp_call_is_admin_set())
#elif defined(__WHMX_ADMIN_SUPPORT__)
				if (mxapp_admin_is_set())
#endif
#endif	
				{ // upload to server
					mxapp_srv_send(buf_tx, ret, mx_srv_send_handle_callback);
				}
				mx_srv_send_handle_callback = NULL;
			}
#if defined(__WHMX_LOG_SRV_SUPPORT__)
			{
				kal_char *head = NULL;
				kal_char *pbuf = NULL;
				kal_uint32 idx = 0;

				head = mx_log_srv_write_buf();
				if (head)
				{
					pbuf = head;
					kal_sprintf(pbuf, "mx_srv_send_handle[%d][%04x]:", ret, cmd);
					pbuf += strlen(pbuf);
					kal_sprintf(pbuf, "%c%c%c%c ", buf_tx[0], buf_tx[1], buf_tx[2], buf_tx[3]);
					pbuf += 5;
					kal_sprintf(pbuf, "%d ", (buf_tx[4] << 8) | buf_tx[5]);
					pbuf += strlen(pbuf);
					for (idx = 6; idx < 18; idx++)
					{
						kal_sprintf(pbuf, "%02x", buf_tx[idx]);
						pbuf += 2;
					}
					*pbuf = ' ';
					pbuf += 1;
					kal_sprintf(pbuf, "<%04x> ", (buf_tx[18] << 8) | buf_tx[19]);
					pbuf += 7;

					kal_sprintf(pbuf, "<");
					pbuf += 1;
					if ((cmd == MXAPP_SRV_ZXBD_CMD_UP_POSITION))
					{
						kal_sprintf(pbuf, "%02d-%02d-%02d ", buf_tx[20], buf_tx[21], buf_tx[22]);
						pbuf += 9;
						kal_sprintf(pbuf, "%02d%02d%02d ", buf_tx[23], buf_tx[24], buf_tx[25]);
						pbuf += strlen(pbuf);
						kal_sprintf(pbuf, "%d.%02d%02d%02d ", buf_tx[26], buf_tx[27], buf_tx[28], buf_tx[29]);
						pbuf += strlen(pbuf);
						kal_sprintf(pbuf, "%d.%02d%02d%02d ", buf_tx[30], buf_tx[31], buf_tx[32], buf_tx[33]);
						pbuf += strlen(pbuf);
						kal_sprintf(pbuf, "%d ", buf_tx[34]);
						pbuf += strlen(pbuf);
						kal_sprintf(pbuf, "%d ", buf_tx[35]);
						pbuf += strlen(pbuf);
						kal_sprintf(pbuf, "%d ", (buf_tx[36] << 8) | (buf_tx[37]));/*高度*/
						pbuf += strlen(pbuf);
						kal_sprintf(pbuf, "0x%02x ", buf_tx[38]);/*定位状态*/
						pbuf += 5;
						kal_sprintf(pbuf, "0x%02x ", buf_tx[39]);/*报警状态*/
						pbuf += 5;
						kal_sprintf(pbuf, "%d ", buf_tx[40]);/*电量*/
						pbuf += strlen(pbuf);
						kal_sprintf(pbuf, "%c ", buf_tx[41]);/*经度标志*/
						pbuf += 2;
						kal_sprintf(pbuf, "%c ", buf_tx[42]);/*纬度标志*/
						pbuf += 2;
						kal_sprintf(pbuf, "0x%02x ", buf_tx[43]);/*REV*/
						pbuf += 5;
						kal_sprintf(pbuf, "%d:", (buf_tx[44] << 8) | (buf_tx[45]));/*国别*/
						pbuf += strlen(pbuf);
						kal_sprintf(pbuf, "%02d:", (buf_tx[46]));/*运营商*/
						pbuf += 3;
						kal_sprintf(pbuf, "%d:", (buf_tx[47] << 8) | (buf_tx[48]));/*小区编号*/
						pbuf += strlen(pbuf);
						kal_sprintf(pbuf, "%d ", (buf_tx[49] << 8) | (buf_tx[50]));/*基站扇区*/
						pbuf += strlen(pbuf);
						kal_sprintf(pbuf, "0x%02x ", buf_tx[51]);/*REV*/
						pbuf += 5;
						kal_sprintf(pbuf, "-%d ", (buf_tx[52]));/*信号量*/
						pbuf += strlen(pbuf);
						kal_sprintf(pbuf, "%02d-%02d-%02d ", buf_tx[53], buf_tx[54], buf_tx[55]);/*基站时间1*/
						pbuf += 9;
						kal_sprintf(pbuf, "%02d:%02d:%02d ", buf_tx[56], buf_tx[57], buf_tx[58]);/*基站时间2*/
						pbuf += 9;
						kal_sprintf(pbuf, "0x%02x 0x%02x ", buf_tx[72], buf_tx[73]);/*步数*/
						pbuf += 10;
						for (idx = 74; idx < (send_len_bak - 3); idx++)/*WiFi数据*/
						{
							if (buf_tx[idx] == 0) break;

							if (buf_tx[idx] != 0xFF)
							{
								*pbuf++ = buf_tx[idx];
							}
							else
							{
								*pbuf++ = ' ';
							}
						}
					}
					else
					{
						for (idx = 20; idx < (send_len_bak - 4); idx++)
						{
							kal_sprintf(pbuf, "%02x ", buf_tx[idx]);
							pbuf += 3;
						}
						kal_sprintf(pbuf, "%02x", buf_tx[idx]);
						pbuf += 2;
					}
					kal_sprintf(pbuf, "> ");
					pbuf += 2;

					kal_sprintf(pbuf, "%02x %02x ", buf_tx[send_len_bak - 3], buf_tx[send_len_bak - 2]);
					pbuf += 6;
					kal_sprintf(pbuf, "0x%02x\0", buf_tx[send_len_bak - 1]);
					pbuf += 4;

					mx_log_srv_write(head, strlen(head), KAL_TRUE);
					mxapp_trace("%s", head);
				}
			}
#endif
		}
	}

	return ret;
}


// 逆转义处理：返回处理后长度
static kal_int32 mx_srv_data_de_escape(kal_uint8 *dat, kal_uint32 in_len)
{
	kal_uint32 idx, buf_idx;
	kal_uint8 buf[MXAPP_ONENET_BUFF_MAX];
	kal_uint8 temp1, temp2;

	buf_idx = 0;
	for (idx = 0; idx < in_len;)
	{
		temp1 = dat[idx];
		temp2 = 0;
		if (idx < in_len - 1)
		{
			temp2 = dat[idx + 1];
		}
		
		if (temp1 == 0x7d && temp2 == 0x02)
		{
			buf[buf_idx++] = 0x7e;
			idx += 2;
		}
		else if (temp1 == 0x7d && temp2 == 0x01)
		{
			buf[buf_idx++] = 0x7d;
			idx += 2;
		}
		else
		{
			buf[buf_idx++] = temp1;
			idx += 1;
		}
	}

	kal_mem_cpy(dat, buf, buf_idx);
	return buf_idx;
}


// 接收消息 数据处理
// 格式: 标识位+消息头+消息体+校验码+标识位
// 流程: 逆转义——>验证校验码——>解析消息
// 返回消息体长度
static kal_int32 mx_srv_data_parse(kal_uint8 src, kal_uint8 *in, kal_uint32 in_len, kal_uint32 *cmd, kal_uint16 *flow, kal_uint8 *msg)
{
	kal_uint8 *buf = in;
	kal_int32 buf_len = -1; // 逆转义后长度
	kal_uint8 cs = 0; // 校验码
	kal_uint32 idx = 0;
	kal_uint16 msg_len = -1; // 消息体长度

	if (src != 0) return -1; // src为备用参数，默认为0
	if (!cmd || !flow || !msg) return -1; // 检查参数

	/*检查标识位*/
	if ((buf[0] != 0x7e) || (buf[in_len-1] != 0x7e)) return -1;

	/*逆转义*/
	buf_len = mx_srv_data_de_escape(&buf[1], in_len - 2); // 除去标识位的长度
	buf_len += 2; // 逆转义后总长度

	/*验证校验码*/
	cs = buf[1];
	for (idx = 2; idx < buf_len - 2; idx++)
	{
		cs ^= buf[idx];
	}
	if (buf[buf_len - 2] != cs) return -1;

	/*消息ID*/
	*cmd = (buf[1] << 8) | buf[2];

	/*消息体长度*/
	msg_len = (buf[3] << 8) | buf[4];

	/*消息流水号*/
	*flow = (buf[11] << 8) | buf[12];

	if ((msg_len & 0x20) == 0) // 不分包
	{
		kal_mem_cpy(msg, &buf[13], msg_len); // 消息体内容
	}
	else // 分包
	{
	}

	return msg_len;
}



// 终端接收平台消息
kal_int32 mx_srv_receive_handle_jtt(kal_uint8 src, kal_uint8 *in, kal_int32 in_len)
{	
	kal_int32 ret = 0;
	kal_uint32 cmd = 0; // 消息ID
	kal_uint16 flow = 0; // 消息流水号
	kal_uint8 *msg = g_s8TmpBuf; // 消息体内容
	mx_cmd_handle_struct *func_l = NULL;
	kal_uint32 idx = 0;

	if ((in == NULL) || (in_len < 1)) return -1;

	kal_mem_set(msg, 0, sizeof(g_s8TmpBuf));
	ret = mx_srv_data_parse(src, in, in_len, &cmd, &flow, msg);

	/*查找CMD对应处理函数*/
	while (cmd_func[idx].cmd != MXAPP_SRV_JTT_CMD_INVALID)
	{
		if (cmd_func[idx].cmd == cmd)
		{
			func_l = &cmd_func[idx];
			break;
		}
		idx++;
	}
	if (!func_l)
	{
		mxapp_trace("--->>mx_srv_receive_handle(cmd[%04x] undef!!!!)\n", cmd);
	}
	else
	{
		mxapp_trace("--->>mx_srv_receive_handle([%d]=%04x,%04x)\n", idx, func_l->cmd, cmd);
	}

	/*执行CMD对应处理函数*/
	if ((func_l) && (func_l->handle))
	{
		ret = func_l->handle(cmd, msg, ret, NULL);
	}

	/*TODO: 按需进行应答*/
	if (cmd == MXAPP_SRV_JTT_CMD_DOWN_SET_PARA) // 终端通用应答
	{
		kal_uint8 cmd_ack[5] = { 0 };

		cmd_ack[0] = flow >> 8;
		cmd_ack[1] = flow & 0xFF;
		cmd_ack[2] = cmd >> 8;
		cmd_ack[3] = cmd & 0xFF;
		cmd_ack[4] = ret; // 执行结果

		ret = mx_srv_send_handle(MXAPP_SRV_JTT_CMD_UP_ACK, cmd_ack, 5);
	}
	else if (cmd == MXAPP_SRV_JTT_CMD_DOWN_LOC_QUERY)
	{

	}

	return ret;
}

kal_int32 mx_srv_register_jtt(void)
{
	mx_srv_send_handle(MXAPP_SRV_JTT_CMD_UP_REGISTER, NULL, 0);
}

kal_int32 mx_srv_heartbeat_jtt(void)
{
	mx_srv_send_handle(MXAPP_SRV_JTT_CMD_UP_HEARTBEAT, NULL, 0);
}




kal_int32 mxapp_srvinteraction_upload_location_info(ST_MX_LOCATION_INFO *pLocateInfo)
{
	kal_int32 s32Ret = 0;

	if(pLocateInfo == NULL)
	{
		pLocateInfo = mx_location_get_latest_position();
	}

	if(pLocateInfo->valid)
	{
		mx_srv_send_handle(MXAPP_SRV_JTT_CMD_UP_LOC_REPORT, NULL, 0);
	}

#if MX_LBS_UPLOAD_FILTER
	if(pLocateInfo->valid)
	{
		if(pLocateInfo->type == LOCATION_TYPE_LBS)
		{
			mx_location_lbs_check_repeat(1, &(pLocateInfo->info.lbs)); // set
		}
		else
		{
			mx_location_lbs_check_repeat(1, NULL); // clear
		}
	}
#endif

	return s32Ret;
}

static kal_int8 srvinteraction_location_cb(ST_MX_LOCATION_INFO *pParams)
{
#if 0 // 原OneNet代码，可保留，此处为调试
	kal_int8 s8Ret = 0;

	mxapp_srvinteraction_upload_location_info(pParams);

	if (g_s32PowerOff==1)
	{
		g_s32PowerOff = 2;/*!=0,,防止重新发起低电关机定位请求*/

		mxapp_trace("srvinteraction poweroff");

#if defined(__WHMX_MXT1608S__)
		// buzzer 1s
		mxapp_buzzer_ctrl(MX_BUZZER_ONCE);
		// start led with buzzer
		mxapp_led_ctrl(MX_LED_POWER_OFF, MX_PLAY_ONCE);
#endif

		StopTimer(MXAPP_TIMER_NET_UPLOAD);
		StartTimer(MXAPP_TIMER_NET_UPLOAD, (10 * 1000), mxapp_srvinteraction_poweroff);
	}

	return s8Ret;
#else
	mxapp_srvinteraction_upload_location_info(pParams);
#endif
}


static void srvinteraction_location_request(kal_int8 type)
{
#if 0 // 原OneNet代码，可保留，此处为调试
	kal_prompt_trace(MOD_MMI, "srvinteraction_location_request (type=%d) --------------> \r\n", type);

	switch (type)
	{
	case 0:	// GNSS + LBS
		mx_location_request(srvinteraction_location_cb);
		break;
	case 1: // LBS only
		//		mx_location_request_LBS_only(srvinteraction_location_cb);
		break;
	case 2:
		//		mx_location_request(srvinteraction_bootup_location_cb);
		break;
	case 3: // sos-1
		mx_location_request_NO_GNSS(srvinteraction_location_cb);
		break;
	case 4: // sos-2
		mx_location_request_ignore_movement(srvinteraction_location_cb);
		break;
	}
#else
	srvinteraction_location_cb(NULL);
#endif
}


void main(void)
{
	kal_uint8 ret;

	mx_srv_register_jtt();

	kal_uint8 buf_in2[] = { 0x7E, 0x81, 0x03, 0x00, 0x1A, 0x01, 0x20, 0x00, 0x18, 0x71, 0x48, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x01, 0x04, 0x00, 0x00, 0x00, 0x3C, 0x00, 0x00, 0x00, 0x48, 0x0B, 0x31, 0x33, 0x38, 0x30, 0x38, 0x36, 0x32, 0x38, 0x38, 0x36, 0x33, 0xD2, 0x7E };
	ret = mx_srv_receive_handle_jtt(0, buf_in2, sizeof(buf_in2));

	kal_uint8 buf_in[] = { 0x7E, 0x80, 0x01, 0x00, 0x05, 0x02, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x25, 0x01, 0x00, 0x01, 0xB6, 0x7E }; // 0x8001 平台通用应答
	ret = mx_srv_receive_handle_jtt(0, buf_in, sizeof(buf_in));

	kal_uint8 buf_in1[] = { 0x7E, 0x81, 0x00, 0x00, 0x09, 0x02, 0x00, 0x00, 0x00, 0x00, 0x15, 0x00, 0x00, 0x00, 0x25, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x15, 0xAD, 0x7E }; // 0x8100 终端注册应答
	ret = mx_srv_receive_handle_jtt(0, buf_in1, sizeof(buf_in1));



	kal_uint8 tmp0[] = { 0x30,0x7e,0x08,0x7d,0x55 };
	ret = mx_srv_data_escape(tmp0, sizeof(tmp0));
	ret = mx_srv_data_de_escape(tmp0, ret);

	mx_srv_send_handle(MXAPP_SRV_JTT_CMD_UP_REGISTER, NULL, 0);

	memset(g_s8TxBuf, 1, MXAPP_ONENET_BUFF_MAX);
	mx_srv_cmd_handle_up_register(0, NULL, 0, g_s8TxBuf);

	kal_uint8 tmp[] = { 0x00, 0x25, 0x00, 0x02, 0x31, 0x32, 0x30, 0x30, 0x30, 0x31, 0x38, 0x37, 0x31, 0x34, 0x38 };
	memcpy(g_s8RxBuf, tmp, sizeof(tmp));
	mx_srv_cmd_handle_down_register_ack(0, g_s8RxBuf, sizeof(tmp), NULL);

	mx_srv_cmd_handle_up_authorize(0, NULL, 0, g_s8TxBuf);

	mx_srv_cmd_handle_up_loc_report(0, NULL, 0, g_s8TxBuf);

	kal_uint8 tmp1[] = { 0x00, 0x02, 0x02, 0x00, 0x04 };
	memcpy(g_s8RxBuf, tmp1, sizeof(tmp));
	mx_srv_cmd_handle_down_ack(0, g_s8RxBuf, sizeof(tmp), NULL);
}