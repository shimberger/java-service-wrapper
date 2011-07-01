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

/**
 *
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 */
public class JarMain
{
    static
    {
        if ( System.getProperty( "JarMain.init.fail" ) != null )
        {
            System.out.println( Main.getRes().getString( "About to throw exception in initializer..." ) );
            throw new IllegalStateException( Main.getRes().getString( "This is an intentional error in the initializer." ) );
        }
    }
    
    /*---------------------------------------------------------------
     * Main Method
     *-------------------------------------------------------------*/
    public static void main(String[] args)
    {
        if ( args.length > 0 )
        {
            System.out.println( Main.getRes().getString( "Arguments:" ) );
            for ( int i = 0; i < args.length; i++ )
            {
                System.out.println( "  args[" + i + "]=" + args[i] );
            }
        }
        
        System.out.println( Main.getRes().getString( "Loop for 10 seconds." ) );
        
        for ( int i = 0; i < 10; i++ )
        {
            try
            {
                Thread.sleep(1000);
            }
            catch ( InterruptedException e )
            {
            }
            System.out.println( Main.getRes().getString( "Counting...{0}", new Integer( i ) ) );
        }
        
        System.out.println( Main.getRes().getString( "Loop complete." ) );
    }
}

