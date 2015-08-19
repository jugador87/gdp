#!/usr/bin/env python

"""
A simple program to demonstrate multiread
"""

import sys
sys.path.append("../")
import gdp


def main(name_str, start, stop):

    # create a python object
    gcl_name = gdp.GDP_NAME(name_str)

    # Assume that the GCL already exists
    gcl_handle = gdp.GDP_GCL(gcl_name, gdp.GDP_MODE_RO)

    # this is the actual subscribe call
    gcl_handle.multiread(start, stop-start+1)

    # timeout
    t = {'tv_sec':0, 'tv_nsec':500*(10**6), 'tv_accuracy':0.0}

    while True:

        # This could return a None, after the specified timeout
        event = gdp.GDP_GCL.get_next_event(t)
        if event is None or event["type"] == gdp.GDP_EVENT_EOS:
            break
        datum = event["datum"]
        handle = event["gcl_handle"]
        print datum

if __name__ == "__main__":

    if len(sys.argv) < 4:
        print "Usage: %s <gcl-name> <start-recno> <stop-recno>" % sys.argv[0]
        sys.exit(1)

    # Change this to point to a gdp_router
    gdp.gdp_init()
    main(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]))
