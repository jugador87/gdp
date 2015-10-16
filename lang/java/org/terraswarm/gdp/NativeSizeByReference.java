// https://github.com/nativelibs4java/JNAerator/blob/master/jnaerator-runtime/src/main/java/com/ochafik/lang/jnaerator/runtime/NativeSizeByReference.java

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

import com.sun.jna.ptr.ByReference;

public class NativeSizeByReference extends ByReference {
    public NativeSizeByReference() {
        this(new NativeSize(0));
    }

    public NativeSizeByReference(NativeSize value) {
        super(NativeSize.SIZE);
        setValue(value);
    }

    public void setValue(NativeSize value) {
        if (NativeSize.SIZE == 4) {
            getPointer().setInt(0, value.intValue());
        } else if (NativeSize.SIZE == 8) {
            getPointer().setLong(0, value.longValue());
        } else {
            throw new RuntimeException("GCCLong has to be either 4 or 8 bytes.");
        }
    }

    public NativeSize getValue() {
        if (NativeSize.SIZE == 4) {
            return new NativeSize(getPointer().getInt(0));
        } else if (NativeSize.SIZE == 8) {
            return new NativeSize(getPointer().getLong(0));
        } else {
            throw new RuntimeException("GCCLong has to be either 4 or 8 bytes.");
        }
    }
}
