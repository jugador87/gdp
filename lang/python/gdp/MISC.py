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


from ctypes import *
import os

# Make sure that we can actually find the .so files, irrespective of
#   where the actual python script is it.
# Not sure if there is a cleaner way of doing it.
package_directory = os.path.dirname(os.path.abspath(__file__))

# Load the DLLs. Make sure that the files actually exist ###
gdp = CDLL(os.path.join(package_directory,
           "..", "..", "..", "libs",  "libgdp.so"))
#ep = CDLL(os.path.join(package_directory,
#          "..", "..", "..", "libs", "libep.so"))
try:
    evb = CDLL("libevent.so")       # On linux
except OSError:
    evb = CDLL("libevent.dylib")    # On MAC


# hack for file pointer. Apparently this works only on Python 2.x and not with 3
# copied from
# http://stackoverflow.com/questions/16130268/python-3-replacement-for-pyfile-asfile
class FILE(Structure):
    pass
FILE_P = POINTER(FILE)
PyFile_AsFile = pythonapi.PyFile_AsFile  # problem here
PyFile_AsFile.argtypes = [py_object]
PyFile_AsFile.restype = FILE_P
# Now use the following to create the file pointer
# fp = open(filename, "w")
# corresponding FILE* is obtained by PyFile_AsFile(fp)


# GDP request ID. Not being used at least as of now, but including it anyways.
gdp_rid_t = c_uint32

# GCL record number
gdp_recno_t = c_int64


# I/O modes:
#   GDP_MODE_ANY: no mode specified
#   GDP_MODE_RO: read only
#   GDP_MODE_AO: append only
#   GDP_MODE_RA: read+append

(GDP_MODE_ANY, GDP_MODE_RO, GDP_MODE_AO, GDP_MODE_RA) = (0, 1, 2, 3)

# Event types
#   GDP_EVENT_DATA      1   // returned data
#   GDP_EVENT_EOS       2   // normal end of subscription
#   GDP_EVENT_SHUTDOWN  3   // subscription terminating because of shutdown
#   GDP_EVENT_ASTAT     4   // asynchronous status

(GDP_EVENT_DATA, GDP_EVENT_EOS, GDP_EVENT_SHUTDOWN, GDP_EVENT_ASTAT)=(1,2,3,4)

# GCL Metadata keys
# GDP_GCLMD_XID       0x00584944  // XID (external id)
# GDP_GCLMD_PUBKEY    0x00505542  // PUB (public key)
# GDP_GCLMD_CTIME     0x0043544D  // CTM (creation time)
# GDP_GCLMD_CID       0x00434944  // CID (creator id)

(GDP_GCLMD_XID, GDP_GCLMD_PUBKEY, GDP_GCLMD_CTIME, GDP_GCLMD_CID) = \
            (0x00584944, 0x00505542, 0x0043544D, 0x00434944)


class EP_STAT(Structure):
    pass
EP_STAT._fields_ = [("code", c_ulong)]


# converting EP_STAT error codes to string
def ep_stat_tostr(ep): 
    """ returns string representation of estat """
    buf = create_string_buffer(200)

    __func = gdp.ep_stat_tostr
    __func.argtypes = [EP_STAT, c_void_p, c_size_t]
    __func.restype = c_void_p

    __func(ep, buf, 200)

    return string_at(buf)


class EP_STAT_Exception(Exception):
    """
    A custom execption class to handle various ep library return values
    """ 

    def __init__(self, ep_stat):
        self.ep_stat = ep_stat
        self.msg = ep_stat_tostr(ep_stat)

    def __str__(self):
        return repr(self.msg)


# Handling EP_STAT and error code checking
class EP_STAT_SEV_WARN(EP_STAT_Exception):
    pass


class EP_STAT_SEV_ERROR(EP_STAT_Exception):
    pass


class EP_STAT_SEV_SEVERE(EP_STAT_Exception):
    pass


class EP_STAT_SEV_ABORT(EP_STAT_Exception):
    pass


def check_EP_STAT(ep_stat):
    """
    Perform basic checks on the EP_STAT. In case of not-okay code, raises
        exception, which might be caught and dealt with, if that's required
    """

    # either long is 32 bits, or 64 bits.
    # if sizeof(c_ulong)==8: shiftbits = 61       # This is from ep_stat.h
    # else: shiftbits = 29
    shiftbits = 29
    t = ep_stat.code >> shiftbits
    if t == 4:
        raise EP_STAT_SEV_WARN(ep_stat)
    if t == 5:
        raise EP_STAT_SEV_ERROR(ep_stat)
    if t == 6:
        raise EP_STAT_SEV_SEVERE(ep_stat)
    if t == 7:
        raise EP_STAT_SEV_ABORT(ep_stat)


class event_base(Structure):
    pass

# XXX: I don't think this is used anywhere, Remove, maybe?
GdpIoEventBase = POINTER(event_base)

def gdp_init(*args):
    """
    initialize the library, takes an optional argument of the form "HOST:PORT"
    """

    __func = gdp.gdp_init
    __func.argtypes = [c_void_p]
    __func.restype = EP_STAT

    if len(args) == 0:        # Use the default value
        estat = __func(None)
    else:   # Any issues with string formatting handled at C layer
        buf = create_string_buffer(args[0])
        estat = __func(buf)

    check_EP_STAT(estat)


def dbg_set(level):
    """
    Set debug level to a specified value. This is equivalent to the
        -D option in C example programs
    """
    dbg_string = create_string_buffer(level)

    __func = gdp.ep_dbg_set
    __func.argtypes = [c_void_p]

    __func(dbg_string)


def gdp_run_accept_event_loop(arg):
    "run event loop (normally run from gdp_init; never returns)"

    __func = gdp.gdp_run_accept_event_loop
    __func.argtypes = [c_void_p]
    __func.restype = c_void_p

    ret = __func(arg)
    return ret


# RECIPE FOR CASTING POINTERS
# buf = create_string_buffer(32)
# nameptr = cast(byref(buf), POINTER(gcl_name_t))
# name = nameptr.contents

# scratch code that is required to run the stuff, from
# http://stackoverflow.com/questions/4213095/python-and-ctypes-how-to-correctly-pass-pointer-to-pointer-into-dll
