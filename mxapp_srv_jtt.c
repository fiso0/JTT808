#include "mxapp_srv_jtt.h"
#include "mxapp_srvinteraction_jtt.h"

#define MXAPP_SRV_BUFF_MAX_RX 256
#define MXAPP_SRV_BUFF_MAX_TX 448

#define MXAPP_SRV_DEBUG_TXRX 1
#if MXAPP_SRV_DEBUG_TXRX
static kal_uint8 edp_dbg_buf[MXAPP_SRV_BUFF_MAX_TX * 3];
#endif

#define HTTP_DEBUG 1
#if HTTP_DEBUG
#define http_trace(...)  mxapp_trace(##__VA_ARGS__)
#else
#define http_trace(...)
#endif

#define FUNC_ENTER http_trace("%s enter.\r\n",__FUNCTION__)
#define FUNC_LEAVE http_trace("%s leave.\r\n",__FUNCTION__)

#define MXAPP_SRV_RECON_TIMEOUT	(2000)	/*unit:ms*/
#define MXAPP_SRV_CONN_TIMEOUT	(90000)	/*unit:ms*/
#define MODE_FLIGHT_TO_NORMAL_INTERVAL	(30000)	/*unit:ms*/
#define MXAPP_SRV_RECON_COUNT_MAX	5	//10

static kal_uint8 OnenetEdpRxBuf[MXAPP_SRV_BUFF_MAX_RX] = { 0 };
static kal_uint8 OnenetEdpTxBuf[MXAPP_SRV_BUFF_MAX_TX] = { 0 };

static kal_int8 mxapp_srv_hdl = -1;
static kal_uint8 mxapp_srv_recon_cnt = 0;
static kal_uint8 mxapp_srv_con_state = 0;//0,1,2
static kal_uint8 mxapp_srv_con_login = 0;//0,1,2
static kal_uint8 mxapp_srv_send_err = 0;
static kal_uint8 mxapp_srv_con_req_cnt = 0;


static void mxapp_srv_reconnect(void);
static void mxapp_srv_disconnect(void);
static void mxapp_srv_heart(void);


static void mxapp_srv_recv_handle(kal_uint8 *dat_in, kal_uint16 in_len)
{
	kal_int32 ret = -1;
	kal_uint8 cmd_t = 0;

	FUNC_ENTER;

#if MXAPP_SRV_DEBUG_TXRX
	{
		kal_uint8 *log_buf = edp_dbg_buf, *log_buf_h = edp_dbg_buf;
		kal_uint32 i;

		kal_mem_set(log_buf, 0, sizeof(edp_dbg_buf));
		for (i = 0; i < ((in_len < MXAPP_SRV_BUFF_MAX_TX) ? in_len : MXAPP_SRV_BUFF_MAX_TX); i++)
		{
			kal_sprintf(log_buf, "%x ", dat_in[i]);
			log_buf += strlen(log_buf);
		}
		for (i = 0; i < strlen(log_buf_h);)
		{
			mxapp_trace("%s\r\n", (log_buf_h + i));
			i += 127;
		}
	}
#endif

	if (dat_in)
	{
		mx_srv_receive_handle_jtt(0, dat_in, in_len);
	}

	mxapp_srv_heart();

	mxapp_trace("%s leave (%d)(%x)(%x)(%d)\r\n", __FUNCTION__, ret, cmd_t, dat_in ? (*dat_in) : 0, in_len);
}

