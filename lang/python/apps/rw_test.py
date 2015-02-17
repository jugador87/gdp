#!/usr/bin/python

"""
A simple program that:
- creates a new GCL with either a given human readable name on command line, or a random name
  - if the GCL exists already, complains and dies miserably. You get a python exception from
    the call to the function. It's your responsibility to catch the exception
- writes a number of  random messages, each of size fixed bytes.
- reads all the messages back
- verifies that what was written is exactly what is read back

"""



import sys
sys.path.append("../")      # So that we can actually load the python_api module

import wrapper as gdp    # load the main package
import random
import string               



def generate_random_data(N, count):
    "returns an count-sized array of N-sized alphanumeric strings"

    ret = []
    for idx in xrange(count):
        ret.append(''.join(random.choice(string.ascii_letters + string.digits) for _ in range(N)))

    return ret

def main(name_str):

    gcl_name = gdp.GDP_NAME(name_str)

    print "opening gcl", "".join(["%0.2x"%ord(x) for x in gcl_name.internal_name()])
    gcl_handle = gdp.GDP_GCL(gcl_name, gdp.GDP_MODE_AO)

    # the data that will be written
    data = generate_random_data(100,10)

    # writing the data
    for (idx,s) in enumerate(data):

        print "writing message number", idx
        datum = {"data": s}         # Create a minimalist datum object
        gcl_handle.append(datum)   # write this to the GCL

    # reading the data back
    read_data = []      # to store the data read back from the GCL
    for idx in xrange(len(data)):

        print "reading message number", idx
        datum = gcl_handle.read(idx+1)          # record numbers start from 1
        read_data.append(datum["data"])         # append the data to read_data


    # verifying the correctness
    for idx in xrange(len(data)):
        assert data[idx] == read_data[idx]

if __name__=="__main__":

    if len(sys.argv)<2:
        print "Usage: %s <gcl_name>" % sys.argv[0]
        sys.exit(1)

    gdp.gdp_init("127.0.0.1",8007)

    main(sys.argv[1])   # create a GCL with the given name
