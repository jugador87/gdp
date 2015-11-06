For the demo, we will have 3 root routers 
    gdp-01.eecs.berkeley.edu
    gdp-02.eecs.berkeley.edu  ## seems to be down 
    gdp-03.eecs.berkeley.edu

Further, we need one at-least swarmbox as a secondary router.

To install gdp-router, follow these steps:

1. Run `install.sh` as root. This will download, compile and install click
   in `/opt/`. 
2. If you want the router be a primary, execute the following:
   sysctl -w net.ipv4.ping_group_range="0  2147483647"
3. If you want the router to advertise it's location via zero-conf, somehow
   install apps/gdp-zcpublish as well.
4. Install appropriate configuration file, based on the role of the gdp-router
5. Run `/opt/click_gdp <config-file>` as non-root.
