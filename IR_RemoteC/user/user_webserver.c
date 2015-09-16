/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_webserver.c
 *
 * Description: The web server mode configration.
 *              Check your hardware connection with the host while use this mode.
 * Modification history:
 *     2014/3/12, v1.0 create this file.
*******************************************************************************/
#include "ets_sys.h"
#include "os_type.h"
#include "osapi.h"
#include "mem.h"
#include "user_interface.h"

#include "user_iot_version.h"
#include "espconn.h"
#include "user_json.h"
#include "user_webserver.h"
#include "driver/i2c_bmp180.h"

#include "upgrade.h"
#if ESP_PLATFORM
#include "user_esp_platform.h"
#endif

#if LIGHT_DEVICE
#include "user_light.h"
#endif


//Sensor Values & System Metrics
extern DATA_Sensors mySensors;
extern DATA_System sysParams;

uint8 upgrade_lock = 0;
LOCAL os_timer_t app_upgrade_10s;
LOCAL os_timer_t upgrade_check_timer;

void ICACHE_FLASH_ATTR user_webserver_init(uint32 port);


/******************************************************************************
 * FunctionName : sensors_get
 * Description  : get sensor values as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
sensors_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);
    char string[32];
    //Sensors are read in timer callback in user_main (extern variables to this module)
    //Now the sensor values are placed into the json encoded response
    if (os_strcmp(path, "B_Pressure") == 0) {
    	os_sprintf(string,"%s", mySensors.pBmp085);
    } else if (os_strcmp(path, "B_Temperature") == 0) {
    	os_sprintf(string,"%s", mySensors.tBmp085);
    } else if (os_strcmp(path, "B_Altitude") == 0) {
    	os_sprintf(string,"%s", mySensors.aBmp085);
    } else if (os_strcmp(path, "D_Temperature1") == 0) {
    	os_sprintf(string,"%s", mySensors.t1Ds18b20);
    } else if (os_strcmp(path, "D_Temperature2") == 0) {
    	os_sprintf(string,"%s", mySensors.t2Ds18b20);
    } else if (os_strcmp(path, "DH_Humidity") == 0) {
        os_sprintf(string, mySensors.hDht11);
    } else if (os_strcmp(path, "DH_Temperature") == 0) {
        os_sprintf(string, mySensors.tDht11);
    } else if (os_strcmp(path, "SYS_Time") == 0) {
        os_sprintf(string, sysParams.systime);
    } else if (os_strcmp(path, "SYS_Heap") == 0) {
        os_sprintf(string, sysParams.freeheap);
    } else if (os_strcmp(path, "SYS_Loopcnt") == 0) {
        os_sprintf(string, sysParams.loopcnt);
    } else if (os_strcmp(path, "SYS_WifiStatus") == 0) {
        os_sprintf(string, sysParams.wifistatus);
    } else if (os_strcmp(path, "SYS_WifiRecon") == 0) {
        os_sprintf(string, sysParams.wifireconnects);
    } else if (os_strcmp(path, "SYS_WifiMode") == 0) {
        os_sprintf(string, sysParams.wifimode);
    } else{
    	return 0;
    }
    jsontree_write_string(js_ctx, string);

    return 0;
}

LOCAL struct jsontree_callback sensor_callback =
    JSONTREE_CALLBACK(sensors_get, NULL);

JSONTREE_OBJECT(weathersensor_tree,
        		JSONTREE_PAIR("B_Pressure", &sensor_callback),
				JSONTREE_PAIR("B_Temperature", &sensor_callback),
                JSONTREE_PAIR("B_Altitude", &sensor_callback),
				//JSONTREE_PAIR("B_Temperature1", &sensor_callback), //Placeholder for DS18B20 Sensor 1
				//JSONTREE_PAIR("B_Temperature2", &sensor_callback), //Placeholder for DS18B20 Sensor 2
                JSONTREE_PAIR("DH_Humidity", &sensor_callback),
				JSONTREE_PAIR("DH_Temperature", &sensor_callback),
				JSONTREE_PAIR("SYS_Time", &sensor_callback),
				JSONTREE_PAIR("SYS_Heap", &sensor_callback),
				JSONTREE_PAIR("SYS_Loopcnt", &sensor_callback),
				JSONTREE_PAIR("SYS_WifiStatus", &sensor_callback),
				JSONTREE_PAIR("SYS_WifiRecon", &sensor_callback),
				JSONTREE_PAIR("SYS_WifiMode", &sensor_callback)
				);

JSONTREE_OBJECT(SENSORTree,
                JSONTREE_PAIR("values", &weathersensor_tree));


#if PLUG_DEVICE
/******************************************************************************
 * FunctionName : status_get
 * Description  : set up the device status as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
status_get(struct jsontree_context *js_ctx)
{
    if (user_plug_get_status() == 1) {
        jsontree_write_int(js_ctx, 1);
    } else {
        jsontree_write_int(js_ctx, 0);
    }

    return 0;
}

/******************************************************************************
 * FunctionName : status_set
 * Description  : parse the device status parmer as a JSON format
 * Parameters   : js_ctx -- A pointer to a JSON set up
 *                parser -- A pointer to a JSON parser state
 * Returns      : result
*******************************************************************************/
LOCAL int ICACHE_FLASH_ATTR
status_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
    int type;

    while ((type = jsonparse_next(parser)) != 0) {
        if (type == JSON_TYPE_PAIR_NAME) {
            if (jsonparse_strcmp_value(parser, "status") == 0) {
                uint8 status;
                jsonparse_next(parser);
                jsonparse_next(parser);
                status = jsonparse_get_value_as_int(parser);
                user_plug_set_status(status);
            }
        }
    }

    return 0;
}

