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
import org.tanukisoftware.wrapper.WrapperListener;

/**
 * This is a very simple test of how a main class using Integration Method #3
 *  works.
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 */
public class SimpleWrapperListener
    implements WrapperListener
{
    /**************************************************************************
     * Constructors
     *************************************************************************/
    private SimpleWrapperListener()
    {
        System.out.println( "SimpleWrapperListener()" );
    }
    
    /**************************************************************************
     * WrapperListener Methods
     *************************************************************************/
    public Integer start( String[] args )
    {
        System.out.println( "SimpleWrapperListener.start()" );
        return null;
    }
    
    public int stop( int exitCode )
    {
        System.out.println( "SimpleWrapperListener.stop(" + exitCode + ")" );
        
        return exitCode;
    }
    
    public void controlEvent(int event) {
        System.out.println( "SimpleWrapperListener.controlEvent(" + event + ")" );
    }
    
    /**************************************************************************
     * Main Method
     *************************************************************************/
    public static void main( String[] args )
    {
        System.out.println( Main.getRes().getString( "SimpleWrapperListener.Initializing..." ) );
        
        System.out.println( Main.getRes().getString( "This test should simply 'start()' and 'stop()'." ) );
        
       // Start the application.  If the JVM was launched from the native
        //  Wrapper then the application will wait for the native Wrapper to
        //  call the application's start method.  Otherwise the start method
        //  will be called immediately.
        WrapperManager.start( new SimpleWrapperListener(), args );
    }
}

