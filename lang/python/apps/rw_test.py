#!/usr/bin/env python

"""
A simple program that:
- writes a number of random messages, each of size fixed bytes.
- reads all the messages back
- verifies that what was written is exactly what is read back

"""


import sys
sys.path.append("../")
                # So that we can actually load the python_api module

import gdp    # load the main package
import random
import string


def generate_random_data(N, count):
    "returns an count-sized array of N-sized alphanumeric strings"

    ret = []
    for idx in xrange(count):
        ret.append(''.join(random.choice(string.ascii_letters + string.digits)
                   for _ in range(N)))

    return ret


def main(name_str, keyfile):

    skey = gdp.EP_CRYPTO_KEY(filename=keyfile,
                                keyform=gdp.EP_CRYPTO_KEYFORM_PEM,
                                flags=gdp.EP_CRYPTO_F_SECRET)

    gcl_name = gdp.GDP_NAME(name_str)

    print "opening gcl", "".join(
                        ["%0.2x" % ord(x) for x in gcl_name.internal_name()])
    gcl_handle = gdp.GDP_GCL(gcl_name, gdp.GDP_MODE_RA,
                                open_info={'skey':skey})

    # the data that will be written
    data = generate_random_data(100, 10)

    # writing the data
    for (idx, s) in enumerate(data):

        print "writing message", idx
        datum = {"data": s}         # Create a minimalist datum object
        gcl_handle.append(datum)   # write this to the GCL

    # reading the data back

    # to store the data read back from the GCL
    read_data = [] 
    for idx in xrange(-1*len(data),0):

        print "reading message", -1*idx, "from the end"
        datum = gcl_handle.read(idx)            # -n => n-th record from end
        read_data.append(datum["data"])         # append the data to read_data

    # verifying the correctness
    for idx in xrange(len(data)):
        if data[idx] == read_data[idx]:
            print "message %d matches" % idx

if __name__ == "__main__":

    if len(sys.argv) < 3:
        print "Usage: %s <gcl_name> <signing-key-file>" % sys.argv[0]
        sys.exit(1)

    # Change this to point to a gdp_router
    gdp.gdp_init()
    main(sys.argv[1], sys.argv[2])   # create a GCL with the given name
