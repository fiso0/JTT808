#ifndef __SRVINTERACTION_H__
#define __SRVINTERACTION_H__

#include "others.h"

#define MX_SRV_DEBUG 1

#if MX_SRV_DEBUG
#define	MXAPP_SRV_ADDR_IP		"112.74.87.95"
#define	MXAPP_SRV_ADDR_PORT		6968
#else
#define	MXAPP_SRV_ADDR_IP		"221.204.237.94"
#define	MXAPP_SRV_ADDR_PORT		9988
#endif

#define MXAPP_RX_BUFSIZE        1024

typedef void(*mx_srv_cb)(void *);

kal_int32 mxapp_srv_send(kal_uint8 *dat_in, kal_uint16 in_len, mx_srv_cb cb);

#endif