LOCAL struct jsontree_callback status_callback =
    JSONTREE_CALLBACK(status_get, status_set);

JSONTREE_OBJECT(status_tree,
                JSONTREE_PAIR("status", &status_callback));
JSONTREE_OBJECT(response_tree,
                JSONTREE_PAIR("Response", &status_tree));
JSONTREE_OBJECT(StatusTree,
                JSONTREE_PAIR("switch", &response_tree));
#endif

#if LIGHT_DEVICE
LOCAL int ICACHE_FLASH_ATTR
light_status_get(struct jsontree_context *js_ctx)
{
    const char *path = jsontree_path_name(js_ctx, js_ctx->depth - 1);

    if (os_strncmp(path, "red", 3) == 0) {
        jsontree_write_int(js_ctx, user_light_get_duty(LIGHT_RED));
    } else if (os_strncmp(path, "green", 5) == 0) {
        jsontree_write_int(js_ctx, user_light_get_duty(LIGHT_GREEN));
    } else if (os_strncmp(path, "blue", 4) == 0) {
        jsontree_write_int(js_ctx, user_light_get_duty(LIGHT_BLUE));
    } else if (os_strncmp(path, "freq", 4) == 0) {
        jsontree_write_int(js_ctx, user_light_get_freq());
    }

    return 0;
}

LOCAL int ICACHE_FLASH_ATTR
light_status_set(struct jsontree_context *js_ctx, struct jsonparse_state *parser)
{
    int type;

    while ((type = jsonparse_next(parser)) != 0) {
        if (type == JSON_TYPE_PAIR_NAME) {
            if (jsonparse_strcmp_value(parser, "red") == 0) {
                uint8 status;
                jsonparse_next(parser);
                jsonparse_next(parser);
                status = jsonparse_get_value_as_int(parser);
                //                os_printf("R: %d \n",status);
                user_light_set_duty(status, LIGHT_RED);
            } else if (jsonparse_strcmp_value(parser, "green") == 0) {
                uint8 status;
                jsonparse_next(parser);
                jsonparse_next(parser);
                status = jsonparse_get_value_as_int(parser);
                //                os_printf("G: %d \n",status);
                user_light_set_duty(status, LIGHT_GREEN);
            } else if (jsonparse_strcmp_value(parser, "blue") == 0) {
                uint8 status;
                jsonparse_next(parser);
                jsonparse_next(parser);
                status = jsonparse_get_value_as_int(parser);
                //                os_printf("B: %d \n",status);
                user_light_set_duty(status, LIGHT_BLUE);
            } else if (jsonparse_strcmp_value(parser, "freq") == 0) {
                uint16 status;
                jsonparse_next(parser);
                jsonparse_next(parser);
                status = jsonparse_get_value_as_int(parser);
                //                os_printf("FREQ: %d \n",status);
                user_light_set_freq(status);
            }
        }
    }

    user_light_restart();

    return 0;
}

