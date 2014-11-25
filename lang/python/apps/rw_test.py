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

def main(gcl_name):


    # this might fail with an exception if the file alread exists. 
    # We'd like to test that, so not trying to catch the exception
    gcl_handle = gdp.GDP_GCL(create=True, name=gcl_name)

    # the data that will be written
    data = generate_random_data(100,10)

    # writing the data
    for (idx,s) in enumerate(data):

        print "writing message number", idx
        datum = {"data": s}         # Create a minimalist datum object
        gcl_handle.publish(datum)   # write this to the GCL


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

    if len(sys.argv)==1:    # No name given. Generate a new random GCL
        main(None)
    else:
        # first create a GCL name object
        gcl_name = gdp.GCL_NAME(sys.argv[1])

        main(gcl_name)   # create a GCL with the given name
