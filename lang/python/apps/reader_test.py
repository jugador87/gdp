#!/usr/bin/env python

"""
A simple program to read a range of records in a given log
"""

import sys
sys.path.append("../")
import gdp


def main(name_str, start, stop):

    # create a python object
    gcl_name = gdp.GDP_NAME(name_str)
    print gcl_name.printable_name()

    # Assume that the GCL already exists
    gcl_handle = gdp.GDP_GCL(gcl_name, gdp.GDP_MODE_RO)

    # initialize this to the first record number
    recno = start
    while recno<=stop:
        try:
            datum = gcl_handle.read(recno)
            print datum
            recno += 1
        except:
            # End of log.
            # this can happen for reasons other than end of the log, but 
            #   I don't see there to be any other easy way to find that out
            break

if __name__ == "__main__":

    if len(sys.argv) < 4:
        print "Usage: %s <gcl-name> <start-rec> <stop-rec>" % sys.argv[0]
        sys.exit(1)

    # Change this to point to a gdp_router
    gdp.gdp_init()
    main(sys.argv[1], int(sys.argv[2]), int(sys.argv[3]))
