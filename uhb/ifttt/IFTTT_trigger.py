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
A GDP based "Trigger" for IFTTT Maker channel.
(see https://ifttt.com/maker for more details)

This program subscribes to a GDP log (specified at program startup) and POSTs
any incoming data to IFTTT, that can be used as a trigger. This trigger 
is setup as follows:

  - "Event Name" is the name of the log.
  - Extra data fields: 
    "Value1": record number
    "Value2": the actual data

Requires Python GDP bindings and python-requests. To start, simply use

    ./<this-program-name> logname ifttt-api-key
"""

import argparse, requests, time
import gdp


def start_trigger(logname, key):

    ifttt_url = "https://maker.ifttt.com/trigger/" + logname + \
                                            "/with/key/" + key
    gdp_name = gdp.GDP_NAME(logname)
    gcl_handle = gdp.GDP_GCL(gdp_name, gdp.GDP_MODE_RO)

    gcl_handle.subscribe(0, 0, None)
    while True:

        # this blocks, untile there is a new event
        event = gdp.GDP_GCL.get_next_event(None)
        datum = event["datum"]
        post_data = { "value1" : datum["recno"],
                      "value2" : datum["data"]}

        print time.ctime(), "]", post_data
        r = requests.post(ifttt_url, data = post_data) 


if __name__ == '__main__': 

    parser = argparse.ArgumentParser()
    parser.add_argument("logname", nargs='+', help="Name of the log")
    parser.add_argument("key", nargs='+', help="IFTTT key for Maker Channel")

    args = parser.parse_args()

    print "Starting subscription..."
    start_trigger(args.logname[0], args.key[0])

