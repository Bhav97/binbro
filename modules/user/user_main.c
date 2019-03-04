// #include "lwip/ip.h"
// #include "lwip/netif.h"
// #include "lwip/dns.h"
// #include "lwip/lwip_napt.h"
// #include "lwip/ip_route.h"
// #include "lwip/app/dhcpserver.h"
// #include "lwip/app/espconn.h"
// #include "lwip/app/espconn_tcp.h"

#include "ets_sys.h"
#include "osapi.h"

//#include "config.h"
//#include "debug.h"

#include "gpio.h"
#include "mem.h"
// #include "ip_addr.h"
#include "queue.h"
#include "user_interface.h"
#include "espconn.h"
#include "os_type.h"
#include "wifi/wifi.h" 
#include "conn/conn.h"
#include "stdout/stdout.h"
#include "ping/ping.h"
#include "user_config.h"

static Ping_Data ping;
static os_timer_t loop_timer;

extern volatile bool connected;
extern volatile uint8_t connected_devices;

void ICACHE_FLASH_ATTR
loop(void) {
	if (connected_devices > 0) {
		float distance = 0;
		float maxDistance = 1000; // 1 meter
		
		wifi_softap_free_station_info();
		struct station_info *station = (struct station_info *) wifi_softap_get_station_info();
		char message[32];
		os_memset(message, 0, sizeof(message));

		if (ping_ping(&ping, maxDistance, &distance) ) {
			/* message stucture
			 * chipid-distance\n
			 * 0x226340-19\n
			 */
			os_sprintf(message, "0x%x-%d\n", system_get_chip_id(),  (int)distance);
		} else {
			os_printf("no response\n");
			os_sprintf(message, "no response\n");
			
		}

		while(station) {
			os_printf("udp tx to: bssid :"MACSTR" at ip : "IPSTR"\n", 
				MAC2STR(station->bssid), IP2STR(&station->ip));
			udp_tx_data((uint8_t *) message, sizeof(message), station->ip.addr);
			station = STAILQ_NEXT(station, next);
		}
		wifi_softap_free_station_info();
		
	}
}

// TODO: hide all networks init by device
static void ICACHE_FLASH_ATTR
setup(void) {
  ping_init(&ping, 2, 0, PING_SAMPLE_PERIOD); // trigger=GPIO2, echo=GPIO0
	os_printf("Initiating wifi scan\n");
	char hostname[16];

	os_printf("Initializing Access Point\n");
	softap_init();

	os_sprintf(hostname, "ESP-0x%x", system_get_chip_id());
	wifi_init(hostname);

	os_printf("Initializing connection for mesh propogation\n");
	server_init();

	/* works only for upward propogation*/
	/* something like this could also work for downward propogation as well, but I'm lazy*/
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

  //Set the setup timer
  os_timer_disarm(&loop_timer);
  os_timer_setfn(&loop_timer, (os_timer_func_t *) setup, NULL);
  os_timer_arm(&loop_timer, 1000, false);

  //INFO("\nSystem started\n");
  os_printf("\nSystem started\n");
  os_printf("Chip ID: 0x%x\n", system_get_chip_id());

  system_init_done_cb(setup);
}
