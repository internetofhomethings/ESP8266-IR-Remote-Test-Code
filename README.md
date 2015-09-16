<h2><strong>IR Remote for ESP8266 Development Software</strong></h2>

This project ports the IRremote library from Arduino to the ESP8266 EspressIf SDK platform.

Setup:

1. Using the Eclipse Luna Platform setup for the EspressIf SDK, import the project in the 
IR_RemoteC folder to your choice of workspaces.

2. Open the user_config.h file and add your local WIFI credentials:
<code>
#define WIFI_CLIENTSSID	"YOURSSID"
#define WIFI_CLIENTPASSWORD	"YOURPW"
</code>

Operation:

The ESP8266 Application (user_main.c):
1. user_init() - sets up configuration of GPIO used.
           	   - Starts Wifi in station mode (Even though we do not use WiFi in this example)
           
2. loop_cb()   - timer callback with 1000ms period
               - checks state of push button switch
               - if button pushed, last IR code received is transmitted every 500 ms until button is released
               - if button released, IR receiver checked every 50 us for new codes. if received, code is saved

The current settings (change if needed in the sketch to match your configuration):

ESP8266 Static IP: 192.168.0.177
ESP8266 Server Port: 9703
Router IP: 192.168.0.1

Serial port baud: 115200 bps 

