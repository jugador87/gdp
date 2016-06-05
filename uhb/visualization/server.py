#!/usr/bin/env python

"""
A visualization tool for time-series data stored in GDP logs
"""

from twisted.internet import reactor
from twisted.web.resource import Resource
from twisted.web.server import Site
from twisted.web.resource import NoResource

import gdp
import urlparse
import argparse
import json
import gviz_api
from datetime import datetime


class DataResource(Resource):

    isLeaf = True
    def __init__(self):
        Resource.__init__(self)


    def __handleQuery(self, request):

        args = request.args
        response = ""

        try:

            # get query parameters
            logname = args['logname'][0]
            startRec = int(args['startRec'][0])
            endRec = int(args['endRec'][0])
            step = int(args['step'][0])
            key = args['key'][0]

            # reqId processing
            reqId = 0
            tqx = args['tqx'][0].split(';')
            for t in tqx:
                _t = t.split(':')
                if _t[0] == "reqId":
                    reqId = _t[1]
                    break

            lh = gdp.GDP_GCL(gdp.GDP_NAME(logname), gdp.GDP_MODE_RO)

            mostrecent = lh.read(-1)['recno']

            # create data
            reclist = range(startRec, endRec, step)
            data = []
            for t in reclist:

                # make sure we are in the right range
                if (t>=0 and (t<1 or t>mostrecent)) or \
                        (t<0 and (t*(-1)>=mostrecent)):
                    break

                # go read the data
                datum = lh.read(t)
                _time = datetime.fromtimestamp(datum['ts']['tv_sec'] + \
                                    (datum['ts']['tv_nsec']*1.0/10**9))
                _raw_data = json.loads(datum['data'])[key]
                if _raw_data == "true":
                    _data = 1.0
                elif _raw_data == "false":
                    _data = 0.0
                else:
                    _data = float(_raw_data)
                data.append((_time,_data))

            desc = [('time', 'datetime'), ('key', 'number')]
            data_table = gviz_api.DataTable(desc)
            data_table.LoadData(data)

            del lh

            response = data_table.ToJSonResponse(
                            columns_order=('time', 'key'), order_by='time',
                            req_id = reqId)

        except Exception as e:
            print "Something bad happened"
            request.setResponseCode(500)
            response = "500: internal server error"

        return response



    def render_GET(self, request):

        print "Received request:", request.uri
        print request.path
        print "args", request.args

        if request.path == "/":
            # serve the main HTML file
            fh = open("index.html", "r")
            return fh.read()

        elif request.path == "/datasource":
            return self.__handleQuery(request)

        else:
            return NoResource()




if __name__ == '__main__': 

    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port", type=int, default=8888,
                        help="TCP port to serve requests on")

    args = parser.parse_args()

    gdp.gdp_init()

    site = Site(DataResource())
    reactor.listenTCP(args.port, site)

    print "Starting web-server on port", args.port
    reactor.run()

