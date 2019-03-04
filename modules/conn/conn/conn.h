#ifndef CONN_H
#define CONN_H

void server_init(void);
void udp_tx_data(uint8_t *data, uint16_t len, uint32_t ip_addr);
void patch_ifs(ip_addr_t ip, bool nat);

#endif // 	#endif /* CONN_H_ */