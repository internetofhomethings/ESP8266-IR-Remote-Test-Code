#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define USE_WIFI_MODE		STATION_MODE
#define WIFI_CLIENTSSID		"YOURSSID"
#define WIFI_CLIENTPASSWORD	"YOURPW"
#define WIFI_AP_NAME		"ESP8266N4"
#define WIFI_AP_PASSWORD	"00000000"
#define TCPSERVERIP			"192.168.0.106"
#define TCPSERVERPORT		9703
#define PLATFORM_DEBUG		true
#define DNS_SVR             "YourDNS"
#define DNS_SVR_NAME        "iot"
#define DNS_TXTDATA         "version = now"

#define BLINKLED            5
//#define LWIP_DEBUG		true

#define USE_OPTIMIZE_PRINTF


#define ESP_PLATFORM        1
#define LEWEI_PLATFORM      0

#define USE_OPTIMIZE_PRINTF

#if ESP_PLATFORM
#define PLUG_DEVICE             0
#define LIGHT_DEVICE            0
#define SENSOR_DEVICE			1

#if SENSOR_DEVICE
#define HUMITURE_SUB_DEVICE         1
#define FLAMMABLE_GAS_SUB_DEVICE    0
#endif

//#define SERVER_SSL_ENABLE
//#define CLIENT_SSL_ENABLE
//#define UPGRADE_SSL_ENABLE

#define USE_DNS

#ifdef USE_DNS
#define ESP_DOMAIN      "iot.espressif.cn"
//#define ESP_DOMAIN      "google-public-dns-a.google.com"
//#define ESP_DOMAIN        "google-public-dns-b.google.com"
#endif

#define SOFTAP_ENCRYPT

#ifdef SOFTAP_ENCRYPT
#define PASSWORD	"v*%W>L<@i&Nxe!"
#endif

#if SENSOR_DEVICE
#define SENSOR_DEEP_SLEEP

#if HUMITURE_SUB_DEVICE
#define SENSOR_DEEP_SLEEP_TIME    30000000
#elif FLAMMABLE_GAS_SUB_DEVICE
#define SENSOR_DEEP_SLEEP_TIME    60000000
#endif
#endif

#if LIGHT_DEVICE
#define USE_US_TIMER
#endif

#if PLUG_DEVICE || LIGHT_DEVICE
#define BEACON_TIMEOUT  150000000
#define BEACON_TIME     50000
#endif

#define AP_CACHE           1

#if AP_CACHE
#define AP_CACHE_NUMBER    5
#endif

#elif LEWEI_PLATFORM
#endif

#endif

