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
// static netif_input_fn orig_input_ap, orig_input_sta;
// static netif_linkoutput_fn orig_output_ap, orig_output_sta;
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
			// ip_addr_t dns_ip;
			// IP4_ADDR(&dns_ip, 8,8,8,8);
			// dhcps_set_DNS(&dns_ip);
			// ip_addr_t ip = evt->event_info.got_ip.ip;
			// patch_netif(ip, my_input_sta, &my_input_sta, &orig_input_ap, &orig_output_ap, false);
			patch_ifs(my_ip, false);
			os_printf(
				"got ip:" IPSTR ",mask:" IPSTR ",gw:" IPSTR, 
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
			// ip_addr4(&ap_ip) = 1;

			patch_ifs(ap_ip, true);
			os_printf(
				"station: " MACSTR "join, AID = %d\n",
				MAC2STR(evt->event_info.sta_connected.mac),
				evt->event_info.sta_connected.aid
				);
			break;

		case EVENT_SOFTAPMODE_STADISCONNECTED:
			connected_devices--;
			os_printf(
				"station: " MACSTR "leave, AID = %d\n",
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
set_user_station_config(struct bss_info *best_network, uint8_t i_am_root)
{
	struct station_config config;

	if (i_am_root) {
		config.bssid_set = 0;
		os_memcpy(config.ssid, STA_SSID, sizeof(STA_SSID));
		os_memcpy(config.password, STA_PASS, sizeof(STA_PASS));
	} else {
		// connect only if SSID and BSSID match
		config.bssid_set = 1;
		os_memcpy(config.ssid, AP_SSID, sizeof(AP_SSID));
		os_memcpy(config.password, AP_PASS, sizeof(AP_PASS));
		os_memcpy(config.bssid, best_network->bssid, 6);
	}

	if (!wifi_station_set_config(&config))
		os_printf("wifi_station_set_onfig: fail\n");
}

/*
 * Find nearest network based on rssi and check if I;m the first node
 */
void ICACHE_FLASH_ATTR
find_best_network(void *arg, STATUS status) 
{
	os_printf("Starting search for best network post scan\n");
	if (status ==  OK) {
		uint8_t i_am_root = 0;
		uint8_t ap_ssid[32] = AP_SSID;
		uint8_t sta_ssid[32] = STA_SSID;

		struct bss_info *best_network = NULL;
		
		struct bss_info *network = (struct bss_info *) arg;
		sint8 rssi = 0;

		while (network != NULL) {
			os_printf("AP: %s vs %s\n", network->ssid, ap_ssid);
			
			//os_memcpy(ssid, AP_SSID, sizeof(AP_SSID));

			if (os_memcmp(ap_ssid, network->ssid, sizeof(ap_ssid)) == 0) {
				if (rssi < network->rssi)
					best_network = network;
				
				rssi = network->rssi;
			}

			// os_memset(ssid, 0, sizeof(ssid));
			// os_memcpy(ssid, STA_SSID, sizeof(STA_SSID));
			if (os_memcmp(sta_ssid, network->ssid, sizeof(sta_ssid)) == 0) {
				i_am_root = 1;
			}
			network = STAILQ_NEXT(network, next);
		}

		if (best_network) {
			os_printf("Found best network\n");
			set_user_station_config(best_network, 0 /* don't care if router is available, mesh is up*/); 
			
			if(!wifi_station_connect())
				os_printf("wifi_station_connect: fail");

			if (!wifi_station_connect())
				os_printf("wifi_station_connect: fail\n");
		} else if (i_am_root) {
			os_printf("I AM ROOT\n");
			set_user_station_config(best_network, i_am_root);
			if(!wifi_station_connect())
				os_printf("wifi_station_connect: fail");

		} else {

			os_printf("could not find estabilished mesh!\n");
			os_printf("could not find AP for estabilishing mesh!\n");
			os_printf("retrying in 10 seconds");

			if (!system_deep_sleep(10000000)) {
				os_printf("system_deep_sleep: fail");
				system_restart();
			}
		}

	} else {
		os_printf("scan cb_fn: STATUS: %d", (uint8_t) status);
	}
}

void ICACHE_FLASH_ATTR
wifi_init(char *hostname)
{	
	// turn of auto connect, find best network first
	if (!wifi_station_set_auto_connect(0))
		os_printf("wifi_station_set_auto_connect: fail");

	os_printf("setting scan cache limit to max(5)\n");
	// cache 5 networks per scan ; max 5
	if (!wifi_station_ap_number_set((uint8) 5))
		os_printf("wifi_station_ap_number_set: fail\n");

	os_printf("setting operation mode to STA+AP\n");
	// set as STA+AP and save configuration to flash
	if (!wifi_set_opmode(STATIONAP_MODE))
		os_printf("wifi_set_opmode: fail\n");

	os_printf("setting hostname to %s\n", hostname);
	if (!wifi_station_set_hostname(hostname))
		os_printf("wifi_station_set_hostname: fail");

	wifi_set_event_handler_cb(wifi_handle_event_cb);

	os_printf("scanning for networks\n");
	// scan for networks and find best network
	if (!wifi_station_scan(NULL, find_best_network))
		os_printf("wifi_station_scan: fail");
}

void ICACHE_FLASH_ATTR
softap_init() {


	struct softap_config config;
	
	os_memset(config.ssid, 0, sizeof(config.ssid));
	os_memset(config.password, 0, sizeof(config.password));

	os_memcpy(config.ssid, AP_SSID, sizeof(AP_SSID));
	os_memcpy(config.password, AP_PASS, sizeof(AP_PASS));
	config.max_connection = MAX_DEVICES; // TODO: from binary tee to kth binary tree

	if(!wifi_softap_set_config_current(&config))
		os_printf("wifi_softap_set_config_current: fail");

	// ip_addr_t dns_ip;
	// IP4_ADDR(&dns_ip, 8,8,8,8);
	// dhcps_set_DNS(&dns_ip);
}

// static void ICACHE_FLASH_ATTR 
// patch_netif(ip_addr_t netif_ip, netif_input_fn ifn, netif_input_fn *orig_ifn, 
// 	netif_linkoutput_fn ofn, netif_linkoutput_fn *orig_ofn, bool nat)
// {
// 	struct netif *nif;

// 	for (nif = netif_list; nif != NULL && nif->ip_addr.addr != netif_ip.addr; nif = nif->next);
// 	if (nif == NULL) return;

// 	nif->napt = nat?1:0;
// 	if (ifn != NULL && nif->input != ifn) {
// 	  *orig_ifn = nif->input;
// 	  nif->input = ifn;
// 	}
// 	if (ofn != NULL && nif->linkoutput != ofn) {
// 	  *orig_ofn = nif->linkoutput;
// 	  nif->linkoutput = ofn;
// 	}
// }

// err_t ICACHE_FLASH_ATTR 
// my_input_sta (struct pbuf *p, struct netif *inp) 
// {
// 	return orig_input_sta (p, inp);
// }


