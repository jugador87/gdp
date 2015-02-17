#!/usr/bin/env python
"""
An example program that logs some system information periodically.
This is just an example program that can be used as a starting point.

** Requires python-psutil to be installed **
"""

import sys
sys.path.append("../")
import gdp
import psutil
import time
import os
import json

SLEEP_INTERVAL = 60

def main(name_str):

    # Create a GDP_NAME object from a python string provided as argument
    gcl_name = gdp.GDP_NAME(name_str)

    # There's a GCL with the given name, so let's open it
    gcl_handle = gdp.GDP_GCL(gcl_name, gdp.GDP_MODE_AO)

    while True:

        # Collect information that we are going to log
        d = {}
        l = os.getloadavg()
        d['load1'], d['load5'], d['load15'] = l[0], l[1], l[2]
        d['freeram'] = psutil.phymem_usage().free
        d['nprocs'] = len(psutil.get_pid_list())

        # convert that to a nice string
        string_to_write = json.dumps(d)

        # Create a minimalist datum dictionary
        datum = {"data": string_to_write}
        gcl_handle.append(datum)           # Write this datum to the GCL
        time.sleep(SLEEP_INTERVAL)


if __name__ == "__main__":

    if len(sys.argv) < 2:
        print "Usage: %s <gcl_name>" % sys.argv[0]
        sys.exit(1)

    # Change this to point to a gdp_router
    gdp.gdp_init()
    main(sys.argv[1])
