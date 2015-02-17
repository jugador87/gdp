#!/usr/bin/python

from MISC import *


class GDP_DATUM:

    """
    A class only for internal use. The C datum equivalent exposed to python
        users is of a dictionary.

    This is to avoid any side effects that the underlying buffer implementation
        may cause.
    """

    class gdp_datum_t(Structure):
        pass

    # Python representation of this is a dictionary, with the exact same fields
    class __EP_TIME_SPEC(Structure):
        pass

    __EP_TIME_SPEC._fields_ = [("tv_sec", c_int64),
                               ("tv_nsec", c_uint32),
                               ("tv_accuracy", c_float)]

    def __init__(self, **kwargs):
        """
        Constructor: Two ways of creating a new pythong GDP datum object:
            - Create a new object and allocate new memeory for it by calling
              the C library function
            - Create a new object, but associate it with an existing memory
              location.
        """
        if len(kwargs) == 0:

            # We do need to create a corresponding C data structure.
            __func = gdp.gdp_datum_new
            __func.argtypes = []
            __func.restype = POINTER(self.gdp_datum_t)

            self.gdp_datum = __func()
            self.did_i_create_it = True

        else:
            # we probably got passed a C pointer to an existing datum.
            if "ptr" in kwargs:
                self.gdp_datum = kwargs["ptr"]
                self.did_i_create_it = False
            else:
                raise Exception     # FIXME

        return

    def __del__(self):
        """
        Destructor: Does nothing if this GDP datum was created by
            passing an exisiting datum pointer
        """

        if self.did_i_create_it:

            __func = gdp.gdp_datum_free
            __func.argtypes = [POINTER(self.gdp_datum_t)]

            __func(self.gdp_datum)
        return

    def print_to_file(self, fh):
        """
        Print the GDP datum C memory location contents to a file handle fh.
            fh could be sys.stdout, or any other open file handle.
        Note: This just calls the corresponding C library function which
              handles the printing
        """

        # need to convert this file handle to a C FILE*
        __fh = PyFile_AsFile(fh)
        __func = gdp.gdp_datum_print
        __func.argtypes = [POINTER(self.gdp_datum_t), FILE_P]
        # ignore the return value

        __func(self.gdp_datum, __fh)
        return

    def getrecno(self):
        """
        Get the corresponding record number associated with this datum
        """

        __func = gdp.gdp_datum_getrecno
        __func.argtypes = [POINTER(self.gdp_datum_t)]
        __func.restype = gdp_recno_t

        ret = __func(self.gdp_datum)
        return int(ret)

    def getts(self):
        """
        Return the timestamp associated with this GDP_DATUM in the form of a
            dictionary. The keys are: tv_sec, tv_nsec and tv_accuracy
        """

        ts = self.__EP_TIME_SPEC()
        __func = gdp.gdp_datum_getts
        __func.argtypes = [
            POINTER(self.gdp_datum_t), POINTER(self.__EP_TIME_SPEC)]
        # ignore the return value

        __func(self.gdp_datum, byref(ts))
        # represent the time spec as a dictionary

        ret = {}
        ret['tv_sec'] = ts.tv_sec
        ret['tv_nsec'] = ts.tv_nsec
        ret['tv_accuracy'] = ts.tv_accuracy

        return ret

    def getdlen(self):
        "Returns the length of the data associated with this GDP_DATUM"

        __func = gdp.gdp_datum_getdlen
        __func.argtypes = [POINTER(self.gdp_datum_t)]
        __func.restype = c_size_t

        ret = __func(self.gdp_datum)
        return ret

    def getbuf(self):
        """
        Return the data associated with this GDP_DATUM. Internally, it queries
            a buffer for the data, and returns whatever it has read so far from
            the buffer.

        Effectively drains the buffer too.
        """

        class __gdp_buf_t(Structure):
            pass

        __func = gdp.gdp_datum_getbuf
        __func.argtypes = [POINTER(self.gdp_datum_t)]
        __func.restype = POINTER(__gdp_buf_t)

        gdp_buf_ptr = __func(self.gdp_datum)
        __func_read = gdp.gdp_buf_read
        __func_read.argtypes = [POINTER(__gdp_buf_t), c_void_p, c_size_t]
        __func_read.restype = c_size_t

        dlen = self.getdlen()
        tmp_buf = create_string_buffer(dlen)
        readbytes = __func_read(gdp_buf_ptr, byref(tmp_buf), dlen)
        return string_at(tmp_buf, readbytes)

    def setbuf(self, data):
        "Set the buffer to the given data. data is a python string"

        class __gdp_buf_t(Structure):
            pass

        __func = gdp.gdp_datum_getbuf
        __func.argtypes = [POINTER(self.gdp_datum_t)]
        __func.restype = POINTER(__gdp_buf_t)

        gdp_buf_ptr = __func(self.gdp_datum)

        __func_write = gdp.gdp_buf_write
        __func_write.argtypes = [POINTER(__gdp_buf_t), c_void_p, c_size_t]
        __func_write.restype = c_int

        size = c_size_t(len(data))
        tmp_buf = create_string_buffer(
            data, len(data))     # XXX: should it be +1 for null?
        written_bytes = __func_write(gdp_buf_ptr, byref(tmp_buf), size)
        return
