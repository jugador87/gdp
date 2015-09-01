#include <gdp/gdp_zc_client.h>

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main() {
    ZCInfo *i, **list;
    char *zcstr;

    printf("start browse\n");
    if (gdp_zc_scan()) {
        printf("getting info\n");
        /* always need to retrieve list */
        list = gdp_zc_get_infolist();

        /* you can access info as a linked list */
        for (i = *list; i; i = i->info_next) {
            printf("host:%s port: %d\n", i->address, i->port);
        }

        /* or you can access info as a string */
        zcstr = gdp_zc_addr_str(list);
        if (zcstr) {
            printf("list: %s\n", zcstr);
            /* need to free the string after you're done */
            free(zcstr);
        } else {
            printf("list fail\n");
        }

        /* you always need to free the list after you're done */
        printf("freeing info\n");
        gdp_zc_free_infolist(list);
        return 0;
    } else {
        return 1;
    }
}
