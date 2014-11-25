#!/usr/bin/python

import sys
sys.path.append("../")

import wrapper as gdp


def main(name_str):

    # Create a GCL_NAME object from a python string provided as argument
    gcl_name = gdp.GCL_NAME(name_str)

    try:
        # Try to create a new GDP_GCL. This might fail if a GCL with given 
        #   name already exists.
        gcl_handle = gdp.GDP_GCL(create=True, name=gcl_name)
    except:
        # There's a GCL with the given name, so let's open it
        gcl_handle = gdp.GDP_GCL(open=True, name=gcl_name, iomode=gdp.GDP_MODE_AO)

    while True:

        line = sys.stdin.readline().strip() # read from stdin
        datum = {"data": line}              # Create a minimalist datum dictionary
        gcl_handle.publish(datum)           # Write this datum to the GCL


if __name__=="__main__":
    if len(sys.argv)==1:
        main(None)
    else:
        main(sys.argv[1])
