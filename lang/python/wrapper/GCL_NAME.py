#!/usr/bin/python
 
from MISC import *
 
class GCL_NAME: 
    """
    Represents name of a GCL. Each GCL has potentially up to three kind of names:
        - a 256 bit binary name
        - a Base-64(ish) representation
        - (optional) a more memorable name, which potentially is hashed to get the
          binary name
    """ 

    # name_t, pname_t are ctypes types corresponding to gcl_name_t, gcl_pname_t 

    # Types that will be used for some type-checking #  
    name_t = c_uint8*32     # internal name of a GCL 
    PNAME_LEN = 43          # this is following the convention in gdp.h 
    pname_t = c_uint8*(PNAME_LEN+1)     # printable name of a GCL 
 
    def __init__(self,name): 
        """ 
        takes either an internal name, or a printable name, or a human friendly 
            name, and creates a python object that can be passed around for the 
            various GDP calls 
        """ 
 
        def __get_printable_name(internal_name): 
  
            # ctypes magic to create an array representing gcl_name_t 
            buf1 = create_string_buffer(internal_name,32) 
            name_t_ptr = cast(byref(buf1), POINTER(self.name_t)) 
 
            # ctypes magic to create an array representing gcl_pname_t 
            buf2 = create_string_buffer(self.PNAME_LEN+1) 
            pname_t_ptr = cast(byref(buf2), POINTER(self.pname_t)) 
 
            # for strict type checking 
            __func = gdp.gdp_gcl_printable_name 
            __func.argtypes = [self.name_t, self.pname_t] 
            __func.restype = c_char_p 
  
            res = __func(name_t_ptr.contents, pname_t_ptr.contents) 
            # the return of __func is just a pointer to the printable_name 
            return string_at(pname_t_ptr.contents,self.PNAME_LEN+1) 
             
        def __parse_name(name): 
 
            buf1 = create_string_buffer(name, len(name)) 
            buf2 = create_string_buffer(32) 
            name_t_ptr = cast(byref(buf2), POINTER(self.name_t)) 
 
            __func = gdp.gdp_gcl_parse_name 
            __func.argtypes = [c_char_p, self.name_t] 
            __func.restype = EP_STAT 
 
            estat = __func(buf1, name_t_ptr.contents) 
            check_EP_STAT(estat)
            return string_at(name_t_ptr.contents, 32) 
 
 
        if len(name)==32: 
            # If the length of the name is exactly 32, we treat it as a gcl_name_t 
            #   This is in accordance with the libgdp 
 
            self.name = name                                # Python string 
            self.pname = __get_printable_name(self.name)    # Python string 
 
        else: 
            # we invoke parse_name, which gives us the internal name. 
            self.name = __parse_name(name) 
            self.pname = __get_printable_name(self.name) 
 
 
    def is_zero(self): 
        "Borrowed from the GDP C api, tells if the name is ZERO or not"

 
        # We could potentially just return it by reimplementing the logic in  
        #   gdp_gcl_name_is_zero, but then we could theoretically reimplement 
        #   everything in python too. 
 
        # ctypes magic to create an array representing gcl_name_t 
        buf1 = create_string_buffer(self.name,32) 
        name_t_ptr = cast(byref(buf1), POINTER(self.name_t)) 
 
        __func = gdp.gdp_gcl_name_is_zero 
        __func.argtypes = [self.name_t] 
        __func.restype = c_bool 
 
        return __func(name_t_ptr.contents) 
 
    def printable_name(self): 
        "Returns a python string that represents the printable name"
        return self.pname                                   # Python string 
 
 
    def internal_name(self): 
        "Returns a python string that represents the internal binray name"
        return self.name                                    # Python string 


