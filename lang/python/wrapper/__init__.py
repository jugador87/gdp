#!/usr/bin/python

"""
A Python API for libgdp. Uses ctypes and shared library versions of libgdp
and libep.

This package exports two classes, and a few utility functions.
- GDP_NAME:     represents names in GDP
- GDP_GCL :     represents a GCL file handle

The utility functions are:
- gdp_init
- gdp_run_accept_event_loop

"""

from MISC import GDP_MODE_ANY, GDP_MODE_RO, GDP_MODE_AO, \
    gdp_init, gdp_run_accept_event_loop
from GDP_NAME import GDP_NAME
from GDP_GCL import GDP_GCL

__all__ = [GDP_MODE_ANY, GDP_MODE_RO, GDP_MODE_AO,
           gdp_init, gdp_run_accept_event_loop, GDP_NAME, GDP_GCL]
