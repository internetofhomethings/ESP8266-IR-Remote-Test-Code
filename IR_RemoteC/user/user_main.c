/******************************************************************************
 * Copyright 2013-2014 Espressif Systems (Wuxi)
 *
 * FileName: user_main.c
 *
 * Description: entry file of user application
 *
 * Modification history:
 *     2014/1/1, v1.0 create this file.
 *     2015/09/15, v2.0 Customized for IR Remote Demo Application
*******************************************************************************/
#include "ets_sys.h"
#include "osapi.h"

#include "user_interface.h"
#include "user_config.h"

#include "user_devicefind.h"
#include "user_webserver.h"
#include "c_types.h"
#include "espconn.h"
#include "../../ThirdParty/include/lwipopts.h"

#include "driver/uart.h"

#include <os_type.h>
#include <gpio.h>
#include "driver/gpio16.h"
#include "driver/EspIRremote.h"
#include "driver/EspIRremoteInt.h"

#define USE_US_TIMER 1

//#define DELAY 2000 /* milliseconds */
#define sleepms(x) os_delay_us(x*1000);

#if ESP_PLATFORM
#include "user_esp_platform.h"
#endif

#ifdef SERVER_SSL_ENABLE
#include "ssl/cert.h"
#include "ssl/private_key.h"
#else
#ifdef CLIENT_SSL_ENABLE
unsigned char *default_certificate;
unsigned int default_certificate_len = 0;
unsigned char *default_private_key;
unsigned int default_private_key_len = 0;
#endif
#endif
#define DELAY 1000 /* milliseconds */

LOCAL os_timer_t loop_timer;
extern volatile irparams_t irparams;
extern int ets_uart_printf(const char *fmt, ...);
extern void ets_wdt_enable (void);
extern void ets_wdt_disable (void);


typedef enum {
	WIFI_CONNECTING,
	WIFI_CONNECTING_ERROR,
	WIFI_CONNECTED,
	TCP_DISCONNECTED,
	TCP_CONNECTING,
	TCP_CONNECTING_ERROR,
	TCP_CONNECTED,
	TCP_SENDING_DATA_ERROR,
	TCP_SENT_DATA
} tConnState;

struct espconn Conn;
esp_tcp ConnTcp;
extern int ets_uart_printf(const char *fmt, ...);
int (*console_printf)(const char *fmt, ...) = ets_uart_printf;

//mDNS
static char szH[40],szS[10],szN[30];
struct mdns_info thismdns;

static ETSTimer WiFiLinker;
static tConnState connState = WIFI_CONNECTING;

LOCAL os_timer_t loop_timer;
LOCAL nTcnt=0;
//Sensor Values & System Metrics
DATA_Sensors mySensors;
DATA_System sysParams;

static void wifi_check_ip(void *arg);
LOCAL void ICACHE_FLASH_ATTR loop_cb(void *arg);

const char *WiFiMode[] =
{
		"NULL",		// 0x00
		"STATION",	// 0x01
		"SOFTAP", 	// 0x02
		"STATIONAP"	// 0x03
};

#ifdef PLATFORM_DEBUG
// enum espconn state, see file /include/lwip/api/err.c
const char *sEspconnErr[] =
{
		"Ok",                    // ERR_OK          0
		"Out of memory error",   // ERR_MEM        -1
		"Buffer error",          // ERR_BUF        -2
		"Timeout",               // ERR_TIMEOUT    -3
		"Routing problem",       // ERR_RTE        -4
		"Operation in progress", // ERR_INPROGRESS -5
		"Illegal value",         // ERR_VAL        -6
		"Operation would block", // ERR_WOULDBLOCK -7
		"Connection aborted",    // ERR_ABRT       -8
		"Connection reset",      // ERR_RST        -9
		"Connection closed",     // ERR_CLSD       -10
		"Not connected",         // ERR_CONN       -11
		"Illegal argument",      // ERR_ARG        -12
		"Address i"
		"Error in   use",        // ERR_USE        -13
		"Low-level netif error", // ERR_IF         -14
		"Already connected"      // ERR_ISCONN     -15
};
#endif

// Instantiate IR results object.
decode_results results;
int lastButtonState;



#define DEGREES_C 0
#define DEGREES_F 1

