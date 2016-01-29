package org.terraswarm.gdp; 
import java.nio.ByteBuffer;
import java.nio.IntBuffer;
import java.util.Date;
import java.util.Map;
import java.util.HashMap;
import java.awt.PointerInfo;
import java.lang.Exception;


import com.sun.jna.Native;
import com.sun.jna.Memory;
import com.sun.jna.Pointer;
import com.sun.jna.ptr.PointerByReference;
import org.terraswarm.gdp.NativeSize; // Fixed by cxh in makefile.

/**
 * GCL: GDP Channel Log
 * A class that represents a log. The name GCL is for historical reasons.
 * This mimics the Python wrapper around the GDP C-library. However, be
 * aware that this Java wrapper may not make the latest features available.
 * 
 * @author Nitesh Mor, based on Christopher Brooks' early implementation.
 *
 */
public class GDP_GCL {

    // internal 256 bit name
    private ByteBuffer gclname = ByteBuffer.allocate(32);

    // a pointer to pointer to open handle
    private Pointer gclh = null;

    // To store the i/o mode for this log
    private GDP_MODE iomode;



    //////////////// Public Interface /////////////////////////

    /**
     * Create a GCL. Note that this is a static method. 
     * 
     * @param logName   Name of the log to be created
     * @param logdName  Name of the log server where this should be 
     *                  placed.
     * @param metadata  Metadata to be added to the log. 
     */
    
    public static void create(GDP_NAME logName, GDP_NAME logdName, 
            Map<Integer, String> metadata) {
        
        EP_STAT estat;
        
        // Get the 256-bit internal names for logd and log
        ByteBuffer logdNameInternal = ByteBuffer.wrap(logdName.internal_name());
        ByteBuffer logNameInternal = ByteBuffer.wrap(logName.internal_name());
        
        // Just create a throwaway pointer.
        Pointer tmpPtr = null;
        
        // metadata processing
        
        GDP_GCLMD m = new GDP_GCLMD();
        for (int k: metadata.keySet()) {
            m.add(k, metadata.get(k));
        }
        
        estat = Gdp02Library.INSTANCE.gdp_gcl_create(logNameInternal, logdNameInternal,
                        m.gdp_gclmd_ptr, new PointerByReference(tmpPtr));
        
        GDP.check_EP_STAT(estat);
        
        return;
    }
    

    /**
     * Create a GCL. Note that this is a static method    
     * @param logName   Name of the log
     * @param logdName  Name of the logserver that should host this log
     */
    public static void create(GDP_NAME logName, GDP_NAME logdName) {
        create (logdName, logName, new HashMap<Integer, String>());
    }

    /**
     * I/O mode for a log.
     * <ul>
     * <li> ANY: for internal use only </li>
     * <li> RO : Read only </li>
     * <li> AO : Append only </li>
     * <li> RA : Read + append </li>
     * </ul> 
     */
    public enum GDP_MODE {
        ANY, RO, AO, RA
    }

    /** 
     * Initialize a new GCL object.
     * No signatures support, yet. TODO
     * 
     * @param name   Name of the log. Could be human readable or binary
     * @param iomode Should this be opened read only, read-append, append-only
     */
      
    public GDP_GCL(String name, GDP_MODE iomode) {

        EP_STAT estat;

        this.iomode = iomode;

        // initialize gdp by calling gdp_init
        estat = Gdp02Library.INSTANCE.gdp_init((Pointer)null);
        GDP.check_EP_STAT(estat);

        //Gdp02Library.INSTANCE.ep_dbg_set("*=80");
        
        // convert human name to internal 256 bit name
        estat = Gdp02Library.INSTANCE.gdp_parse_name(name, this.gclname);
        GDP.check_EP_STAT(estat);

        // open the GCL
        PointerByReference gclhByReference = new PointerByReference();
        estat = Gdp02Library.INSTANCE.gdp_gcl_open(this.gclname, 
                            iomode.ordinal(), (PointerByReference) null, 
                            gclhByReference);
        GDP.check_EP_STAT(estat);

        // Anything else?
        this.gclh = gclhByReference.getValue();
        assert this.gclh != null;

    }


    /**
     * Read a record by record number
     * 
     * @param recno Record number to be read
     * @return  The data from the datum returned.
     */
    
    public String read(long recno) {
        // only returns the data in the record as a string
        // ignores record number, timestamp and anything else 

        EP_STAT estat;
        PointerByReference datum = Gdp02Library.INSTANCE.gdp_datum_new();

        // get the datum populated
        estat = Gdp02Library.INSTANCE.gdp_gcl_read(this.gclh, recno, datum);
        GDP.check_EP_STAT(estat);

        // read the data in the datum. Ignore recno, timestamp for now
        GDP_DATUM d = new GDP_DATUM(datum);

        // free the corresponding C structure            
        Gdp02Library.INSTANCE.gdp_datum_free(datum);
        return d.data;
    }


    /**
     * Append data to a log
     * 
     * @param data  Data to be appended
     */
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
        GDP.check_EP_STAT(estat);

        Gdp02Library.INSTANCE.gdp_datum_free(datum);

        return;
    }


    /** 
     * Start a subscription to a log
     * See the documentation in C library for examples.
     * 
     * @param firstrec  The record num to start the subscription from
     * @param numrecs   Max number of records to be returned. 0 => infinite
     */
    public void subscribe(long firstrec, int numrecs) {

        EP_STAT estat;
        estat = Gdp02Library.INSTANCE
                    .gdp_gcl_subscribe(this.gclh, firstrec, numrecs,
                                        null, null, null);

        GDP.check_EP_STAT(estat);

        return;
    }

    /** 
     * Multiread: Similar to subscription, but for existing data
     * only. Not for any future records
     * See the documentation in C library for examples.
     * 
     * @param firstrec  The record num to start reading from
     * @param numrecs   Max number of records to be returned. 
     */
    public void multiread(long firstrec, int numrecs) {

        EP_STAT estat;
        estat = Gdp02Library.INSTANCE
                    .gdp_gcl_multiread(this.gclh, firstrec, numrecs,
                                        null, null);

        GDP.check_EP_STAT(estat);

        return;
    }

    /** 
     * Get data from next record for a subscription or multiread
     * This is a wrapper around 'get_next_event', and works only 
     * for subscriptions, multireads.
     * 
     * @param timeout_msec  Time (in ms) for which to block. Can be used 
     *                      to block eternally as well.
     */
    public String get_next_data(int timeout_msec) {
        // returns next data item from subcription or multiread
        // returns null if end of subscription

        GDP_EVENT ev = get_next_event(timeout_msec);
        if (ev.type == Gdp02Library.INSTANCE.GDP_EVENT_DATA) {
            return ev.datum.data;
        } else {
            return null;
        }

    }


    /**
     * Get next event as a result of subscription, multiread or
     * an asynchronous operation.
     * 
     * @param timeout_msec  Time (in ms) for which to block.
     */
    public GDP_EVENT get_next_event(int timeout_msec) {
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

        // Create an object of type GDP_EVENT that we'll return
        GDP_EVENT ev = new GDP_EVENT(this.gclh, datum, type);

        // Free the C data-structure
        Gdp02Library.INSTANCE.gdp_event_free(gev);

        return ev;
    }
}
