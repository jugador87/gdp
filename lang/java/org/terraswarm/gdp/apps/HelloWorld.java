package org.terraswarm.gdp.apps;

import org.terraswarm.gdp.*;
import java.util.Date;

public class HelloWorld {

    public static void main(String[] args) {

        GDP.dbg_set("*=10");

        String logname = "logjava";
        System.out.println("Creating object");
        GDP_GCL g = new GDP_GCL(logname, GDP_GCL.GDP_MODE.RA);

/*
        g.subscribe(0,3);
        for (;;) {
            String data = g.get_next_data(0);
            if (data == null) { break; }
            System.out.println(data);
        }

*/
        g.append(new Date().toString());
        System.out.println(g.read(-1));
        g.append(new Date().toString());
        System.out.println(g.read(-1));

    }

}
