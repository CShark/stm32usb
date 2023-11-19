#include "ncm/ncm_netif.h"
#include "ncm/ncm_device.h"

#include <netif/ethernet.h>
#include <lwip/etharp.h>
//#include <lwip/apps/dhcp_server.h>

static struct netif netif;
static const short hwaddr[6] = {0x12, 0x54, 0xF9, 0xD9, 0x1F, 0x18};

static err_t ncm_netif_output(struct netif *netif, struct pbuf *p) {
    struct pbuf *q;
    short offset = 0;
    char *buffer = NCM_GetNextTxDatagramBuffer(p->tot_len);

    for(q = p; q != NULL; q = q->next) {
        memcpy(buffer + offset, q->payload, q->len);
        offset += q->len;

        if(q->len == q->tot_len) {
            break;
        }
    }
}

void ncm_netif_poll(struct netif *netif) {
    short length = 0;
    short offset = 0;
    char *datagram;
    struct pbuf *p, *q;

    datagram = NCM_GetNextRxDatagramBuffer(&length);

    if(datagram != 0 && length > 0) {
        p = pbuf_alloc(PBUF_RAW, length, PBUF_POOL);

        if(p != NULL) {
            for(q = p; q != NULL && offset < length; q = q->next) {
                memcpy(q->payload, datagram + offset, q->len);
                offset += q->len;
            }

            if(netif->input(p, netif) != ERR_OK) {
                pbuf_free(p);
            }
        }
    }
}

err_t ncm_netif_init(struct netif *netif) {
    // Set MAC-Address
    netif->hwaddr_len = ETHARP_HWADDR_LEN;
    netif->hwaddr[0] = hwaddr[0];
    netif->hwaddr[1] = hwaddr[1];
    netif->hwaddr[2] = hwaddr[2];
    netif->hwaddr[3] = hwaddr[3];
    netif->hwaddr[4] = hwaddr[4];
    netif->hwaddr[5] = hwaddr[5];

    netif->name[0] = 'e';
    netif->name[0] = '0';

    netif->mtu = 1500;

    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_LINK_UP;

    netif->output = etharp_output;
    netif->linkoutput = ncm_netif_output;

    return ERR_OK;
}