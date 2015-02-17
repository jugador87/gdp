#!/usr/bin/env python

from MISC import *
from GDP_NAME import *
from GDP_DATUM import *


class GDP_GCL:

    """
    A class that represents a GCL handle. A GCL handle resembles an open
        file handle in various ways. However, it is still different in
        certain ways
    """

    # We need to keep a list of all the open GCL handles we have, and
    #   their associated gdp handles. I don't see any cleaner solution
    #   to how to differentiate between events associated for different
    #   GCLs. Also, we no longer can free the handles automatically.
    #   Why? How would python know that we are not planning to get an
    #   event for a gcl handle?

    # C pointer to python object mapping
    object_dir = {}

    class gdp_gcl_t(Structure):

        "Corresponds to gdp_gcl_t structure exported by C library"
        pass

    class gdp_event_t(Structure):

        "Corresponds to gdp_event_t structure exported by C library"
        pass

    def __init__(self, name, iomode):
        """
        Open a GCL with given name and io-mode
        Creating new GCL's is no longer supported.

        name=<name-of-gcl>, iomode=<mode>
        name is a GDP_NAME object
        mode is one of the following: GDP_MODE_ANY, GDP_MODE_RO, GDP_MODE_AO
        """

        # self.ptr is just a C style pointer, that we will assign to something
        self.ptr = POINTER(self.gdp_gcl_t)()

        # we do need an internal represenation of the name.
        gcl_name_python = name.internal_name()
        # convert this to a string that ctypes understands. Some ctypes magic
        # ahead
        buf = create_string_buffer(gcl_name_python, 32)
        gcl_name_ctypes_ptr = cast(byref(buf), POINTER(GDP_NAME.name_t))
        gcl_name_ctypes = gcl_name_ctypes_ptr.contents

        # open an existing gcl
        __func = gdp.gdp_gcl_open
        __func.argtypes = [GDP_NAME.name_t, c_int,
                           c_void_p, POINTER(POINTER(self.gdp_gcl_t))]
        __func.restype = EP_STAT

        estat = __func(gcl_name_ctypes, iomode, None, pointer(self.ptr))
        check_EP_STAT(estat)

        # Also add itself to the global list of objects
        # XXX: See if there is a cleaner way of dealing with this?
        self.object_dir[addressof(self.ptr.contents)] = self

    def __del__(self):
        "close this GCL handle, and free the allocated resources"

        if bool(self.ptr) == False:   # null pointers have a false boolean value
            # Null pointer => no memory allocated => nothing to clean up
            #   this happens if __init__ did not finish
            return

        # remove the entry from object directory
        self.object_dir.pop(addressof(self.ptr.contents), None)

        # call the C function to free associated C memory block
        __func = gdp.gdp_gcl_close
        __func.argtypes = [POINTER(self.gdp_gcl_t)]
        __func.restype = EP_STAT

        estat = __func(self.ptr)
        check_EP_STAT(estat)
        return

    def read(self, recno):
        """
        Returns a datum dictionary. The dictionary has the following keys:
            - recno: the record number for this GDP
            - ts   : the timestamp, which itself is a dictionary with the keys
                        being tv_sec, tv_nsec, tv_accuracy
            - data : the actual data associated with this datum.
        """

        datum = GDP_DATUM()
        __recno = gdp_recno_t(recno)

        __func = gdp.gdp_gcl_read
        __func.argtypes = [
            POINTER(self.gdp_gcl_t), gdp_recno_t, POINTER(GDP_DATUM.gdp_datum_t)]
        __func.restype = EP_STAT

        estat = __func(self.ptr, __recno, datum.gdp_datum)
        check_EP_STAT(estat)

        datum_dict = {}
        datum_dict["recno"] = datum.getrecno()
        datum_dict["ts"] = datum.getts()
        datum_dict["data"] = datum.getbuf()

        return datum_dict

    def append(self, datum_dict):
        """
        Write a datum to the GCL. The datum should be a dictionary, with
            the only valid key being 'data'. The value is the actual 
            data that is to be written
        """

        datum = GDP_DATUM()

        if "data" in datum_dict.keys():
            datum.setbuf(datum_dict["data"])

        __func = gdp.gdp_gcl_append
        __func.argtypes = [
            POINTER(self.gdp_gcl_t), POINTER(GDP_DATUM.gdp_datum_t)]
        __func.restype = EP_STAT

        estat = __func(self.ptr, datum.gdp_datum)
        check_EP_STAT(estat)

        return

    # XXX: Check if this works.
    # More details here: http://python.net/crew/theller/ctypes/tutorial.html#callback-functions
    # The first argument, I believe, is the return type, which I think is void*
    gdp_gcl_sub_cbfunc_t = CFUNCTYPE(c_void_p, POINTER(gdp_gcl_t),
                                     POINTER(GDP_DATUM.gdp_datum_t), c_void_p)

    def __subscribe(self, start, numrecs, timeout, cbfunc, cbarg):
        """
        This works somewhat similar to the subscribe in GDP C api.
        callback functions is experimental. Events are better for now.
        """

        # casting start to ctypes
        __start = gdp_recno_t(start)

        # casting numrecs to ctypes
        __numrecs = c_int32(numrecs)

        # we get timeout as a dictionary, that we then translate to a C
        # structure
        class __EP_TIME_SPEC(Structure):
            pass

        __EP_TIME_SPEC._fields_ = [("tv_sec", c_int64),
                                   ("tv_nsec", c_uint32),
                                   ("tv_accuracy", c_float)]

        # if timeout is None, then we just skip this
        if timeout == None:
            __timeout = None
        else:
            __timeout = __EP_TIME_SPEC()
            __timeout.tv_sec = c_int64(timeout['tv_sec'])
            __timeout.tv_nsec = c_uint32(timeout['tv_nsec'])
            __timeout.tv_accuracy = c_float(timeout['tv_accuracy'])

        # casting the python function to the callback function
        if cbfunc == None:
            __cbfunc = None
        else:
            __cbfunc = self.gdp_gcl_sub_cbfunc_t(cbfunc)

        __func = gdp.gdp_gcl_subscribe
        if cbfunc == None:
            __func.argtypes = [POINTER(
                self.gdp_gcl_t), gdp_recno_t, c_int32, POINTER(__EP_TIME_SPEC),
                c_void_p, c_void_p]
        else:
            __func.argtypes = [POINTER(
                self.gdp_gcl_t), gdp_recno_t, c_int32, POINTER(__EP_TIME_SPEC),
                self.gdp_gcl_sub_cbfunc_t, c_void_p]

        __func.restype = EP_STAT

        estat = __func(
            self.ptr, __start, __numrecs, __timeout, __cbfunc, cbarg)
        check_EP_STAT(estat)
        return estat

    def subscribe(self, start, numrecs, timeout):
        """
        Subscriptions. Refer to the C-API for more details
        For now, callbacks are not exposed to end-user. Events are 
            generated instead.
        """
        return self.__subscribe(start, numrecs, timeout, None, None)

    def __multiread(self, start, numrecs, cbfunc, cbarg):
        """
        similar to multiread in the GDP C API
        """

        # casting start to ctypes
        __start = gdp_recno_t(start)

        # casting numrecs to ctypes
        __numrecs = c_int32(numrecs)

        # casting the python function to the callback function
        if cbfunc == None:
            __cbfunc = None
        else:
            __cbfunc = self.gdp_gcl_sub_cbfunc_t(cbfunc)

        __func = gdp.gdp_gcl_multiread
        if cbfunc == None:
            __func.argtypes = [POINTER(self.gdp_gcl_t), gdp_recno_t, c_int32,
                               c_void_p, c_void_p]
        else:
            __func.argtypes = [POINTER(self.gdp_gcl_t), gdp_recno_t, c_int32,
                               self.gdp_gcl_sub_cbfunc_t, c_void_p]
        __func.restype = EP_STAT

        estat = __func(self.ptr, __start, __numrecs, __cbfunc, cbarg)
        check_EP_STAT(estat)
        return estat

    def multiread(self, start, numrecs):
        """
        Multiread. Refer to the C-API for details.
        For now, callbacks are not exposed to end-user. Events are
            generated instead.
        """

        return self.__multiread(self, start, numrecs, None, None)

    def print_to_file(self, fh, detail, indent):
        """
        Print this GDP object to a file. Could be sys.stdout
            The actual printing is done by the C library
        """

        __fh = PyFile_AsFile(fh)

        __func = gdp.gdp_gcl_print
        __func.argtypes = [POINTER(self.gdp_gcl_t), FILE_P, c_int, c_int]
        # ignore the return type

        __func(self.ptr, __fh, c_int(detail), c_int(indent))
        return

    def getname(self):
        "Get the name of this GCL, returns a GDP_NAME object"

        __func = gdp.gdp_gcl_getname
        __func.argtypes = [POINTER(self.gdp_gcl_t)]
        __func.restype = POINTER(GDP_NAME.name_t)

        gcl_name_pointer = __func(self.ptr)
        gcl_name = string_at(gcl_name_pointer, 32)
        return GDP_NAME(gcl_name)

    @classmethod
    def get_next_event(cls, timeout):
        """
        Get the events for any open GCL.
        """

        # we get timeout as a dictionary, that we then translate to a C
        # structure
        class __EP_TIME_SPEC(Structure):
            pass

        __EP_TIME_SPEC._fields_ = [("tv_sec", c_int64),
                                   ("tv_nsec", c_uint32),
                                   ("tv_accuracy", c_float)]

        __func1 = gdp.gdp_event_next

        # if timeout is None, then we just skip this
        if timeout == None:
            __timeout = None
            __func1.argtypes = [c_void_p, c_void_p]
        else:
            __timeout = __EP_TIME_SPEC()
            __timeout.tv_sec = c_int64(timeout['tv_sec'])
            __timeout.tv_nsec = c_uint32(timeout['tv_nsec'])
            __timeout.tv_accuracy = c_float(timeout['tv_accuracy'])

            __func1.argtypes = [c_void_p, POINTER(__EP_TIME_SPEC)]

        __func1.restype = POINTER(cls.gdp_event_t)

        # Don't set this for a particular GCL.
        event_ptr = __func1(None, __timeout)
        if bool(event_ptr) == False:  # Null pointers have a false boolean value
            return None

        # now get the associated GCL handle
        __func2 = gdp.gdp_event_getgcl
        __func2.argtypes = [POINTER(cls.gdp_event_t)]
        __func2.restype = POINTER(cls.gdp_gcl_t)

        gcl_ptr = __func2(event_ptr)

        # now find this in the dictionary
        gcl_handle = cls.object_dir.get(addressof(gcl_ptr.contents), None)

        # also get the associated datum object
        __func3 = gdp.gdp_event_getdatum
        __func3.argtypes = [POINTER(cls.gdp_event_t)]
        __func3.restype = POINTER(GDP_DATUM.gdp_datum_t)

        datum_ptr = __func3(event_ptr)
        datum = GDP_DATUM(ptr=datum_ptr)
        datum_dict = {}
        datum_dict["recno"] = datum.getrecno()
        datum_dict["ts"] = datum.getts()
        datum_dict["data"] = datum.getbuf()

        # find the type of the event
        __func4 = gdp.gdp_event_gettype
        __func4.argtypes = [POINTER(cls.gdp_event_t)]
        __func4.restype = c_int

        event_type = __func4(event_ptr)

        # also free the event
        __func5 = gdp.gdp_event_free
        __func5.argtypes = [POINTER(cls.gdp_event_t)]
        __func5.restype = EP_STAT

        estat = __func5(event_ptr)
        check_EP_STAT(estat)

        gdp_event = {}
        gdp_event["gcl_handle"] = gcl_handle
        gdp_event["datum"] = datum_dict
        gdp_event["type"] = event_type

        return gdp_event
