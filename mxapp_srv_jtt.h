
#if 1
#ifndef __MXAPP_SRV_JTT_H__
#define __MXAPP_SRV_JTT_H__

#include "others.h"

#define SRV_USE_NVRAM 1

#if (SRV_USE_NVRAM == 0)
#define JTT808_SERVER_1 1 // 平台1:天眼视讯正式服务器
#define JTT808_SERVER_1_DEBUG 2 // 平台1:天眼视讯调试服务器
#define JTT808_SERVER_2 3 // 平台2
#define JTT808_SERVER_3 4 // 平台2

#define MX_JTT808_SRV JTT808_SERVER_3

#if (MX_JTT808_SRV == JTT808_SERVER_1)
#define	MXAPP_SRV_ADDR_IP		"221.204.237.94"
#define	MXAPP_SRV_ADDR_PORT		9988

#elif (MX_JTT808_SRV == JTT808_SERVER_1_DEBUG)
#define	MXAPP_SRV_ADDR_IP		"112.74.87.95"
#define	MXAPP_SRV_ADDR_PORT		6968

#elif (MX_JTT808_SRV == JTT808_SERVER_2)
#define	MXAPP_SRV_ADDR_IP		"139.196.164.147"
#define	MXAPP_SRV_ADDR_PORT		9090

#elif (MX_JTT808_SRV == JTT808_SERVER_3)
#define	MXAPP_SRV_ADDR_IP		"122.114.160.11"
#define	MXAPP_SRV_ADDR_PORT		7611
#endif
#endif

#define MXAPP_RX_BUFSIZE        1024

typedef void(*mx_srv_cb)(void *);

void mxapp_srv_connect(void);
kal_int32 mxapp_srv_send(kal_uint8 *dat_in, kal_uint16 in_len, mx_srv_cb cb);
kal_int8 mxapp_srv_heart_set(kal_uint16 heart_s);
kal_uint32 mxapp_srv_heart_get(void);
void mxapp_srv_address_set(kal_char * ip, kal_uint16 port);
void mxapp_srv_address_get(kal_char * ip, kal_uint16 * port);

#endif
#endif
