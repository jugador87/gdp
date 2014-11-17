#!/usr/bin/python

"""
A Python API for libgdp. Uses ctypes and shared library versions of libgdp 
and libep. 

This package exports two classes, and a few utility functions.
- GCL_NAME:     represents name of a GCL
- GDP_GCL :     represents a GCL file handle

The utility functions are:
- gdp_init
- gdp_run_accept_event_loop

"""

from MISC import GDP_MODE_ANY, GDP_MODE_RO, GDP_MODE_AO, \
        gdp_init, gdp_run_accept_event_loop
from GCL_NAME import GCL_NAME
from GDP_GCL import GDP_GCL

__all__ = [GDP_MODE_ANY, GDP_MODE_RO, GDP_MODE_AO, gdp_init, gdp_run_accept_event_loop, GCL_NAME, GDP_GCL]

# TODO: Look up the environmnt variables. and use that as host, port
gdp_init("127.0.0.1:2468")
