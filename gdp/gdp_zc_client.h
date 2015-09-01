#include <avahi-common/simple-watch.h>
#include <avahi-common/error.h>
#include <avahi-common/malloc.h>
#include <avahi-common/domain.h>
#include <avahi-common/llist.h>
#include <avahi-client/client.h>
#include <avahi-client/lookup.h>

typedef struct ZCInfo {
    char *address;
    uint16_t port;
    struct ZCInfo *info_next;
} ZCInfo;

int gdp_zc_scan();
ZCInfo **gdp_zc_get_infolist();
char *gdp_zc_addr_str(ZCInfo **list);
int gdp_zc_free_infolist(ZCInfo **list);
