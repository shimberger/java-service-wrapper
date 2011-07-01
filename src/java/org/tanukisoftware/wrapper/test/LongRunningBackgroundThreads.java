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

import org.tanukisoftware.wrapper.WrapperManager;

/**
 * Test case which launched several threads and lets them run for 30 seconds.
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 */
public class LongRunningBackgroundThreads
    implements Runnable
{
    private volatile int m_threadCount;
    
    /*---------------------------------------------------------------
     * Runnable Method
     *-------------------------------------------------------------*/
    public void run()
    {
        m_threadCount++;
        int loops = 0;

        while ( loops < 60 )
        {
            loops++;
            System.out.println( Main.getRes().getString( "{0} loop #{1}", new Object[]{ Thread.currentThread().getName(), new Integer( loops ) } ) );
            try
            {
                Thread.sleep(500);
            }
            catch ( InterruptedException e )
            {
            }
        }
        
        System.out.println(Main.getRes().getString( "{0} stopping", Thread.currentThread().getName() ) );
        m_threadCount--;
        if ( m_threadCount <= 0 )
        {
            System.out.println( Main.getRes().getString( "The JVM and then the wrapper should exit now.") );
        }
    }
    
    /*---------------------------------------------------------------
     * Main Method
     *-------------------------------------------------------------*/
    public static void main( String[] args )
    {
        System.out.println( Main.getRes().getString( "Long-running Background Threads Running..." ) );
        
        LongRunningBackgroundThreads app = new LongRunningBackgroundThreads();
        for ( int i = 0; i < 2; i++ )
        {
            Thread thread = new Thread( app, "App-Thread-" + i );
            thread.start();
        }
        
        System.out.println( Main.getRes().getString( "Running as a service: {0}", new Boolean( WrapperManager.isLaunchedAsService() ) ) );
        System.out.println( Main.getRes().getString( "Controlled by wrapper: {0}", new Boolean( WrapperManager.isControlledByNativeWrapper() ) ) );
        
        System.out.println( Main.getRes().getString( "Long-running Background Threads Main Done...") ) ;
    }
}
