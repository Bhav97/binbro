
#include "ets_sys.h"
#include "osapi.h"
#include "os_type.h"
#include "c_types.h"
#include "mem.h"

#include "lwip/ip.h"
#include "lwip/netif.h"
#include "lwip/dns.h"
#include "lwip/lwip_napt.h"
#include "lwip/app/dhcpserver.h"
#include "lwip/app/espconn.h"
#include "lwip/app/espconn_tcp.h"

#include "conn/conn.h"

extern uint32_t chip_id;
extern volatile float distance;

LOCAL struct espconn udp_tx;
LOCAL uint32_t last_addr = 0;
LOCAL esp_udp udp_proto_tx;

static netif_input_fn orig_input_ap, orig_input_sta;
static netif_linkoutput_fn orig_output_ap, orig_output_sta;

static void ICACHE_FLASH_ATTR client_sent_cb(void *arg)
{
	struct espconn *pespconn = (struct espconn *)arg;
	os_printf("client_sent_cb\n");
	// espconn_disconnect(pespconn);
}

static void ICACHE_FLASH_ATTR client_discon_cb(void *arg)
{
    //os_printf("web_config_client_sent_cb(): data sent to client\n");
    struct espconn *pespconn = (struct espconn *)arg;
    os_printf("client_discon_cb\n");
    // espconn_disconnect(pespconn);
}

static void ICACHE_FLASH_ATTR client_recv_cb(void *arg, char *data, unsigned short length)
{
	struct espconn *pespconn = (struct espconn *) arg;
	char *str;
	str = strstr(data, " /");
	if (str != NULL) {
		str = strtok(str+2, " ");
	}
	char *txbuf = (char *) os_malloc(128);
	switch(str[0]) {
		case 'i':
			os_sprintf(txbuf, "%s%s\n", HTTP_OK_RESP(text/plain), chip_id);
			os_printf("%s\n", txbuf);
			espconn_send(pespconn, txbuf, 128);
			break;
		case 'l':
			os_sprintf(txbuf, "%s%s\n", HTTP_OK_RESP(text/plain), "MESH LEVEL");
			os_printf("%s\n", txbuf);
			espconn_send(pespconn, txbuf, os_strlen(txbuf));
			break;
		case 'f':
		default : {		
			os_sprintf(txbuf, "%s%s\n", HTTP_OK_RESP(text/plain), "PORTAL");
			os_printf("%s\n", txbuf);
			espconn_send(pespconn, txbuf, sizeof(txbuf));
		}
	}
	os_free(txbuf);
	// espconn_disconnect(pespconn);
}

static void ICACHE_FLASH_ATTR client_connected_cb(void *arg)
{
	os_printf("client_connected_cb\n");
	struct espconn *pespconn = (struct espconn *)arg;

	char *txbuf = (char *) os_malloc(128);
	os_sprintf(txbuf, "%s<html><body><h1>0x%d</h1></body></html>\n", HTTP_OK_RESP(text/html), distance);
	espconn_send(pespconn, txbuf, os_strlen(txbuf));
	espconn_regist_disconcb(pespconn, client_discon_cb);
	espconn_regist_recvcb(pespconn, (espconn_recv_callback) client_recv_cb);
	espconn_regist_sentcb(pespconn, client_sent_cb);
	os_printf("%s", txbuf);
	os_free(txbuf);
}

void ICACHE_FLASH_ATTR
server_init()
{
	os_printf("server_init():\n");
	struct espconn *conn = (struct espconn *) os_zalloc(sizeof(struct espconn));
	conn->type  = ESPCONN_TCP;
	conn->state = ESPCONN_NONE;
	conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	conn->proto.tcp->local_port = 80;

	espconn_regist_connectcb(conn, client_connected_cb);
	espconn_accept(conn);
}

void ICACHE_FLASH_ATTR udp_tx_data(uint8_t *data, uint16_t len, uint32_t ip_addr) {
	// Set the destination IP address and port.
	os_memcpy(udp_proto_tx.remote_ip, &ip_addr, 4);
	udp_proto_tx.remote_port = 5555;

	// Prepare the UDP "connection" structure.
	udp_tx.type = ESPCONN_UDP;
	udp_tx.state = ESPCONN_NONE;
	udp_tx.proto.udp = &udp_proto_tx;

	// Send the UDP packet.
	espconn_create(&udp_tx);
	espconn_send(&udp_tx, data, len);
	espconn_delete(&udp_tx);
}

err_t ICACHE_FLASH_ATTR
my_input_ap(struct pbuf *p, struct netif *inp)
{
  orig_input_ap (p, inp);
}

err_t ICACHE_FLASH_ATTR
my_output_ap(struct netif *outp, struct pbuf *p)
{
  orig_output_ap (outp, p);
}

err_t ICACHE_FLASH_ATTR
my_input_sta(struct pbuf *p, struct netif *inp)
{
  orig_input_sta (p, inp);
}

err_t ICACHE_FLASH_ATTR
my_output_sta(struct netif *outp, struct pbuf *p)
{
  orig_output_sta (outp, p);
}

static void ICACHE_FLASH_ATTR
patch_netif(ip_addr_t netif_ip,
            netif_input_fn ifn,
            netif_input_fn *orig_ifn,
            netif_linkoutput_fn ofn,
            netif_linkoutput_fn *orig_ofn,
            bool nat)
{
  struct netif *nif;

  for (nif = netif_list;
       nif != NULL && nif->ip_addr.addr != netif_ip.addr;
       nif = nif->next);

  if (nif == NULL)
  {
    return;
  }

  nif->napt = nat?1:0;
  if (ifn != NULL && nif->input != ifn)
  {
    *orig_ifn = nif->input;
    nif->input = ifn;
  }
  if (ofn != NULL && nif->linkoutput != ofn)
  {
    *orig_ofn = nif->linkoutput;
    nif->linkoutput = ofn;
  }
}

void patch_ifs(ip_addr_t ip, bool nat) {
	os_printf("patching\n");
	patch_netif(ip, my_input_sta, &orig_input_sta, my_output_sta, &orig_output_sta, nat);
}

void set_dns_server() {
	ip_addr_t dns_ip;
	IP4_ADDR(&dns_ip, 8, 8, 8, 8);
	dhcps_set_DNS(&dns_ip);
}