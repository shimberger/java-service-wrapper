package org.tanukisoftware.wrapper.test;

/*
 * Copyright (c) 1999, 2011 Tanuki Software, Ltd.
 * http://www.tanukisoftware.com
 * All rights reserved.
 *
 * This software is the proprietary information of Tanuki Software.
 * You shall use it only in accordance with the terms of the
 * license agreement you entered into with Tanuki Software.
 * http://wrapper.tanukisoftware.com/doc/english/licenseOverview.html
 * 
 * 
 * Portions of the Software have been derived from source code
 * developed by Silver Egg Technology under the following license:
 * 
 * Copyright (c) 2001 Silver Egg Technology
 * 
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without 
 * restriction, including without limitation the rights to use, 
 * copy, modify, merge, publish, distribute, sub-license, and/or 
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following 
 * conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 */

import java.io.FileWriter;
import java.io.IOException;

/**
 *
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 */
public class OutputLoader {
    /*---------------------------------------------------------------
     * Main Method
     *-------------------------------------------------------------*/
    public static void main(String[] args) {
        System.out.println( Main.getRes().getString( "Start outputting lots of data.") );
        
        long start = System.currentTimeMillis();
        int count = 0;
        while ((System.currentTimeMillis()) < start + 20000) {
            System.out.println( Main.getRes().getString( "Testing line Out #{0}", new Integer( ++count ) ) );
            System.err.println( Main.getRes().getString( "Testing line Err #{0}", new Integer( ++count ) ) );
        }
        
        System.out.println( Main.getRes().getString( "Printed {0} lines of output in 20 seconds", new Integer( count ) ) );
        
        // Write the output to a file as well, so we can see the results
        //  when output is disabled.
        try {
            FileWriter fw = new FileWriter("OutputLoader.log", true);
            fw.write( Main.getRes().getString( "Printed {0} lines of output in 20 seconds", new Integer( count ) ) );
            fw.close();
        } catch (IOException e) {
            e.printStackTrace();
        }
    }
}