static void mxapp_srv_ind(kal_int8 hdl, kal_uint32 evt)
{
	kal_int32 s32Ret = 0;
	kal_int32 len_real = 0;
#if defined(__WHMX_LOG_SRV_SUPPORT__)
	kal_char *head = NULL;
	kal_char *pbuf = NULL;
#endif

	mxapp_trace("%s (%d)(%d)\r\n", __FUNCTION__, hdl, evt);

#if defined(__WHMX_LOG_SRV_SUPPORT__)
	{
		kal_char *ind_info[7] = { "unknow", "connected", "write", "read", "broken", "host not found", "pipe closed" };
		kal_uint32 idx = 0;

		head = mx_log_srv_write_buf();
		if (head)
		{
			pbuf = head;
			kal_sprintf(pbuf, "%s handle = (%d,%d), event = %d.%s", __FUNCTION__, mxapp_srv_hdl, hdl, evt, ind_info[(evt > 6) ? 0 : evt]);
			//			mx_log_srv_write(head,strlen(head),KAL_TRUE);
		}
	}
#endif

	if (hdl != mxapp_srv_hdl)
	{
#if defined(__WHMX_LOG_SRV_SUPPORT__)
		if (head)
		{
			mx_log_srv_write(head, strlen(head), KAL_TRUE);
			mxapp_trace("%s", head);
		}
#endif
		return;
	}
#if defined(__WHMX_LOG_SRV_SUPPORT__)
	else
	{
		if (evt != MX_TCP_EVT_CAN_READ)
		{
			if (head)
			{
				mx_log_srv_write(head, strlen(head), KAL_TRUE);
				mxapp_trace("%s", head);
			}
		}
	}
#endif

	switch (evt)
	{
	case MX_TCP_EVT_CONNECTED:
		StopTimer(MXAPP_TIMER_CMD_TIMEOUT_SERVICE);
		mxapp_srv_recon_cnt = 0;
		mxapp_srv_con_state = 2;
		mx_srv_register_jtt();
		break;

	case MX_TCP_EVT_CAN_WRITE:
		break;

	case MX_TCP_EVT_CAN_READ:
		kal_mem_set(OnenetEdpRxBuf, 0, sizeof(OnenetEdpRxBuf));
		do
		{
			s32Ret = mx_tcp_read(hdl, OnenetEdpRxBuf, sizeof(OnenetEdpRxBuf));
			len_real += s32Ret;
		} while (s32Ret == MXAPP_SRV_BUFF_MAX_RX);
		if (len_real > MXAPP_SRV_BUFF_MAX_RX)
		{
#if defined(__WHMX_LOG_SRV_SUPPORT__)
			if (head)
			{
				pbuf = head + strlen(head);
				kal_sprintf(pbuf, " %d", len_real);
				mx_log_srv_write(head, strlen(head), KAL_TRUE);
			}
#endif
			break;
		}

#if defined(__WHMX_LOG_SRV_SUPPORT__)
		if (head)
		{
			pbuf = head + strlen(head);
			kal_sprintf(pbuf, " %d", s32Ret);
			if (s32Ret > 0)
			{
				kal_uint32 i;

				pbuf = head + strlen(head);
				for (i = 0; i<((s32Ret>MXAPP_SRV_BUFF_MAX_RX) ? MXAPP_SRV_BUFF_MAX_RX : s32Ret); i++)
				{
					kal_sprintf(pbuf, " %02x", OnenetEdpRxBuf[i]);
					pbuf += 3;
				}
			}
			mx_log_srv_write(head, strlen(head), KAL_TRUE);
			mxapp_trace("%s", head);
		}
#endif
		if (s32Ret > 0)
		{
			mxapp_srv_recv_handle(OnenetEdpRxBuf, s32Ret);
		}
		break;

	case MX_TCP_EVT_PIPE_BROKEN:
	case MX_TCP_EVT_HOST_NOT_FOUND:
	case MX_TCP_EVT_PIPE_CLOSED:
		mxapp_srv_recon_cnt++;
		if (mxapp_srv_recon_cnt >= MXAPP_SRV_RECON_COUNT_MAX)
		{
			mxapp_srv_recon_cnt = 0;
			mxapp_srv_disconnect();
			/*****/
			if (KAL_FALSE == mx_srv_call_connected())
			{
				mmi_flight_mode_switch_for_mx(1);/*normal--->flight*/
				StopTimer(MXAPP_TIMER_CMD_TIMEOUT_SERVICE);
				StartTimer(MXAPP_TIMER_CMD_TIMEOUT_SERVICE, MODE_FLIGHT_TO_NORMAL_INTERVAL, mxapp_srv_reconnect);
			}
			else
			{
				mxapp_srv_reconnect();
			}
			/*****/
		}
		else
		{
			mxapp_srv_reconnect();
		}
		break;

	default:
		mxapp_srv_reconnect();
		break;
	}
}

static void mxapp_srv_connect(void)
{
	kal_int8 hdl = mxapp_srv_hdl;

	FUNC_ENTER;

	if (hdl != -1) return;

	/*****/
	mmi_flight_mode_switch_for_mx(0);/*flight--->normal*/
	/*****/

	mxapp_srv_con_state = 0;
	mxapp_srv_con_login = 0;

	if (KAL_TRUE == mx_srv_call_connected())
	{
		hdl = -1;
	}
	else
	{
		hdl = mx_tcp_connect(MXAPP_SRV_ADDR_IP, MXAPP_SRV_ADDR_PORT, mx_net_get_apn(0), mxapp_srv_ind);
	}
	if (hdl < 0)
	{
		mxapp_srv_reconnect();
	}
	else
	{
		mxapp_srv_hdl = hdl;
		mxapp_srv_con_state = 1;

		StopTimer(MXAPP_TIMER_CMD_TIMEOUT_SERVICE);
		StartTimer(MXAPP_TIMER_CMD_TIMEOUT_SERVICE, MXAPP_SRV_CONN_TIMEOUT, mxapp_srv_reconnect);
	}

	mxapp_trace("%s leave (%d)\r\n", __FUNCTION__, hdl);
}

