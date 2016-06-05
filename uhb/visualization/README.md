# A simple sensor visualization tool

This directory contains a minimalistic web-server to perform visualization
of time-series data in a GDP log. The web-server is based on python-twisted,
so you of course need to have that installed.

You also need google visualization library in order to run the server side.
Download it from https://github.com/google/google-visualization-python

In order to install the visualization library, use
`sudo python ./setup.py install`
or if you prefer to do an installation in your home directory, then
`python ./setup.py install --user`


## execution

Start a server by `./server.py`. Go to 'http://localhost:8888' in a browser
window. The UI needs quite a bit of improvement, this is only a first step.

