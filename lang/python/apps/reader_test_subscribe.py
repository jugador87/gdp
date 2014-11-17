#!/usr/bin/python

import sys
sys.path.append("../")

import wrapper as gdp


def main(name_str):

    # create a python object
    gcl_name = gdp.GCL_NAME(name_str)

    # Assume that the GCL already exists
    gcl_handle = gdp.GDP_GCL(open=True, name=gcl_name, iomode=gdp.GDP_MODE_RO)

    # this is the actual subscribe call
    gcl_handle.subscribe(0,0,None,None,None)

    while True:

        # This blocks, until there is a new event
        event = gdp.GDP_GCL.get_next_event(True)
        datum = event["datum"]
        handle = event["gcl_handle"]
        print event

if __name__=="__main__":
    if len(sys.argv)==1:
        main(None)
    else:
        main(sys.argv[1])
