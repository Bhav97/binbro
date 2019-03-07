#include "c_types.h"
#include "mem.h"
#include "ets_sys.h"
#include "osapi.h"
#include "gpio.h"
#include "os_type.h"


#include "user_interface.h"
#include "osapi.h"
#include "espconn.h"
#include "os_type.h"
#include "mem.h"
#include "queue.h"

#include "user_config.h"
#include "wifi/wifi.h"


static ETSTimer wifi_timer;
bool connected = false;
uint8_t connected_devices = 0;
ip_addr_t dns_ip;

extern struct scan_config sc_config;
extern struct station_config st_config, mesh_config;
extern struct softap_config sa_config;

enum MODE_STATUS {
	NO_NETWORK = 101,
	MESH_NODE = 102,
	ROOT_NODE = 103
};

typedef enum MODE_STATUS mode_status_t;

// the root node acts as a bridge between the outside world and the mesh network
static void ICACHE_FLASH_ATTR
wifi_handle_event_cb(System_Event_t *evt)
{
	switch(evt->event) {

		case EVENT_STAMODE_CONNECTED:
			os_printf(
				"connected to %s channel %d\n", 
				evt->event_info.connected.ssid, 
				evt->event_info.connected.channel
				);
			break;

		case EVENT_STAMODE_DISCONNECTED:
			os_printf(
				"disconnected from %s, due to code:%d\n", 
				evt->event_info.disconnected.ssid, 
				evt->event_info.disconnected.reason
				);
			break;

		case EVENT_STAMODE_AUTHMODE_CHANGE:
			os_printf(
				"auth mode updated from %d to %d\n", 
				evt->event_info.auth_change.old_mode, 
				evt->event_info.auth_change.new_mode
				);
			break;
		case EVENT_STAMODE_GOT_IP:
			connected = true;
			set_dns_server();
			ip_addr_t my_ip = evt->event_info.got_ip.ip;
			patch_ifs(my_ip, false);
			os_printf(
				"ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR, 
				IP2STR(&my_ip),
				IP2STR(&evt->event_info.got_ip.mask),
				IP2STR(&evt->event_info.got_ip.gw)
				);
			os_printf("\n");
			break;

		case EVENT_SOFTAPMODE_STACONNECTED:
			connected_devices++;
			
			ip_addr_t ap_ip;
			IP4_ADDR(&ap_ip, 192, 168, 4, 1);

			patch_ifs(ap_ip, true);
			os_printf(
				"station: " MACSTR " join, AID = %d\n",
				MAC2STR(evt->event_info.sta_connected.mac),
				evt->event_info.sta_connected.aid
				);
			break;

		case EVENT_SOFTAPMODE_STADISCONNECTED:
			connected_devices--;
			os_printf(
				"station: " MACSTR " leave, AID = %d\n",
				MAC2STR(evt->event_info.sta_disconnected.mac),
				evt->event_info.sta_disconnected.aid
				);
			break;
		
		case EVENT_STAMODE_DHCP_TIMEOUT:
			os_printf("Received EVENT_STAMODE_DHCP_TIMEOUT.\n");
			wifi_station_disconnect();
			wifi_station_connect();
			break;
		
		/* nop */
		case EVENT_SOFTAPMODE_PROBEREQRECVED:
		//case EVENT_SOFTAPMODE_DISTRIBUTE_STA_IP:
		case EVENT_MAX:
		default:
			break;

	}
}


void ICACHE_FLASH_ATTR
choose_network_mode(void *arg, STATUS status) 
{
	os_printf("[INFO] Starting search for best network post scan\n");
	if (status ==  OK) {
		mode_status_t mode_stat = NO_NETWORK;
		struct bss_info *best_network;
		sint8_t rssi = -100;
		struct bss_info *network = (struct bss_info *) arg;

		while (network != NULL) {
			os_printf("[INFO] AP: %s (%d dBm)\n", network->ssid, network->rssi);

			if (os_memcmp(network->ssid, AP_SSID, sizeof(AP_SSID)) == 0) {
				mode_stat = MESH_NODE;
				if (rssi < network->rssi) {
					best_network = network;
					rssi = network->rssi;
				}
			}

			if (os_memcmp(network->ssid, STA_SSID, sizeof(STA_SSID)) == 0 && mode_stat != MESH_NODE) {
				mode_stat = ROOT_NODE;
			}
			network = STAILQ_NEXT(network, next);
		}

		switch (mode_stat) {
			case MESH_NODE: {
				// if (!wifi_station_disconnect())
				// 	os_printf("[ERR] wifi_station_disconnect: fail");
				os_printf("[INFO] Found mesh: \"%s\"(" MACSTR ") (%d dBm)\n", 
					best_network->ssid, MAC2STR(best_network->bssid) ,best_network->rssi);
				strncpy(st_config.ssid, AP_SSID, sizeof(AP_SSID));
				strncpy(st_config.password, AP_PASS, sizeof(AP_PASS));
				os_memcpy(st_config.bssid, best_network->bssid, sizeof(best_network->bssid));
				st_config.bssid_set = 1;
				if (!wifi_station_set_config(&st_config))
					os_printf("wifi_station_set_config: fail\n");

				// wifi_station_set_auto_connect(0);
				if (!wifi_station_connect())
					os_printf("wifi_station_connect: fail");
				break;
			}
			case ROOT_NODE: {
				if (!wifi_station_set_config(&st_config))
					os_printf("[ERR] wifi_station_set_config: fail");

				if (!wifi_station_connect())
					os_printf("[ERR] wifi_station_connect: fail");

				// wifi_station_set_auto_connect(1);
				break;
			}
			case NO_NETWORK:
			default:
				// system_deep_sleep( 10 seconds)
				os_printf("[INFO] cannot find "STA_SSID". cannot become root.\n");
				os_printf("[INFO] cannot find "AP_SSID". cannot join mesh\n");
				system_restart();
				while(1);
				break;
		}

	} else {
		os_printf("[ERR] scan cb_fn: STATUS: %d", (uint8_t) status);
		system_restart();
		while(1);
	}
}

void ICACHE_FLASH_ATTR
wifi_init()
{	
	char hostname[16];
	os_sprintf(hostname, "ESP-0x%x", system_get_chip_id());

	// turn of auto connect, find best network first
	if (!wifi_station_set_auto_connect(0))
		os_printf("[ERR] wifi_station_set_auto_connect: fail\n");

	wifi_set_event_handler_cb(wifi_handle_event_cb);

	os_printf("setting scan cache limit to max(5)\n");
	// cache 5 networks per scan ; max 5
	if (!wifi_station_ap_number_set((uint8) 5))
		os_printf("wifi_station_ap_number_set: fail\n");

	os_printf("[INFO] setting operation mode to STA+AP\n");
	// set as STA+AP and save configuration to flash
	if (!wifi_set_opmode(STATIONAP_MODE))
		os_printf("[ERR] wifi_set_opmode: fail\n");

	os_printf("[INFO] setting hostname to %s\n", hostname);
	if (!wifi_station_set_hostname(hostname))
		os_printf("[ERR] wifi_station_set_hostname: fail");

	os_printf("[INFO] scanning for networks\n");
	// scan for networks and find best network
	if (!wifi_station_scan(NULL, choose_network_mode))
		os_printf("[ERR] wifi_station_scan: fail");
}

void ICACHE_FLASH_ATTR
softap_init() {
	if(!wifi_softap_set_config(&sa_config))
		os_printf("[ERR] wifi_softap_set_config_current: fail\n");
}
