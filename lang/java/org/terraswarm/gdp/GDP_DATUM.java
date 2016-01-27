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
 * A class that represents a GDP Datum. The original C GDP Datum
 * may create confusion since data is stored in a buffer (which
 * unsurprisingly acts like a buffer).
 * <p>
 * This is for internal use only, for users the GDP DATUM is more
 * like a dictionary.
 * <p>
 * TODO Make sure there are no memory leaks. Implement a destructor.
 * @author nitesh mor
 *
 */

class GDP_DATUM {

    public long recno;
    public String data;
    public EP_TIME_SPEC ts;

    public GDP_DATUM(long recno, String data, EP_TIME_SPEC ts) {
        this.recno = recno;
        this.data = data;
        this.ts = ts;
    }


    public GDP_DATUM(PointerByReference d) {
        // this is when we have just a C pointer.

        // based on Christopher's gdp_datum_print
        long recno = Gdp02Library.INSTANCE.gdp_datum_getrecno(d);
        PointerByReference buf = Gdp02Library.INSTANCE.gdp_datum_getbuf(d);
        NativeSize len = Gdp02Library.INSTANCE.gdp_buf_getlength(buf);
        Pointer bufptr = Gdp02Library.INSTANCE.gdp_buf_getptr(buf, len);
        
        // Now read len bytes from bp
        byte[] bytes = bufptr.getByteArray(0, len.intValue());
        data = new String(bytes);

        EP_TIME_SPEC ts = new EP_TIME_SPEC();
        Gdp02Library.INSTANCE.gdp_datum_getts(d, ts);

        this.recno = recno;
        this.data = data;
        this.ts = ts;
    }
}
