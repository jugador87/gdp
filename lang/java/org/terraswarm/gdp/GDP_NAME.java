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
 * A class to represent names inside GDP. This works for GCL's, log-servers
 * and any other entities. An entity can have up to three kind of names:
 * <ul>
 * <li> A 256 bit binary name </li>
 * <li> A base-64 (-ish) representation </li>
 * <li> (optional) A more memorable name, which is potentially hashed to
 * generate the 256-bit binary name </li>
 * </ul>
 * 
 * @author nitesh mor
 */

public class GDP_NAME {
    
    /**
     * Pass a string that could be either a human readable name, or a
     * printable name.
     * @param name A version of the name
     */
    public GDP_NAME(String name) {
        this.name = this.__parse_name(name);
        this.pname = this.__get_printable_name(this.name);
    }

    /**
     * Pass a binary string containing the internal name
     * @param name the internal name
     */
    public GDP_NAME(byte[] name) {
        assert name.length == 32;
        this.name = name;
        this.pname = this.__get_printable_name(name);
    }
    
    /**
     * Get the printable name
     * @return printable name as a byte array
     */
    public byte[] printable_name() {
        // we need to make a copy.
        return this.pname.clone();
    }

    /**
     * Get the internal 256-bit name
     * @return internal name as a byte array. length: 32 bytes
     */
    public byte[] internal_name() {
        // we need to make a copy
        return this.name.clone();
    }

    /**
     * Some sanity checking, borrowed from the C-api
     * @return whether this is a valid GDP name or not
     */
    public boolean is_valid() {
        ByteBuffer tmp = ByteBuffer.allocate(32);

        // copy to newly created bytebuffer
        tmp.put(this.name);

        byte ret = Gdp02Library.INSTANCE.gdp_name_is_valid(tmp);
        if (ret == 0x00) {
            return false;
        } else {
            return true;
        }
    }

    /////////////// PRIVATE MEMBERS ///////////////////////
    
    // to hold the 256-bit binary name
    private byte[] name = new byte[32];
    
    private static int PNAME_LEN = 43;
    
    // to hold the printable name
    private byte[] pname = new byte[this.PNAME_LEN + 1];
    
    /**
     * Just a heurestic to check if a string is binary or not
     * <p>
     * We don't need this anymore. Should we cleanup???
     * @param s the string under question
     * @return yes if it contains non-text characters
     */
    private boolean __is_binary(byte[] s) {

        // textchars = {7,8,9,10,12,13,27} | set(range(0x20, 0x100)) - {0x7f}
        for (byte b: s) {
            int x = (int) b; // typecast
            
            if (!( (x>=7 && x<=10) || x==12 || x==13 || x==27 || 
                    (x>=0x20 && x<0x100 && x!=0x7f) )) 
                return true;
        }
        return false;
    }
    
    /**
     * Parse the (presumably) printable name to return an internal name
     * @param s A potential printable name
     * @return A 32-byte long internal name
     */
    private byte[] __parse_name(String s) {
        ByteBuffer dst = ByteBuffer.allocate(32); // FIXME
        
        EP_STAT estat = Gdp02Library.INSTANCE.gdp_parse_name(s, dst);
        GDP.check_EP_STAT(estat);

        // we don't care if the bytebuffer gets modified. We aren't using
        // it elsewhere anyways.
        return dst.array();
    }
    
    /**
     * Generate printable name from internal name
     * @param internal_name
     * @return
     */
    
    private byte[] __get_printable_name(byte[] internal_name) {
        
        byte[] internal_name_copy = internal_name.clone();
        
        ByteBuffer internal = ByteBuffer.wrap(internal_name_copy);
        ByteBuffer printable = ByteBuffer.allocate(this.PNAME_LEN+1);
        
                
        Gdp02Library.INSTANCE.gdp_printable_name(internal, printable);
        // no problem, since we don't need bytebuffer interface to this
        // memory anymore.
        
        return printable.array();
    }
}
 
