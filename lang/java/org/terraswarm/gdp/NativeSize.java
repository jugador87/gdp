// Based on JNAerator: https://github.com/nativelibs4java/JNAerator/
// /jnaerator-runtime/src/main/java/NativeSize.java

/*
   Copyright (c) 2009 Olivier Chafik, All Rights Reserved
   
   This file is part of JNAerator (http://jnaerator.googlecode.com/).
   
   JNAerator is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   JNAerator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with JNAerator.  If not, see <http://www.gnu.org/licenses/>.
*/

package org.terraswarm.gdp;

import com.sun.jna.IntegerType;
import com.sun.jna.Native;

/** A Java representation for a C type size_t, which is 32 or 64 bits.
 * @version $Id: NativeSizeT.java 72874 2015-07-26 21:01:52Z cxh $
 * @since Ptolemy II 10.0
 * @version $Id: NativeSizeT.java 72874 2015-07-26 21:01:52Z cxh $
 * @Pt.ProposedRating Red (cxh)
 * @Pt.AcceptedRating Red (cxh)
 */
@SuppressWarnings("serial")
public class NativeSize extends IntegerType {
    /** Construct a zero-sized object. */
    public NativeSize() {
        this(0);
    }

    /** Construct size_t object with the given value.
     *  The size is the size of the C size_t type,
     *  which is typically 32 or 64 bytes.
     *  @param value The given value.
     */
    public NativeSize(long value) {
        super(Native.SIZE_T_SIZE, value);
    }

    /** Size of an integer. */
    public static int SIZE = Native.SIZE_T_SIZE;
}