LOCAL struct jsontree_callback light_callback =
    JSONTREE_CALLBACK(light_status_get, light_status_set);

JSONTREE_OBJECT(rgb_tree,
                JSONTREE_PAIR("red", &light_callback),
                JSONTREE_PAIR("green", &light_callback),
                JSONTREE_PAIR("blue", &light_callback));
JSONTREE_OBJECT(sta_tree,
                JSONTREE_PAIR("freq", &light_callback),
                JSONTREE_PAIR("rgb", &rgb_tree));
JSONTREE_OBJECT(PwmTree,
                JSONTREE_PAIR("light", &sta_tree));
#endif



	/******************************************************************************
	 * FunctionName : parse_url_params
	 * Description  : parse the received data parameters from the server
	 * Parameters   : precv -- the received data
	 *                purl_frame -- the result of parsing the url
	 * Returns      : none
	*******************************************************************************/
	LOCAL void ICACHE_FLASH_ATTR
	parse_url_params(char *precv, URL_Param *purl_param)
	{
    char *str = NULL;
    uint8 length = 0;
    char *pbuffer = NULL;
    char *pbufer = NULL;
    int ipar=0;

    if (purl_param == NULL || precv == NULL) {
        return;
    }

    pbuffer = (char *)os_strstr(precv, "Host:");

    if (pbuffer != NULL) {
        length = pbuffer - precv;
        pbufer = (char *)os_zalloc(length + 1);
        pbuffer = pbufer;
        os_memcpy(pbuffer, precv, length);
        os_memset(purl_param->pParam, 0, URLSize*URLSize);
        os_memset(purl_param->pParVal, 0, URLSize*URLSize);

        if (os_strncmp(pbuffer, "GET /favicon.ico", 16) == 0) {
			purl_param->Type = GET_FAVICON;
			os_free(pbufer);
			return;
        } else if (os_strncmp(pbuffer, "GET ", 4) == 0) {
        	purl_param->Type = GET;
            pbuffer += 4;
        } else if (os_strncmp(pbuffer, "POST ", 5) == 0) {
        	purl_param->Type = POST;
            pbuffer += 5;
        }

        pbuffer ++;
        str = (char *)os_strstr(pbuffer, "?");

        if (str != NULL) {
            str ++;
            do {
            	pbuffer = (char *)os_strstr(str, "=");
            	length = pbuffer - str;
            	os_memcpy(purl_param->pParam[ipar], str, length);
            	str = (char *)os_strstr(++pbuffer, "&");
            	if(str != NULL) {
            		length = str - pbuffer;
            		os_memcpy(purl_param->pParVal[ipar++], pbuffer, length);
            		str++;
            	}
            	else {
            		str = (char *)os_strstr(pbuffer, " HTTP");
            		if(str != NULL) {
                		length = str - pbuffer;
                		os_memcpy(purl_param->pParVal[ipar++], pbuffer, length);
                		str = NULL;
            		}
            	}
            }
            while (str!=NULL);
        }

        purl_param->nPar = ipar;
        os_free(pbufer);
    } else {
        return;
    }
}


