#ifndef _USER_CONFIG_H_
#define _USER_CONFIG_H_

#define CFG_HOLDER	0x00FF55A4	/* Change this value to load default configurations */
#define CFG_LOCATION	0x3C	/* Please don't change or if you know what you doing */
#define CLIENT_SSL_ENABLE

#define STA_SSID "HOME"
#define STA_PASS "madarchod"
#define STA_TYPE AUTH_WPA2_PSK

#define PING_SAMPLE_PERIOD 1000 // 250 ms between each sample. you could go faster if you like

#endif