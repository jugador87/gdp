#!/usr/bin/env python

"""
A visualization tool for time-series data stored in GDP logs
"""

from twisted.internet import reactor
from twisted.web.resource import Resource
from twisted.web.server import Site

import gdp
import argparse
import json
import gviz_api
from datetime import datetime
import time


class DataResource(Resource):

    isLeaf = True
    def __init__(self):
        Resource.__init__(self)


    def __handleQuery(self, request):

        args = request.args

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

        # open GDP log
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

        return response


    def render_GET(self, request):

        start = time.time()
        resp = ""
        respCode = 200
        if request.path == "/":
            # serve the main HTML file
            fh = open("index.html", "r")
            resp = fh.read()

        elif request.path == "/datasource":
            # for querying GDP
            try:
                resp = self.__handleQuery(request)
            except Exception as e:
                request.setResponseCode(500)
                respCode = 500
                resp = "500: internal server error"

        else:
            # default to 404 not found
            request.setResponseCode(404)
            respCode = 404
            resp = "404: not found"

        end = time.time()
        print "%s]] GET: <<%s>>\t%d\t%d\t%f" % (time.strftime("%x %X"),
                                request.uri, respCode, len(resp), end-start) 
        return resp



if __name__ == '__main__': 

    parser = argparse.ArgumentParser()
    parser.add_argument("-p", "--port", type=int, default=8888,
                        help="TCP port to serve requests on")

    args = parser.parse_args()

    gdp.gdp_init()

    site = Site(DataResource())
    reactor.listenTCP(args.port, site)

    print "Starting web-server on port", args.port
    print "Log format is: (date)]] (method): <<(requestPath))>> "\
                "(responseCode) (responseLength) (timeTaken)"
    reactor.run()

