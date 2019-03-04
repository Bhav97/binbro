
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

LOCAL struct espconn udp_tx;
LOCAL uint32_t last_addr = 0;
LOCAL esp_udp udp_proto_tx;

static netif_input_fn orig_input_ap, orig_input_sta;
static netif_linkoutput_fn orig_output_ap, orig_output_sta;

static void ICACHE_FLASH_ATTR client_sent_cb(void *arg)
{
	//os_printf("web_config_client_sent_cb(): data sent to client\n");
	struct espconn *pespconn = (struct espconn *)arg;
	os_printf("client_sent_cb\n");
	espconn_disconnect(pespconn);
}

static void ICACHE_FLASH_ATTR client_discon_cb(void *arg)
{
    //os_printf("web_config_client_sent_cb(): data sent to client\n");
    struct espconn *pespconn = (struct espconn *)arg;
    os_printf("client_discon_cb\n");
    espconn_disconnect(pespconn);
}

static void ICACHE_FLASH_ATTR client_recv_cb(void *arg)
{
	os_printf("client_recv_cb\n");
}

static void ICACHE_FLASH_ATTR client_connected_cb(void *arg)
{
	os_printf("client_connected_cb\n");
	struct espconn *pespconn = (struct espconn *)arg;
	espconn_send(pespconn, "HTTP/1.0 200 OK\r\nContent-Type: text/json\r\n\r\n{SmartBIN:{fill:10}}", 8);
	espconn_regist_disconcb(pespconn, client_discon_cb);
	espconn_regist_recvcb(pespconn, (espconn_recv_callback) client_recv_cb);
	espconn_regist_sentcb(pespconn, client_sent_cb);
}


void ICACHE_FLASH_ATTR
udp_recv_cb(void *arg)
{
	// struct espconn *conn = (struct espconn *)arg;
	// uint8_t *addr_array = NULL;
	// if (conn->type == ESPCONN_TCP) {
	// 	addr_array = conn->proto.tcp->remote_ip;
	// } else {
	// 	addr_array = conn->proto.udp->remote_ip;
	// }
	// if (addr_array != NULL) {
	// 	ip_addr_t addr;
	// 	IP4_ADDR(&addr, addr_array[0], addr_array[1], addr_array[2], addr_array[3]);
	// 	last_addr = addr.addr;
	// 	os_printf("Received data from "IPSTR"\n", IP2STR(&last_addr));
	// }
	// os_printf("OK\n");
}

// void ICACHE_FLASH_ATTR
// tcp_connect_cb(void *arg)
// {
// 	struct espconn * conn  = (struct espconn *) arg;
// 	// os_printf("TCP connection received from "IPSTR":%d\n",
//  //              IP2STR(conn->proto.tcp->remote_ip), conn->proto.tcp->remote_port);
// 	os_printf("tcp_connect_cb");
// 	espconn_regist_recvcb(conn, recv_cb);
// }

void ICACHE_FLASH_ATTR
server_init()
{
	// setup tcp server
	// struct espconn tcp_conn;
	// esp_tcp tcp_proto;
	

	// tcp_proto.local_port = 5555;
	// tcp_conn.type = ESPCONN_TCP;
	// tcp_conn.state = ESPCONN_NONE;
	// tcp_conn.proto.tcp = &tcp_proto;
	// espconn_regist_connectcb(&tcp_conn, tcp_connect_cb);
	// espconn_accept(&tcp_conn);

	//setup udp server
	// struct espconn udp_conn;
	// esp_udp udp_proto;

	// udp_proto.local_port = 5555;
	// udp_conn.type = ESPCONN_UDP;
	// udp_conn.state = ESPCONN_NONE;
	// udp_conn.proto.udp = &udp_proto;
	// espconn_create(&udp_conn);
	// espconn_regist_recvcb(&udp_conn,(espconn_recv_callback) udp_recv_cb);
	os_printf("server_init():\n");
	struct espconn *conn = (struct espconn *) os_zalloc(sizeof(struct espconn));
	conn->type  = ESPCONN_TCP;
	conn->state = ESPCONN_NONE;
	conn->proto.tcp = (esp_tcp *)os_zalloc(sizeof(esp_tcp));
	conn->proto.tcp->local_port = 5555;

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