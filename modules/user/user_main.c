#include "ets_sys.h"
#include "osapi.h"

#include "gpio.h"
#include "mem.h"
#include "queue.h"
#include "ip_addr.h"
#include "user_interface.h"
#include "espconn.h"
#include "os_type.h"
#include "wifi/wifi.h" 
// #include "conn/conn.h"
#include "stdout/stdout.h"
#include "ping/ping.h"
#include "user_config.h"

static Ping_Data ping;
static os_timer_t loop_timer;

extern volatile bool connected;
extern volatile uint8_t connected_devices;

float distance = 0;
uint32_t chip_id;

struct scan_config sc_config;
struct station_config st_config, mesh_config;
struct softap_config sa_config;
char ap_ssid[sizeof(AP_SSID)] = AP_SSID;

void ICACHE_FLASH_ATTR
loop(void) {
// 	if (connected_devices > 0) {
// 		float maxDistance = 1000; // 1 meter
// 		wifi_softap_free_station_info();
// 		struct station_info *station = (struct station_info *) wifi_softap_get_station_info();
// 		char message[32];
// 		os_memset(message, 0, sizeof(message));

// 		if (ping_ping(&ping, maxDistance, &distance) ) {
// 			/* message stucture
// 			 * chipid-distance\n
// 			 * 0x226340-19\n
// 			 */
// 			os_sprintf(message, "0x%x-%d\n", system_get_chip_id(),  (int)distance);
// 		} else {
// 			os_printf("no response\n");
// 			os_sprintf(message, "no response\n");
			
// 		}

// 		while(station) {
// 			os_printf("udp tx to: bssid :"MACSTR" at ip : "IPSTR"\n", 
// 				MAC2STR(station->bssid), IP2STR(&station->ip));
// 			udp_tx_data((uint8_t *) message, sizeof(message), station->ip.addr);
// 			station = STAILQ_NEXT(station, next);
// 		}
// 		ip_addr_t base_ip;
// 		IP4_ADDR(&base_ip, 192, 168, 1 ,19);
// 		udp_tx_data((uint8_t *) message, sizeof(message), base_ip.addr);
// 		wifi_softap_free_station_info();
		
// 	}
}

// TODO: hide all networks init by device
static void ICACHE_FLASH_ATTR
setup(void) {

	ping_init(&ping, 0, 2, PING_SAMPLE_PERIOD); // trigger=GPIO0, echo=GPIO2
	os_printf("[INFO] Initiating wifi scan\n");
	wifi_init();

	os_printf("[INFO] Initializing Access Point\n");
	
	softap_init();

	// os_printf("[INFO] Initializing tcp server\n");
	// server_init();


	// Start repeating loop timer
	os_timer_disarm(&loop_timer);
	os_timer_setfn(&loop_timer, (os_timer_func_t *) loop, NULL);
	os_timer_arm(&loop_timer, PING_SAMPLE_PERIOD, true);
}

void user_init(void) {
	// Initialize the GPIO subsystem.
	gpio_init();

	// Make uart0 work with just the TX pin. Baud:115200,n,8,1
	// The RX pin is now free for GPIO use (GPIO3).
	stdout_init();

	os_printf("[INFO] Setting up configurations\n");

	strncpy(sa_config.ssid, AP_SSID, sizeof(sa_config.ssid));
	strncpy(sa_config.password, AP_PASS, sizeof(sa_config.password));
	sa_config.max_connection = MAX_DEVICES;
	sa_config.authmode = AUTH_WPA2_PSK;
	os_printf("[INFO] AP Config:\n\rssid:%s, password: %s, max allowed devices\
		: %d\n", sa_config.ssid, sa_config.password);

	sc_config.show_hidden = 0;
	sc_config.ssid = (uint8_t *) &ap_ssid;	
	os_printf("[INFO] Scan Config:\n\tssid rule: %s, show_hidden: %d\n", sc_config.ssid, 
		sc_config.show_hidden);

	st_config.bssid_set = 0;
	strncpy(st_config.ssid, STA_SSID, sizeof(STA_SSID));
	strncpy(st_config.password, STA_PASS, sizeof(STA_PASS));
	os_printf("[INFO] Station Config:\n\tssid: %s, password: %s, match mac: %d\n", st_config.ssid, 
		st_config.password, st_config.bssid_set);

	// mesh_config.bssid_set = 1;
	// strncpy(mesh_config.ssid, AP_SSID, sizeof(AP_SSID));
	// strncpy(mesh_config.password, AP_PASS, sizeof(AP_PASS));
	// os_printf("[INFO] Mesh Config:\n\tssid: %s, password: %s, match mac: %d\n", mesh_config.ssid, 
	// 	mesh_config.password, mesh_config.bssid_set);

	//Set the setup timer
	os_timer_disarm(&loop_timer);
	os_timer_setfn(&loop_timer, (os_timer_func_t *) setup, NULL);
	os_timer_arm(&loop_timer, 1000, false);

	os_printf("\n[INFO] System started\n");
	chip_id = system_get_chip_id();
	os_printf("[INFO] Chip ID: 0x%x\n", chip_id);

	system_init_done_cb(setup);
}
