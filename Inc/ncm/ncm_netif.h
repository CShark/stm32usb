#ifndef __NCM_NETIF_H
#define __NCM_NETIF_H

#include "lwip/netif.h"

err_t ncm_netif_init(struct netif *netif);
void ncm_netif_poll(struct netif *netif);

#endif
