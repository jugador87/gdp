package org.terraswarm.gdp.apps;

import org.terraswarm.gdp.*;
import java.util.Date;
import java.util.HashMap;

public class HelloWorld {

    public static void main(String[] args) {

        GDP.gdp_init();
        GDP.dbg_set("*=10");

        String logname = "edu.berkeley.eecs.mor.20160131_182143";
        GDP_NAME gn = new GDP_NAME(logname);
        System.out.println("Creating object");
        GDP_GCL g = new GDP_GCL(gn, GDP_GCL.GDP_MODE.RA);

/*
        g.subscribe(0,3);
        for (;;) {
            String data = g.get_next_data(0);
            if (data == null) { break; }
            System.out.println(data);
        }

*/
        /*
        g.append(new Date().toString());
        System.out.println(g.read(-1));
        g.append(new Date().toString());
        System.out.println(g.read(-1));
        */
        
        g.append((new Date().toString()).getBytes());
        
        HashMap<String, Object> returnDatum = g.read(-1);
        byte[] data = (byte[]) returnDatum.get("data");
        for (int i=0; i<data.length; i++) {
            System.out.print((char) data[i]);
        }

    }

}