#define LOW  0
#define HIGH 1

//GPIO usage for IR
//int IR_SEND_PIN = 4;     //GPIO2;
int IR_SEND_PIN = 6;     //GPIO12;
int IR_RECV_PIN = 7;     //GPIO13;
int IR_BUTTON_PIN = 5;   //GPIO14;
int IR_STATUS_PIN = 0;  //GPIO16;

// Storage for the recorded code
int codeType = -1; // The type of code
unsigned long codeValue; // The code value if not raw
unsigned int rawCodes[RAWBUF]; // The durations if raw
int codeLen; // The length of the code
int toggle = 0; // The RC5/6 toggle state

// Stores the code for later playback
// Most of this code is just logging
void ICACHE_FLASH_ATTR storeCode(decode_results *results) {
  codeType = results->decode_type;
  int count = results->rawlen;
  int i;
  if (codeType == UNKNOWN) {
	ets_uart_printf("Received unknown code, saving as raw\r\n");
     codeLen = results->rawlen - 1;
    // To store raw codes:
    // Drop first value (gap)
    // Convert from ticks to microseconds
    // Tweak marks shorter, and spaces longer to cancel out IR receiver distortion
    for (i = 1; i <= codeLen; i++) {
      if (i % 2) {
        // Mark
        rawCodes[i - 1] = results->rawbuf[i]*USECPERTICK - MARK_EXCESS;
        ets_uart_printf(" m");
      }
      else {
        // Space
        rawCodes[i - 1] = results->rawbuf[i]*USECPERTICK + MARK_EXCESS;
        ets_uart_printf(" s");
      }
      ets_uart_printf("%d",rawCodes[i - 1]);
    }
	ets_uart_printf("\r\n");
  }
  else {
    if (codeType == NEC) {
      ets_uart_printf("Received NEC: ");
      if (results->value == REPEAT) {
        // Don't record a NEC repeat value as that's useless.
    	ets_uart_printf("repeat; ignoring.\r\n");
        return;
      }
    }
    else if (codeType == SONY) {
   	  ets_uart_printf("Received SONY: ");
    }
    else if (codeType == RC5) {
      ets_uart_printf("Received RC5: ");
    }
    else if (codeType == RC6) {
      ets_uart_printf("Received RC6: ");
    }
    else {
  	  ets_uart_printf("Unexpected codeType ");
  	  ets_uart_printf("%d",codeType);
  	  ets_uart_printf("\r\n");
    }
	  ets_uart_printf("%X",results->value);
    codeValue = results->value;
    codeLen = results->bits;
  }
}


void ICACHE_FLASH_ATTR sendCode(int repeat) {
  //ets_wdt_disable();
  //os_intr_lock();

  if (codeType == NEC) {
    if (repeat) {
      sendNEC(REPEAT, codeLen);
  	  ets_uart_printf("Sent NEC repeat\r\n");
    }
    else {
      sendNEC(codeValue, codeLen);
  	  ets_uart_printf("Sent NEC ");
  	  ets_uart_printf("%X\r\n",codeValue);
    }
  }
  else if (codeType == SONY) {
      sendSony(codeValue, codeLen);
	  ets_uart_printf("Sent Sony ");
	  ets_uart_printf("%X\r\n",codeValue);
  }
  else if (codeType == RC5 || codeType == RC6) {
    if (!repeat) {
      // Flip the toggle bit for a new button press
      toggle = 1 - toggle;
    }
    // Put the toggle bit into the code to send
    codeValue = codeValue & ~(1 << (codeLen - 1));
    codeValue = codeValue | (toggle << (codeLen - 1));
    if (codeType == RC5) {
  	  ets_uart_printf("Sent RC5 ");
  	  ets_uart_printf("%X\r\n",codeValue);
      sendRC5(codeValue, codeLen);
    }
    else {
      sendRC6(codeValue, codeLen);
  	  ets_uart_printf("Sent RC6 ");
  	  ets_uart_printf("%X\r\n",codeValue);
    }
  }
  else if (codeType == UNKNOWN /* i.e. raw */) {
    // Assume 38 KHz
    sendRaw(rawCodes, codeLen, 38);
	ets_uart_printf("Sent raw\r\n");
  }
  //os_intr_unlock();
  //ets_wdt_enable();
}


