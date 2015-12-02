/*
 * Heavily based on Christopher's GdpUtilities, 
 * apps/ReaderTest and apps/WriterTest
 */

package org.terraswarm.gdp; 
import java.nio.ByteBuffer;
import java.util.Date;

import com.sun.jna.Native;
import com.sun.jna.Memory;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;
import org.terraswarm.gdp.NativeSize; // Fixed by cxh in makefile.


public class GCL {

    // mimics object-oriented Python wrapper around C library


    // private variables

    // internal 256 bit name
    private ByteBuffer gclname = ByteBuffer.allocate(32);

    // a pointer to pointer to open handle
    private Pointer gclh = null;

    // To store the i/o mode for this log
    private GDP_MODE iomode;



    //////////////// Public Interface /////////////////////////

    /** Set the debug levels for the EP/GDP C library
      * @param debug_level A string of the form X=Y, Y is an integer.
      */
    public static void dbg_set(String debug_level) {

        Gdp02Library.INSTANCE.ep_dbg_set(debug_level);
    }


    /* I/O mode for a log */
    public enum GDP_MODE {
        ANY, RO, AO, RA
    }

    /** Initialize a new GCL object.
      * @param name Name of the log. Could be human readable or binary
      * @param iomode Should this be opened read only, read-append, append-only
      * No signatures yet. TODO
      */
      
    public GCL(String name, GDP_MODE iomode) {

        EP_STAT estat;

        this.iomode = iomode;

        // initialize gdp by calling gdp_init
        estat = Gdp02Library.INSTANCE.gdp_init((Pointer)null);
        check_EP_STAT(estat);

        // convert human name to internal 256 bit name
        estat = Gdp02Library.INSTANCE.gdp_parse_name(name, this.gclname);
        check_EP_STAT(estat);

        // open the GCL
        PointerByReference gclhByReference = new PointerByReference();
        estat = Gdp02Library.INSTANCE.gdp_gcl_open(this.gclname, 
                            iomode.ordinal(), (PointerByReference) null, 
                            gclhByReference);
        check_EP_STAT(estat);

        // Anything else?
        this.gclh = gclhByReference.getValue();
        assert this.gclh != null;

    }


    public String read(long recno) {
        // only returns the data in the record as a string
        // ignores record number, timestamp and anything else 

        EP_STAT estat;
        PointerByReference datum = Gdp02Library.INSTANCE.gdp_datum_new();

        // get the datum populated/
        estat = Gdp02Library.INSTANCE.gdp_gcl_read(this.gclh, recno, datum);
        check_EP_STAT(estat);

        // read the data in the datum. Ignore recno, timestamp for now
        GDPDatum d = new GDPDatum(datum);

        // free the corresponding C structure            
        Gdp02Library.INSTANCE.gdp_datum_free(datum);
        return d.data;
    }


    public void append(String data) {

        EP_STAT estat;
        PointerByReference datum = Gdp02Library.INSTANCE.gdp_datum_new();

        // Create a C string that is a copy (?) of the Java String.
        Memory memory = new Memory(data.length()+1);
        // FIXME: not sure about alignment.
        Memory alignedMemory = memory.align(4);
        memory.clear();
        Pointer pointer = alignedMemory.share(0);
        pointer.setString(0, data);

        // Now feed this data into the gdp buffer
        PointerByReference dbuf = Gdp02Library.INSTANCE.gdp_datum_getbuf(datum);
        Gdp02Library.INSTANCE.gdp_buf_write(dbuf, pointer, 
                                new NativeSize(data.length()));

        estat = Gdp02Library.INSTANCE.gdp_gcl_append(this.gclh, datum);
        check_EP_STAT(estat);

        Gdp02Library.INSTANCE.gdp_datum_free(datum);

        return;
    }


    public void subscribe(long firstrec, int numrecs) {

        EP_STAT estat;
        estat = Gdp02Library.INSTANCE
                    .gdp_gcl_subscribe(this.gclh, firstrec, numrecs,
                                        null, null, null);

        check_EP_STAT(estat);

        return;
    }

    public void multiread(long firstrec, int numrecs) {

        EP_STAT estat;
        estat = Gdp02Library.INSTANCE
                    .gdp_gcl_multiread(this.gclh, firstrec, numrecs,
                                        null, null);

        check_EP_STAT(estat);

        return;
    }

    public String get_next_data(int timeout_msec) {
        // returns next data item from subcription or multiread
        // returns null if end of subscription

        GDPEvent ev = get_next_event(timeout_msec);
        if (ev.type == Gdp02Library.INSTANCE.GDP_EVENT_DATA) {
            return ev.datum.data;
        } else {
            return null;
        }

    }


    private GDPEvent get_next_event(int timeout_msec) {
        // timeout in ms. Probably not very precise
        // 0 means block forever

        EP_TIME_SPEC timeout = null;

        if (timeout_msec != 0) {

            // Get current time
            Date d = new Date();
            long current_msec = d.getTime();

            // Get values to create a C timeout structure
            long tv_sec = (current_msec + timeout_msec)/1000;
            int tv_nsec = (int) ((current_msec + timeout_msec)%1000);
            float tv_accuracy = (float) 0.0;

            timeout = new EP_TIME_SPEC(tv_sec, tv_nsec, tv_accuracy);
        }

        // Get the C pointer to next event. This blocks till timeout
        PointerByReference gev = Gdp02Library.INSTANCE
                                    .gdp_event_next(this.gclh, timeout);

        // Get the data associated with this event
        int type = Gdp02Library.INSTANCE.gdp_event_gettype(gev);
        PointerByReference datum = Gdp02Library.INSTANCE.gdp_event_getdatum(gev);

        // Create an object of type GDPEvent that we'll return
        GDPEvent ev = new GDPEvent(this.gclh, datum, type);

        // Free the C data-structure
        Gdp02Library.INSTANCE.gdp_event_free(gev);

        return ev;
    }

    private static void check_EP_STAT(EP_STAT estat) {

        /* 

        if (!GdpUtilities.EP_STAT_ISOK(estat)) {
            System.err.println("Exiting with status " + estat 
                    + ", code: " + estat.code);

            System.exit(1);
        } */
    }

}


class GDPEvent {

    public Pointer gcl_handle;
    public GDPDatum datum;
    public int type;

    public GDPEvent(Pointer gcl_handle, PointerByReference datum, int type) {
        this.gcl_handle = gcl_handle;
        if (type != Gdp02Library.INSTANCE.GDP_EVENT_DATA) {
            assert datum == null;
            this.datum = null;
        } else {
            this.datum = new GDPDatum(datum);
        }
        this.type = type;
    }

    public GDPEvent(Pointer gcl_handle, GDPDatum datum, int type) {
        this.gcl_handle = gcl_handle;
        this.datum = datum;
        this.type = type;
    }

}

class GDPDatum {

    public long recno;
    public String data;
    public EP_TIME_SPEC ts;

    public GDPDatum(long recno, String data, EP_TIME_SPEC ts) {
        this.recno = recno;
        this.data = data;
        this.ts = ts;
    }


    public GDPDatum(PointerByReference d) {
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
