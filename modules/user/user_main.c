#include "ets_sys.h"
#include "osapi.h"

//#include "config.h"
//#include "debug.h"

#include "gpio.h"
#include "user_interface.h"
#include "mem.h"

#include "wifi.h" 
#include "stdout.h"
#include "ping.h"
#include "user_config.h"

static Ping_Data ping;

void ICACHE_FLASH_ATTR
wifiConnectCb(uint8_t status) {

  os_sprintf("/%0x/ping\n", system_get_chip_id());
  if (status == STATION_GOT_IP) {
    os_sprintf("/%0x/got_ip\n", system_get_chip_id());
  } else {
    os_sprintf("/%0x/no_ip\n", system_get_chip_id());
  }
}

void ICACHE_FLASH_ATTR
loop(void) {
  float distance = 0;
  float maxDistance = 3000; // 3 meter
  if (ping_ping(&ping, maxDistance, &distance) ) {
    os_printf("%0x-%d\n", (int)distance, system_get_chip_id());
  } else {
    os_printf("no response\n");
  }
}

static void ICACHE_FLASH_ATTR
setup(void) {
  ping_init(&ping, 2, 0, PING_SAMPLE_PERIOD); // trigger=GPIO2, echo=GPIO0

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

  INFO("\nSystem started\n");
}

