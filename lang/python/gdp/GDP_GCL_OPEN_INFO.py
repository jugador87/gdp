#!/usr/bin/env python

from MISC import *
from EP_CRYPTO import *

class GDP_GCL_OPEN_INFO:

    """
    Information that is required to open a GCL -- for internal use only.
    The C equivalent exposed to python users is of a dictionary.

    """

    class gdp_gcl_open_info_t(Structure):
        pass


    def __init__(self, open_info={}):
        """
        Constructor: calls the C-function to allocate memory.
            Also sets up various things (such as private signature key)
            into this structure

        The (incomplete) list of the keys, their description, and the 
            associated functions for the dictionary open_info is:

        * skey: an instance of the class EP_CRYPTO_KEY.
                Calls C library's gdp_gcl_open_info_set_signing_key
        """

        # allocate memory for the C structure
        __func1 = gdp.gdp_gcl_open_info_new
        __func1.argtypes = []
        __func1.restype = POINTER(self.gdp_gcl_open_info_t)

        self.gdp_gcl_open_info_ptr = __func1()

        # read the open_info dictionary and set appropriate fields
        #   based on the key name.

        if 'skey' in open_info.keys():
            skey = open_info['skey']
            __func2 = gdp.gdp_gcl_open_info_set_signing_key
            __func2.argtypes = [POINTER(self.gdp_gcl_open_info_t),
                                    POINTER(EP_CRYPTO_KEY.EP_CRYPTO_KEY)]

            __func2.restype = EP_STAT

            estat = __func2(self.gdp_gcl_open_info_ptr, skey.key_ptr)
            check_EP_STAT(estat)


    def __del__(self):
        """
        Destructor: Frees the C structure
        """

        __func = gdp.gdp_gcl_open_info_free
        __func.argtypes = [POINTER(self.gdp_gcl_open_info_t)]

        __func(self.gdp_gcl_open_info_ptr)




