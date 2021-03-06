#!/usr/bin/env python
# ----- BEGIN LICENSE BLOCK -----
#	GDP: Global Data Plane
#	From the Ubiquitous Swarm Lab, 490 Cory Hall, U.C. Berkeley.
#
#	Copyright (c) 2015, Regents of the University of California.
#	All rights reserved.
#
#	Permission is hereby granted, without written agreement and without
#	license or royalty fees, to use, copy, modify, and distribute this
#	software and its documentation for any purpose, provided that the above
#	copyright notice and the following two paragraphs appear in all copies
#	of this software.
#
#	IN NO EVENT SHALL REGENTS BE LIABLE TO ANY PARTY FOR DIRECT, INDIRECT,
#	SPECIAL, INCIDENTAL, OR CONSEQUENTIAL DAMAGES, INCLUDING LOST
#	PROFITS, ARISING OUT OF THE USE OF THIS SOFTWARE AND ITS DOCUMENTATION,
#	EVEN IF REGENTS HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
#
#	REGENTS SPECIFICALLY DISCLAIMS ANY WARRANTIES, INCLUDING, BUT NOT
#	LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
#	FOR A PARTICULAR PURPOSE. THE SOFTWARE AND ACCOMPANYING DOCUMENTATION,
#	IF ANY, PROVIDED HEREUNDER IS PROVIDED "AS IS". REGENTS HAS NO
#	OBLIGATION TO PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS,
#	OR MODIFICATIONS.
# ----- END LICENSE BLOCK -----


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


def main(name_str, keyfile=None):

    if keyfile is not None:
        skey = gdp.EP_CRYPTO_KEY(filename=keyfile,
                                keyform=gdp.EP_CRYPTO_KEYFORM_PEM,
                                flags=gdp.EP_CRYPTO_F_SECRET)
        open_info = {'skey': skey}

    else:
        open_info = {}

    gcl_name = gdp.GDP_NAME(name_str)

    print "opening gcl", "".join(
                        ["%0.2x" % ord(x) for x in gcl_name.internal_name()])
    gcl_handle = gdp.GDP_GCL(gcl_name, gdp.GDP_MODE_RA, open_info)

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

    if len(sys.argv) < 2:
        print "Usage: %s <gcl_name> [<signing-key-file>]" % sys.argv[0]
        sys.exit(1)

    name_str = sys.argv[1]
    if len(sys.argv)>=3:
        keyfile = sys.argv[2]
    else:
        keyfile = None

    # Change this to point to a gdp_router
    gdp.gdp_init()
    main(name_str, keyfile)   # create a GCL with the given name