static void ICACHE_FLASH_ATTR wifi_check_ip(void *arg)
{
	static int serverinit=0;
	struct ip_info ipConfig;
	os_timer_disarm(&WiFiLinker);
	switch(wifi_station_get_connect_status())
	{
		case STATION_GOT_IP:
			wifi_get_ip_info(STATION_IF, &ipConfig);
			if(ipConfig.ip.addr != 0) {
				connState = WIFI_CONNECTED;
				#ifdef PLATFORM_DEBUG
				ets_uart_printf("WiFi connected\r\n");
				ets_uart_printf("IP: %03d.%03d.%03d.%03d\r\n",ipConfig.ip.addr&0xFF,(ipConfig.ip.addr>>8)&0xFF,(ipConfig.ip.addr>>12)&0xFF,(ipConfig.ip.addr>>16)&0xFF);
				//Initialize mDNS Responder
				strcpy(szH,DNS_SVR);
				thismdns.host_name = &szH[0];
				strcpy(szS,DNS_SVR_NAME);
				thismdns.server_name = szS;
				thismdns.server_port =SERVER_PORT;
				strcpy(szN,DNS_TXTDATA);
				thismdns.txt_data[0] = szN;
				thismdns.ipAddr = ipConfig.ip.addr;
				espconn_mdns_init(&thismdns);
				//espconn_mdns_server_register();
				//espconn_mdns_enable();

				#endif
				connState = TCP_CONNECTING;
				//Start Web Server (Upon first connection)
				if(!serverinit) {
					user_webserver_init(SERVER_PORT);
					serverinit=1;
				}
				//Start periodic loop
				os_timer_disarm(&loop_timer);
				os_timer_setfn(&loop_timer, (os_timer_func_t *)loop_cb, (void *)0);
				os_timer_arm(&loop_timer, DELAY, 10);
				//Start listening for IR commands
				enableIRIn();
				return;
			}
			break;
		case STATION_WRONG_PASSWORD:
			connState = WIFI_CONNECTING_ERROR;
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting error, wrong password\r\n");
			#endif
			break;
		case STATION_NO_AP_FOUND:
			connState = WIFI_CONNECTING_ERROR;
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting error, ap not found\r\n");
			#endif
			break;
		case STATION_CONNECT_FAIL:
			connState = WIFI_CONNECTING_ERROR;
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting fail\r\n");
			#endif
			break;
		default:
			connState = WIFI_CONNECTING;
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("WiFi connecting...\r\n");
			#endif
	}
	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
	os_timer_arm(&WiFiLinker, 1000, 0);
}



void ICACHE_FLASH_ATTR setup_wifi_st_mode(void)
{
	//wifi_set_opmode((wifi_get_opmode()|STATIONAP_MODE)&USE_WIFI_MODE);
	struct station_config stconfig;
	wifi_station_disconnect();
	wifi_station_dhcpc_stop();
	if(wifi_station_get_config(&stconfig))
	{
		os_memset(stconfig.ssid, 0, sizeof(stconfig.ssid));
		os_memset(stconfig.password, 0, sizeof(stconfig.password));
		os_sprintf(stconfig.ssid, "%s", WIFI_CLIENTSSID);
		os_sprintf(stconfig.password, "%s", WIFI_CLIENTPASSWORD);
		if(!wifi_station_set_config(&stconfig))
		{
			#ifdef PLATFORM_DEBUG
			ets_uart_printf("ESP8266 not set station config!\r\n");
			#endif
		}
	}
    //Configure IP
	wifi_station_dhcpc_stop();
    LOCAL struct ip_info info;
    IP4_ADDR(&info.ip, 192, 168, 0, 177);
    IP4_ADDR(&info.gw, 192, 168, 0, 1);
    IP4_ADDR(&info.netmask, 255, 255, 255, 0);
    wifi_set_ip_info( STATION_IF, &info);
    ets_uart_printf("ESP8266 Broadcast if:%d\r\n",wifi_get_broadcast_if());
    sleepms(1000);
    wifi_station_connect();
	wifi_station_dhcpc_start();
	wifi_station_set_auto_connect(1);
	#ifdef PLATFORM_DEBUG
	ets_uart_printf("ESP8266 in STA mode configured.\r\n");
	#endif
	// Wait for Wi-Fi connection and start TCP connection
	os_timer_disarm(&WiFiLinker);
	os_timer_setfn(&WiFiLinker, (os_timer_func_t *)wifi_check_ip, NULL);
	os_timer_arm(&WiFiLinker, 1000, 0);
    /*****************************************************
	//Start Web Server (Upon first connection)
	//if(!serverinit) {
		user_webserver_init(SERVER_PORT);
		//serverinit=1;
	//}
	wifi_get_ip_info( STATION_IF, &info);
	ets_uart_printf("ESP8266 IP:%x\r\n",info.ip);
	//Start periodic loop
	os_timer_disarm(&loop_timer);
	os_timer_setfn(&loop_timer, (os_timer_func_t *)loop_cb, (void *)0);
	os_timer_arm(&loop_timer, DELAY, 10);
	//Start listening for IR commands
	enableIRIn();
    *****************************************************/
}


