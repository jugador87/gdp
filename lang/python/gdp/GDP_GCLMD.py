#!/usr/bin/env python

from MISC import *


class GDP_GCLMD:

    """
    GCL Metadata equivalent -- for internal use only.
    The C equivalent exposed to python users is of a dictionary.

    """

    class gdp_gclmd_t(Structure):
        pass


    def __init__(self, **kwargs):
        """
        Constructor: calls the C-function to allocate memory.
        """

        if len(kwargs)==0:
            __func = gdp.gdp_gclmd_new
            __func.argtypes = [c_int]
            __func.restype = POINTER(self.gdp_gclmd_t)

            self.gdp_gclmd_ptr = __func(c_int(0))
            self.did_i_create_it = True

        else:
            if "ptr" in kwargs:
                self.gdp_gclmd_ptr = kwargs["ptr"]
                self.did_i_create_it = False
            else:
                raise Exception


    def __del__(self):
        """
        Destructor: Frees the C structure
        """

        if self.did_i_create_it:
            __func = gdp.gdp_gclmd_free
            __func.argtypes = [POINTER(self.gdp_gclmd_t)]

            __func(self.gdp_gclmd_ptr)

        return


    def print_to_file(self, fh, detail):
        """
        Print the gdp_gclmd C memory location contents to a file handle fh.
            fh could be sys.stdout, or any other open file handle.
        Note: This just calls the corresponding C library function which
              handles the printing
        """

        # need to convert this file handle to a C FILE*
        __fh = PyFile_AsFile(fh)
        __func = gdp.gdp_gclmd_print
        __func.argtypes = [POINTER(self.gdp_gclmd_t), FILE_P, c_int]
        # ignore the return value

        __func(self.gdp_gclmd_ptr, __fh, detail)


    def add(self, gclmd_id, data):
        """
        Add a new entry to the metadata set
        """

        __func = gdp.gdp_gclmd_add
        __func.argtypes = [POINTER(self.gdp_gclmd_t), c_uint32, c_size_t, c_void_p]
        __func.restype = EP_STAT

        size = c_size_t(len(data))
        tmp_buf = create_string_buffer(data, len(data))        
        
        estat = __func(self.gdp_gclmd_ptr, c_uint32(gclmd_id), size, byref(tmp_buf))
        check_EP_STAT(estat)

        return


    def get(self, index):
        """
        Get a new entry from the metadata set by index
        """

        __func = gdp.gdp_gclmd_get
        __func.argtypes = [POINTER(self.gdp_gclmd_t), c_int, POINTER(c_uint32), 
                                POINTER(c_size_t), POINTER(c_void_p)]
        __func.restype = EP_STAT

        gclmd_id = c_uint32()
        dlen = c_size_t()
        data_ptr = c_void_p()
        _index = c_int(index)

        estat = __func(self.gdp_gclmd_ptr, _index, byref(gclmd_id), byref(dlen), byref(data_ptr))
        check_EP_STAT(estat)

        data = string_at(data_ptr, dlen.value)

        return (gclmd_id.value, data)


    def find(self, gclmd_id):
        """
        Get a new entry from the metadata set by id
        """

        __func = gdp.gdp_gclmd_find
        __func.argtypes = [POINTER(self.gdp_gclmd_t), c_uint32, 
                                POINTER(c_size_t), POINTER(c_void_p)]
        __func.restype = EP_STAT

        dlen = c_size_t()
        data_ptr = c_void_p()

        estat = __func(self.gdp_gclmd_ptr, c_uint32(gclmd_id), byref(dlen), byref(data_ptr))
        check_EP_STAT(estat)

        data = string_at(data_ptr, dlen.value)

        return data
