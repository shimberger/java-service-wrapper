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
 */

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Date;

/**
 *
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 */
public class LoadedLogOutput {
    /*---------------------------------------------------------------
     * Main Method
     *-------------------------------------------------------------*/
    public static void main(String[] args) {
        DateFormat df = new SimpleDateFormat( "yyyyMMdd'T'HHmmssSSS" );

        /*
        // Let the JVM inform the Wrapper that it is started.
        try
        {
            Thread.sleep( 1500 );
        }
        catch ( InterruptedException e )
        {
        }
        */
        
        long start = System.currentTimeMillis();
        long now = start;
        
        System.out.println( Main.getRes().getString( "Log as much as possible for 60 seconds..." ) );
        int line = 1;
        while ( now - start < 60000 ) {
            System.out.println( ( line++ ) + " : " + ( now - start ) + " : " + df.format( new Date( now ) ) + " : ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz1234567890" );
            now = System.currentTimeMillis();
        }
        System.out.println( Main.getRes().getString( "All done.") );
    }
}

