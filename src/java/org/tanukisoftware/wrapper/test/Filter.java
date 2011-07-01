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

import org.tanukisoftware.wrapper.WrapperManager;

/**
 *
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 */
public class Filter {
    /*---------------------------------------------------------------
     * Main Method
     *-------------------------------------------------------------*/
    public static void main(String[] args) {
        System.out.println( Main.getRes().getString( "Test the handling of filters." ) );
        System.out.println( Main.getRes().getString( "The Wrapper should restart the JVM when it detects either the string:" ) );
        System.out.println( Main.getRes().getString( "  \"ERR OR\" or \"N ice long restart message.\", both without the" ) );
        System.out.println( Main.getRes().getString( "  extra space.  It should ignore the string: \"NONERROR\".  Then" ) );
        System.out.println( Main.getRes().getString( "  it should exit when it detects the string: \"ALL DONE\", once again" ) );
        System.out.println( Main.getRes().getString( "  without the space." ) );
        System.out.println();
        
        System.out.println( Main.getRes().getString( "The next line should be ignored:" ) );
        System.out.println( "  NONERROR");
        System.out.println();
        
        if (WrapperManager.getJVMId() >= 6) {
            // Time to shutdown
            System.out.println( Main.getRes().getString( "The next line should cause the Wrapper to exit:" ) );
            System.out.println("  ALLDONE");
        } else if (WrapperManager.getJVMId() == 5) {
            // Perform a restart.
            System.out.println( Main.getRes().getString( "The next line should cause the Wrapper to restart the JVM:" ) );
            System.out.println( "  HEAD and a bunch of stuff before the TAIL" );
        } else if (WrapperManager.getJVMId() == 4) {
            // Perform a thread dump and restart.
            System.out.println( Main.getRes().getString( "The next line should cause the Wrapper to invoke a thread dump and then restart the JVM:" ) );
            System.out.println( "  DUMP -n- RESTART" );
        } else if (WrapperManager.getJVMId() == 3) {
            // Try a restart with spaces.
            System.out.println( Main.getRes().getString( "The next line should cause the Wrapper to restart the JVM:" ) );
            System.out.println( "  Nice long restart message." );
        } else {
            System.out.println( Main.getRes().getString( "The next line should cause the Wrapper to restart the JVM:" ) );
            System.out.println("  ERROR");
        }
        System.out.println();
        System.out.println( Main.getRes().getString( "The above message should be caught before this line, but this line" ) );
        System.out.println( Main.getRes().getString( "  will still be visible.  Wait for 5 seconds before this thread is" ) );
        System.out.println( Main.getRes().getString( "  allowed to complete.  This prevents the Wrapper from detecting" ) );
        System.out.println( Main.getRes().getString( "  that the application has completed and exiting normally.  The" ) );
        System.out.println( Main.getRes().getString( "  Wrapper will try to shutdown the JVM cleanly, so it will not exit" ) );
        System.out.println( Main.getRes().getString( "  until this thread has completed." ) );

        try {
            Thread.sleep(5000);
        } catch (InterruptedException e) {
        }

        System.out.println( Main.getRes().getString( "Main complete." ) );
    }
}

