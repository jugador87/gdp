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
A REST bridge to receive an "Action" from IFTTT Maker channel.
(see https://ifttt.com/maker for more details)

The IFTTT Maker "Action" makes requests to a public URL. This program starts
a web-server on a specified port, and any data 'POST'-ed by IFTTT is appended
to a GDP log (specified at program startup). I will refer to such public URL
as http://example.com:8000/ below.

Requires Python GDP bindings and Python-twisted. Use as follows:

Step 1: Start the REST bridge. (Again, make sure it is publicly accessible)
    ./<this-program-name> [-p port] logname keyfile
        -p: TCP port to listen on

Step 2 (optional): check if things are working by making a POST request using
    a local web-client. e.g. 
    curl -X POST --data "Hello World" http://example.com:8000

Step 3: Create your IFTTT recipes. 
    In any recipe, you can use the 'Maker' channel as the 'Action'. The only
    'Action' available currently is 'Make a web request'. In the web-form 
    for 'that' section of IFTTT, fill it as follows:
    - URL: http://example.com:8000  (change it to suit your situation)
    - Method: POST
    - Content-type: text/plain
    - Body: this depends on what your trigger (i.e. 'this' field of the recipe)
            is. In any case, the contents of this field are appended as a new
            record each time this recipe is invoked.
"""

from twisted.internet import reactor
from twisted.web.resource import Resource
from twisted.web.server import Site

import argparse, time
import gdp


class AppendResource(Resource):

    isLeaf = True

    def __init__(self, logname, keyfile):
        self.logname = logname
        gdp_name = gdp.GDP_NAME(self.logname)
        signing_key = gdp.EP_CRYPTO_KEY(filename=keyfile, 
                                            keyform=gdp.EP_CRYPTO_KEYFORM_PEM,
                                            flags=gdp.EP_CRYPTO_F_SECRET)
        self.gcl_handle = gdp.GDP_GCL(gdp_name, gdp.GDP_MODE_AO, 
                                open_info = { 'skey': signing_key })
        Resource.__init__(self)

    def render_POST(self, request):
        val = request.content.read()
        print time.ctime(), "]", val
        self.gcl_handle.append({"data": val})
        return "OK\n"


if __name__ == '__main__': 

    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port", type=int, default=8000,
                        help="TCP port to serve requests on")
    parser.add_argument("logname", nargs='+', help="Name of the log")
    parser.add_argument("keyfile", nargs='+', help="Signing key file")

    args = parser.parse_args()

    site = Site(AppendResource(args.logname[0], args.keyfile[0]))
    reactor.listenTCP(args.port, site)
    print "Starting REST interface on port", args.port
    reactor.run()