LOCAL void ICACHE_FLASH_ATTR loop_cb(void *arg)
{
	char szT[32];
    // If button pressed, send the code.
     int buttonState;
    buttonState = gpio_read(IR_BUTTON_PIN);
    if (lastButtonState == HIGH && buttonState == LOW) {
  	   ets_uart_printf("Released\r\n");
	   enableIRIn(); // Re-enable receiver
    }

   if (buttonState) {
  	   ets_uart_printf("Pressed, sending\r\n");
  	   gpio16_output_set(HIGH);
 	   sendCode(lastButtonState == buttonState);
 	   gpio16_output_set(LOW);
  	   sleepms(50); // Wait a bit between retransmissions
   }
   else if (decode(&results)) {
  	   gpio16_output_set(HIGH);
	   storeCode(&results);
	   resume(); // resume receiver
  	   ets_uart_printf("\r\n");
  	   gpio16_output_set(LOW);
   }
   lastButtonState = buttonState;

    //Read Sensors & copy values here
    // Change nTcnt%1 to nTcnt%n for n actions...
	switch(nTcnt++%1) {
		case 0:
			//Placeholder 0
			ets_uart_printf("Iteration: %d Button:%d CodeType:%d RcvState/Pin/Val:%d/%d/%d\r\n",nTcnt,buttonState,codeType,irparams.rcvstate,irparams.recvpin,irparams.value);
			break;
		case 1:
			//Placeholder 1
			break;
		case 2:
			//Placeholder 2
			break;
		case 3:
			//Placeholder 3
			break;
		default:
			break;
	}

}

void user_rf_pre_init(void)
{
	system_phy_set_rfoption(2);
}

/******************************************************************************
 * FunctionName : user_init
 * Description  : entry of user application, init user function here
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
void user_init(void)
{
	system_timer_reinit();
	os_delay_us(2000000); //wait 2 sec
	ets_wdt_enable();
	ets_wdt_disable();

	user_rf_pre_init();

	// Configure the UART
	uart_init(BIT_RATE_115200, BIT_RATE_115200);

	//Status LED turned off here (Signify FW running)
	gpio16_output_conf();
	gpio16_output_set(0);

	// Setup IR GPIO channels
	set_gpio_mode(IR_SEND_PIN, GPIO_FLOAT, GPIO_OUTPUT);  //GPIO12 IR SEND
	set_gpio_mode(IR_RECV_PIN, GPIO_PULLUP, GPIO_INPUT);   //GPIO13 IR RECV
	set_gpio_mode(IR_BUTTON_PIN, GPIO_PULLUP, GPIO_INPUT); //GPIO14 IR BUTN
	gpio_write(IR_SEND_PIN, 1); //Set GPIO12 LO -> IR On

	IRrecv(IR_RECV_PIN);
	//Initialize Wifi in STA Mode (CNX to WIFI)
	setup_wifi_st_mode();


#if ESP_PLATFORM
    //user_esp_platform_init();
#endif

    //user_devicefind_init();
#ifdef SERVER_SSL_ENABLE
    user_webserver_init(SERVER_SSL_PORT);
#else
    //user_webserver_init(SERVER_PORT);
#endif


}

