#
# Regular cron jobs for the gdp package
#
0 4	* * *	root	[ -x /usr/bin/gdp_maintenance ] && /usr/bin/gdp_maintenance