LOCAL char *precvbuffer;
static uint32 dat_sumlength = 0;
LOCAL bool save_data(char *precv, uint16 length)
{
    bool flag = false;
    char length_buf[10] = {0};
    char *ptemp = NULL;
    char *pdata = NULL;
    uint16 headlength = 0;
    static uint32 totallength = 0;

    ptemp = (char *)os_strstr(precv, "\r\n\r\n");

    if (ptemp != NULL) {
        length -= ptemp - precv;
        length -= 4;
        totallength += length;
        headlength = ptemp - precv + 4;
        pdata = (char *)os_strstr(precv, "Content-Length: ");

        if (pdata != NULL) {
            pdata += 16;
            precvbuffer = (char *)os_strstr(pdata, "\r\n");

            if (precvbuffer != NULL) {
                os_memcpy(length_buf, pdata, precvbuffer - pdata);
                dat_sumlength = atoi(length_buf);
            }
        } else {
        	if (totallength != 0x00){
        		totallength = 0;
        		dat_sumlength = 0;
        		return false;
        	}
        }
        if ((dat_sumlength + headlength) >= 1024) {
        	precvbuffer = (char *)os_zalloc(headlength + 1);
            os_memcpy(precvbuffer, precv, headlength + 1);
        } else {
        	precvbuffer = (char *)os_zalloc(dat_sumlength + headlength + 1);
        	os_memcpy(precvbuffer, precv, os_strlen(precv));
        }
    } else {
        if (precvbuffer != NULL) {
            totallength += length;
            os_memcpy(precvbuffer + os_strlen(precvbuffer), precv, length);
        } else {
            totallength = 0;
            dat_sumlength = 0;
            return false;
        }
    }

    if (totallength == dat_sumlength) {
        totallength = 0;
        dat_sumlength = 0;
        return true;
    } else {
        return false;
    }
}