static void mxapp_srv_disconnect(void)
{
	FUNC_ENTER;

	StopTimer(MXAPP_TIMER_INTERACTION_SERVICE);
	StopTimer(MXAPP_TIMER_CMD_TIMEOUT_SERVICE);
	StopTimer(MXAPP_TIMER_DISCONNECT_SERVICE);
	StopTimer(MXAPP_TIMER_RECONNECT_SERVICE);
	if (mxapp_srv_hdl > 0)
	{
		mx_tcp_close(mxapp_srv_hdl);
		mxapp_srv_hdl = -1;
		mxapp_srv_con_state = 0;
		mxapp_srv_con_login = 0;
		mxapp_srv_send_err = 0;
	}
	mxapp_srv_con_req_cnt = 0;

	FUNC_LEAVE;
}

static void mxapp_srv_reconnect(void)
{
	FUNC_ENTER;

	mxapp_srv_disconnect();
	StartTimer(MXAPP_TIMER_RECONNECT_SERVICE, MXAPP_SRV_RECON_TIMEOUT, mxapp_srv_connect);

	FUNC_LEAVE;
}


kal_int32 mxapp_srv_send(kal_uint8 *dat_in, kal_uint16 in_len, mx_srv_cb cb)
{
	kal_int32 ret = -1;

	FUNC_ENTER;

	if (mxapp_srv_con_state == 0)
	{
		mxapp_srv_connect();
		return ret;
	}

	if ((mxapp_srv_con_state == 2) && (dat_in != NULL) && (in_len > 0))
	{
		ret = mx_tcp_write(mxapp_srv_hdl, dat_in, in_len);
		mxapp_trace("mxapp_srv_send (%d,%d)\r\n", ret, in_len);
		if (ret != in_len)	//(ret <= 0)
		{
			mxapp_srv_reconnect();
		}
		else
		{
#if ((MXAPP_SRV_HEART_TIMEOUT == 0)&&(MXAPP_SRV_DISCON_TIMEOUT > 0))
			StopTimer(MXAPP_TIMER_DISCONNECT_SERVICE);
			StartTimer(MXAPP_TIMER_DISCONNECT_SERVICE, MXAPP_SRV_DISCON_TIMEOUT, mxapp_srv_disconnect);
#endif

#if MXAPP_SRV_DEBUG_TXRX
			{
				kal_uint8 *log_buf = edp_dbg_buf, *log_buf_h = edp_dbg_buf;
				kal_uint32 i;

				kal_mem_set(log_buf, 0, sizeof(edp_dbg_buf));
				for (i = 0; i < ((in_len < MXAPP_SRV_BUFF_MAX_TX) ? in_len : MXAPP_SRV_BUFF_MAX_TX); i++)
				{
					kal_sprintf(log_buf, "%x ", dat_in[i]);
					log_buf += strlen(log_buf);
				}
				for (i = 0; i < strlen(log_buf_h);)
				{
					mxapp_trace("%s\r\n", (log_buf_h + i));
					i += 127;
				}
			}
#endif
		}
		mxapp_srv_send_err = (ret == in_len) ? 0 : 1;
	}

	FUNC_LEAVE;

	return ret;
}


#define MXAPP_SRV_CON_KEEP_TIMEOUT	(600)	/*unit:s*/
#define MXAPP_SRV_HEART_TIMEOUT_REDUNDANCE	(20)	/*unit:s*/
#define MXAPP_SRV_HEART_TIMEOUT	((MXAPP_SRV_CON_KEEP_TIMEOUT-MXAPP_SRV_HEART_TIMEOUT_REDUNDANCE)*1000) /*unit:ms*/
#define MXAPP_SRV_HEART_SEND_ACK_TIMEOUT	((5+MXAPP_SRV_HEART_TIMEOUT_REDUNDANCE)*1000) /*unit:ms*/
#define MXAPP_SRV_DISCON_TIMEOUT	(0) /*unit:ms*/


static void mxapp_srv_heart_callback(void)
{
#if (MXAPP_SRV_HEART_TIMEOUT > 0)
	FUNC_ENTER;

	if (KAL_FALSE == mx_srv_call_connected())
	{
		StartTimer(MXAPP_TIMER_INTERACTION_SERVICE, MXAPP_SRV_HEART_SEND_ACK_TIMEOUT, mxapp_srv_reconnect);
		mx_srv_heartbeat_jtt();
	}
	else
	{
		StartTimer(MXAPP_TIMER_INTERACTION_SERVICE, 5000, mxapp_srv_heart_callback);
	}

	FUNC_LEAVE;
#endif
}

static void mxapp_srv_heart(void)
{
#if (MXAPP_SRV_HEART_TIMEOUT > 0)
	FUNC_ENTER;

	StopTimer(MXAPP_TIMER_INTERACTION_SERVICE);
	if (mxapp_srv_hdl > 0)
	{
		StartTimer(MXAPP_TIMER_INTERACTION_SERVICE, MXAPP_SRV_HEART_TIMEOUT, mxapp_srv_heart_callback);
	}

	FUNC_LEAVE;
#endif
}