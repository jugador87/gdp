package org.terraswarm.gdp; 
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.Date;
import java.util.Map;
import java.awt.PointerInfo;
import java.lang.Exception;


import com.sun.jna.Native;
import com.sun.jna.Memory;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;
import org.terraswarm.gdp.NativeSize; // Fixed by cxh in makefile.

/**
 * A class representing the GDP Event. 
 * <p>
 * TODO: implement destructor. Make sure there are no memory leaks.
 * 
 * @author nitesh mor
 */

class GDP_EVENT {

    public Pointer gcl_handle;
    public GDP_DATUM datum;
    public int type;

    public GDP_EVENT(Pointer gcl_handle, PointerByReference datum, int type) {
        this.gcl_handle = gcl_handle;
        if (type != Gdp02Library.INSTANCE.GDP_EVENT_DATA) {
            assert datum == null;
            this.datum = null;
        } else {
            this.datum = new GDP_DATUM(datum);
        }
        this.type = type;
    }

    public GDP_EVENT(Pointer gcl_handle, GDP_DATUM datum, int type) {
        this.gcl_handle = gcl_handle;
        this.datum = datum;
        this.type = type;
    }

}


