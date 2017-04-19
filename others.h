#ifndef __OTHERS_H__
#define __OTHERS_H__

#include <string.h>
#include <stdio.h>
#include "VS_kal_general_types.h"

#define __WHMX_MXT1608S__
#define __WHMX_SOS_2ROUND__

#define DEBUG_IN_VS 1

#define kal_mem_cpy(...) memcpy(##__VA_ARGS__)
#define kal_mem_set(...) memset(##__VA_ARGS__)

#define kal_sprintf(...) sprintf(##__VA_ARGS__)
#define snprintf _snprintf

#define mxapp_trace(...) printf(##__VA_ARGS__);printf("\n")

#define mx_location_get_latest_position(...) NULL
#define DTGetRTCTime(...)
#define MX_BASE_PRINT(...)

#define StopTimer(...)
#define StartTimer(...)
#define mx_net_get_apn(...) 0
#define mx_tcp_connect(...) 1
#define mx_tcp_read(...) 1
#define mx_tcp_close(...)
#define mx_tcp_write(...) 1
#define mmi_flight_mode_switch_for_mx(...)
#define mx_srv_call_connected(...) 0
#define mx_location_check_and_start_agps(...)


#define mx_location_period_change(...)

#define MX_APP_CALL_NUM_MAX_LEN     15


#define MXAPP_TIMER_RECONNECT_SERVICE 1
#define MXAPP_TIMER_INTERACTION_SERVICE 2
#define MXAPP_TIMER_CMD_TIMEOUT_SERVICE 3
#define MXAPP_TIMER_DISCONNECT_SERVICE 4


#define DEVICE_NUM_LEN 6 // 6位BCD码 JTT808协议规定终端手机号最多12位 前面补0
#define AUTH_CODE_LEN 20 // 鉴权码长度 要保证最后为0
typedef struct
{
	kal_uint32 heart; // unit: s
	kal_char srv_ip[16];
	kal_uint16 srv_port;
	kal_uint8 auth_code_len;
	kal_uint8 auth_code[AUTH_CODE_LEN];
	kal_uint8 cell_num[DEVICE_NUM_LEN];
} nvram_ef_mxapp_jtt_config_t;


/* connected status */
#define MX_TCP_EVT_CONNECTED	1

/* write status */
#define MX_TCP_EVT_CAN_WRITE	2

/* read status */
#define MX_TCP_EVT_CAN_READ		3

/* broken status */
#define MX_TCP_EVT_PIPE_BROKEN	4

/* not find host */
#define MX_TCP_EVT_HOST_NOT_FOUND	 5

/* tcp pipe closed */
#define MX_TCP_EVT_PIPE_CLOSED	     6


typedef struct
{
	kal_uint8 dev_mode; // 0:low;1:normal;2:continue (3:auto in 1608S)
#if defined(__WHMX_MXT1608S__)
	kal_uint8 custom_period; // minute
	kal_uint8 auto_Tmin; // second
	kal_uint8 auto_Tgap; // second
#endif
} nvram_ef_mxapp_pos_mode_t;


typedef enum
{
	LOCATION_TYPE_GPS,
	LOCATION_TYPE_LBS
}LOCATION_TYPE;

typedef struct MX_BASE_TIME
{
	kal_uint8 hour;
	kal_uint8 minute;
	kal_uint8 second;
	kal_uint8 misecond;
}ST_MX_BASE_TIME;

typedef struct MX_BASE_DATE
{
	kal_uint8 year;
	kal_uint8 month;
	kal_uint8 day;
}ST_MX_BASE_DATE;

typedef struct GPS_DRV_INFO
{
	ST_MX_BASE_TIME time;
	//	kal_uint8 hour;
	//	kal_uint8 minute;
	//	kal_uint8 second;
	//	kal_uint8 misecond;
	kal_uint8 status;
	kal_uint8 lat[5];
	kal_uint8 N_S;
	kal_uint8 lon[5];
	kal_uint8 E_W;
	kal_uint16 speed;
	kal_uint16 course;
	ST_MX_BASE_DATE date;
	//	kal_uint8 year;
	//	kal_uint8 month;
	//	kal_uint8 day;
	kal_uint8 postMode;
	kal_uint8 numSV;
	kal_uint8 quality;

	kal_uint8 gnss_type;
	kal_uint8 speed_grade;
	kal_uint16 speed_AVG;

	kal_uint8 CN0_AVG;
	kal_uint8 CN0_max;
	kal_uint8 CN0_cnt;
}ST_GPS_DRV_INFO;

typedef struct
{
	kal_uint16 arfcn;
	kal_uint8  bsic;
	kal_uint8  rxlev;
	kal_uint16 mcc;
	kal_uint16 mnc;
	kal_uint16 lac;
	kal_uint16 ci;
} mx_nbr_info_t;

typedef struct
{
	kal_bool is_info_valid;
	kal_uint8 ta;
	mx_nbr_info_t cur_cell_info;
	kal_uint8 nbr_cell_num;
	mx_nbr_info_t nbr_cell_info[6];
} mx_cell_info_t;

typedef struct MX_LOCATION_INFO
{
	kal_bool			valid;
	LOCATION_TYPE 	type;
	union
	{
		ST_GPS_DRV_INFO gps;
		mx_cell_info_t  	 lbs;
	}info;
}ST_MX_LOCATION_INFO;

typedef struct MYTIME
{
	kal_int16 nYear;
	kal_int8 nMonth;
	kal_int8 nDay;
	kal_int8 nHour;
	kal_int8 nMin;
	kal_int8 nSec;
	kal_int8 DayIndex; /* 0=Sunday */
} MYTIME;

double mx_base_coordinates2double(kal_uint8 *coordinates);

typedef struct
{
	kal_uint8 unused;
} *KAL_ADM_ID;

#endif