/******************************************************************************
 * FunctionName : data_send
 * Description  : processing the data as http format and send to the client or server
 * Parameters   : arg -- argument to set for client or server
 *                responseOK -- true or false
 *                psend -- The send data
 * Returns      :
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
data_send(void *arg, bool responseOK, char *psend)
{
    uint16 length = 0;
    char *pbuf = NULL;
    char httphead[256];
    struct espconn *ptrespconn = arg;
    os_memset(httphead, 0, 256);

    if (responseOK) {
        os_sprintf(httphead,
                   "HTTP/1.0 200 OK\r\nContent-Length: %d\r\nServer: lwIP/1.4.0\r\nAccess-Control-Allow-Origin: *\r\n",
                   psend ? os_strlen(psend) : 0);

        if (psend) {
            os_sprintf(httphead + os_strlen(httphead),
                       "Content-type: application/json\r\nExpires: Fri, 10 Apr 2015 14:00:00 GMT\r\nPragma: no-cache\r\n\r\n");
            length = os_strlen(httphead) + os_strlen(psend);
            pbuf = (char *)os_zalloc(length + 1);
            os_memcpy(pbuf, httphead, os_strlen(httphead));
            os_memcpy(pbuf + os_strlen(httphead), psend, os_strlen(psend));
        } else {
            os_sprintf(httphead + os_strlen(httphead), "\n");
            length = os_strlen(httphead);
        }
    } else {
        os_sprintf(httphead, "HTTP/1.0 400 BadRequest\r\n\
Content-Length: 0\r\nServer: lwIP/1.4.0\r\n\n");
        length = os_strlen(httphead);
    }

    if (psend) {
#ifdef SERVER_SSL_ENABLE
        espconn_secure_sent(ptrespconn, pbuf, length);
#else
        espconn_sent(ptrespconn, pbuf, length);
#endif
    } else {
#ifdef SERVER_SSL_ENABLE
        espconn_secure_sent(ptrespconn, httphead, length);
#else
        espconn_sent(ptrespconn, httphead, length);
#endif
    }

    if (pbuf) {
        os_free(pbuf);
        pbuf = NULL;
    }
}

/******************************************************************************
 * FunctionName : json_send
 * Description  : processing the data as json format and send to the client or server
 * Parameters   : arg -- argument to set for client or server
 *                ParmType -- json format type
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
json_send(void *arg, ParmType ParmType)
{
    char *pbuf = NULL;
    pbuf = (char *)os_zalloc(jsonSize);
    struct espconn *ptrespconn = arg;

    switch (ParmType) {
        case GET_SENSORS:
            json_ws_send((struct jsontree_value *)&SENSORTree, "values", pbuf);
            break;

        default :
            break;
    }

    data_send(ptrespconn, true, pbuf);
    os_free(pbuf);
    pbuf = NULL;
}

/******************************************************************************
 * FunctionName : response_send
 * Description  : processing the send result
 * Parameters   : arg -- argument to set for client or server
 *                responseOK --  true or false
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
response_send(void *arg, bool responseOK)
{
    struct espconn *ptrespconn = arg;

    data_send(ptrespconn, responseOK, NULL);
}

void ICACHE_FLASH_ATTR
upgrade_check_func(void *arg)
{
	struct espconn *ptrespconn = arg;
	os_timer_disarm(&upgrade_check_timer);
	if(system_upgrade_flag_check() == UPGRADE_FLAG_START) {
		response_send(ptrespconn, false);
        system_upgrade_deinit();
        system_upgrade_flag_set(UPGRADE_FLAG_IDLE);
        upgrade_lock = 0;
		os_printf("local upgrade failed\n");
	} else if( system_upgrade_flag_check() == UPGRADE_FLAG_FINISH ) {
		os_printf("local upgrade success\n");
		response_send(ptrespconn, true);
		upgrade_lock = 0;
	} else {

	}


}
/******************************************************************************
 * FunctionName : upgrade_deinit
 * Description  : disconnect the connection with the host
 * Parameters   : bin -- server number
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
LOCAL local_upgrade_deinit(void)
{
    if (system_upgrade_flag_check() != UPGRADE_FLAG_START) {
    	os_printf("system upgrade deinit\n");
        system_upgrade_deinit();
    }
}


/******************************************************************************
 * FunctionName : upgrade_download
 * Description  : Processing the upgrade data from the host
 * Parameters   : bin -- server number
 *                pusrdata -- The upgrade data (or NULL when the connection has been closed!)
 *                length -- The length of upgrade data
 * Returns      : none
*******************************************************************************/
LOCAL void
local_upgrade_download(void * arg,char *pusrdata, unsigned short length)
{
    char *ptr = NULL;
    char *ptmp2 = NULL;
    char lengthbuffer[32];
    static uint32 totallength = 0;
    static uint32 sumlength = 0;
    struct espconn *pespconn = arg;

    if (totallength == 0 && (ptr = (char *)os_strstr(pusrdata, "\r\n\r\n")) != NULL &&
            (ptr = (char *)os_strstr(pusrdata, "Content-Length")) != NULL) {
        ptr = (char *)os_strstr(pusrdata, "\r\n\r\n");
        length -= ptr - pusrdata;
        length -= 4;
        totallength += length;
        os_printf("upgrade file download start.\n");
        system_upgrade(ptr + 4, length);
        ptr = (char *)os_strstr(pusrdata, "Content-Length: ");

        if (ptr != NULL) {
            ptr += 16;
            ptmp2 = (char *)os_strstr(ptr, "\r\n");

            if (ptmp2 != NULL) {
                os_memset(lengthbuffer, 0, sizeof(lengthbuffer));
                os_memcpy(lengthbuffer, ptr, ptmp2 - ptr);
                sumlength = atoi(lengthbuffer);
            } else {
                os_printf("sumlength failed\n");
            }
        } else {
            os_printf("Content-Length: failed\n");
        }
    } else {
        totallength += length;
        system_upgrade(pusrdata, length);
    }

    if (totallength == sumlength) {
        os_printf("upgrade file download finished.\n");
        system_upgrade_flag_set(UPGRADE_FLAG_FINISH);
        totallength = 0;
        sumlength = 0;
        upgrade_check_func(pespconn);
        os_timer_disarm(&app_upgrade_10s);
        os_timer_setfn(&app_upgrade_10s, (os_timer_func_t *)local_upgrade_deinit, NULL);
        os_timer_arm(&app_upgrade_10s, 10, 0);
    }
}

