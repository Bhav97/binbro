#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

#define STA_SSID "bhavanubhav"
#define STA_PASS "tellmywifiloveher"
#define STA_TYPE AUTH_WPA2_PSK

#define AP_SSID "node"
#define AP_PASS "tellmywifiloveher"
#define AP_TYPE AUTH_WPA2_PSK

#define HTTP_OK_HEAD "HTTP/1.0 200 OK"
#define HTTP_OK_RESP(CONTENT) HTTP_OK_HEAD "\r\nContent-Type: " #CONTENT "\r\n\r\n"

#define DATA_PORT 80
#define ID_PORT 101
#define LEVEL_PORT 102
#define CHANNEL 1

#define BIN_DEPTH 1000 // in mm

#define MAX_DEVICES 3 // max allowed devices

#define MAX(a, b) (((a) > (b)) ? (a) : (b))
#define MIN(a, b) (((a) < (b)) ? (a) : (b))

#define PING_SAMPLE_PERIOD 5000 // 250 ms between each sample. you could go faster if you like

#endif