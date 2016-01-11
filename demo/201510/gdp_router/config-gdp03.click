// See https://github.com/nikhildps/gdp_router_click/blob/master/nikhil_nat/gdp_router_primary_proxy.click

define($SADDR em1, $SPORT 8007, $BOOTADDR em1, $BOOTPORT 8007, $CANBEPROXY true, $WPORT 15000, $DEBUG 1)
gdp :: GDPRouterNat($SADDR, $SPORT, $BOOTADDR, $BOOTPORT, $CANBEPROXY, $WPORT, $DEBUG)