/******************************************************************************
 * FunctionName : webserver_recv
 * Description  : Processing the received data from the server
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pusrdata -- The received data (or NULL when the connection has been closed!)
 *                length -- The length of received data
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
webserver_recv(void *arg, char *pusrdata, unsigned short length)
{
	URL_Param *pURL_Param = NULL;
	//URL_Frame *pURL_Frame = NULL;
    char *pParseBuffer = NULL;
    bool parse_flag = false;
    struct espconn *ptrespconn = arg;
    int i;

    espconn_set_opt(ptrespconn, ESPCONN_REUSEADDR);

    if(upgrade_lock == 0){

    	parse_flag = save_data(pusrdata, length);
        if (parse_flag == false) {
        	response_send(ptrespconn, false);
        }

        pURL_Param = (URL_Param *)os_zalloc(sizeof(URL_Param));
        parse_url_params(precvbuffer, pURL_Param);
        switch (pURL_Param->Type) {
            case GET:
            	ets_uart_printf("We have a GET request.\n");
                for(i=0;i<pURL_Param->nPar;i++) {
                	//os_printf("Header: %s\n",precvbuffer);
                	ets_uart_printf("P%d: %s\n",i+1,pURL_Param->pParam[i]);
                	ets_uart_printf("V%d: %s\n",i+1,pURL_Param->pParVal[i]);
                }
                if(os_strcmp(pURL_Param->pParam[0], "request")==0) {
                    if(os_strcmp(pURL_Param->pParVal[0], "GetSensors")==0) {
                    	json_send(ptrespconn, GET_SENSORS);
                    }
                }
                json_send(ptrespconn, CONNECT_STATUS);
                break;

            case POST:
            	ets_uart_printf("We have a POST request.\n");
                 break;
        }

        if (precvbuffer != NULL){
        	os_free(precvbuffer);
        	precvbuffer = NULL;
        }
        os_free(pURL_Param);
        pURL_Param = NULL;

    }
    else if(upgrade_lock == 1){
    	local_upgrade_download(ptrespconn,pusrdata, length);
		if (precvbuffer != NULL){
			os_free(precvbuffer);
			precvbuffer = NULL;
		}
		os_free(pURL_Param);
		pURL_Param = NULL;
    }
}

/******************************************************************************
 * FunctionName : webserver_recon
 * Description  : the connection has been err, reconnection
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL ICACHE_FLASH_ATTR
void webserver_recon(void *arg, sint8 err)
{
    struct espconn *pesp_conn = arg;

    os_printf("webserver's %d.%d.%d.%d:%d err %d reconnect\n", pesp_conn->proto.tcp->remote_ip[0],
    		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
    		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port, err);
}
/******************************************************************************
 * FunctionName : webserver_discon
 * Description  : the connection has been err, reconnection
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL ICACHE_FLASH_ATTR
void webserver_discon(void *arg)
{
    struct espconn *pesp_conn = arg;

    os_printf("webserver's %d.%d.%d.%d:%d disconnect\n", pesp_conn->proto.tcp->remote_ip[0],
        		pesp_conn->proto.tcp->remote_ip[1],pesp_conn->proto.tcp->remote_ip[2],
        		pesp_conn->proto.tcp->remote_ip[3],pesp_conn->proto.tcp->remote_port);

    //user_webserver_init(SERVER_PORT);

}

/******************************************************************************
 * FunctionName : user_accept_listen
 * Description  : server listened a connection successfully
 * Parameters   : arg -- Additional argument to pass to the callback function
 * Returns      : none
*******************************************************************************/
LOCAL void ICACHE_FLASH_ATTR
webserver_listen(void *arg)
{
    struct espconn *pesp_conn = arg;

    espconn_regist_recvcb(pesp_conn, webserver_recv);
    espconn_regist_reconcb(pesp_conn, webserver_recon);
    espconn_regist_disconcb(pesp_conn, webserver_discon);
}

/******************************************************************************
 * FunctionName : user_webserver_init
 * Description  : parameter initialize as a server
 * Parameters   : port -- server port
 * Returns      : none
*******************************************************************************/
void ICACHE_FLASH_ATTR
user_webserver_init(uint32 port)
{
    LOCAL struct espconn esp_conn;
    LOCAL esp_tcp esptcp;

    esp_conn.type = ESPCONN_TCP;
    esp_conn.state = ESPCONN_NONE;
    esp_conn.proto.tcp = &esptcp;
    esp_conn.proto.tcp->local_port = port;
    esp_conn.recv_callback = NULL;
    esp_conn.sent_callback = NULL;
    esp_conn.reverse = NULL;
    espconn_regist_time(&esp_conn,0,0);
    espconn_regist_connectcb(&esp_conn, webserver_listen);

#ifdef SERVER_SSL_ENABLE
    espconn_secure_accept(&esp_conn);
#else
    espconn_accept(&esp_conn);
#endif
}
