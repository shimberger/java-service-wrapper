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
 * This is a simple test to see how the WrapperManager behaves when a main
 *  class designed for Integration Method #3 is called using Integration
 *  Method #2.
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 */
public class NestedWrapperListener
    implements WrapperListener
{
    /**************************************************************************
     * Constructors
     *************************************************************************/
    private NestedWrapperListener()
    {
        System.out.println( "NestedWrapperListener()" );
    }
    
    /**************************************************************************
     * WrapperListener Methods
     *************************************************************************/
    public Integer start( String[] args )
    {
        System.out.println( "NestedWrapperListener.start()" );
        return null;
    }
    
    public int stop( int exitCode )
    {
        System.out.println( "NestedWrapperListener.stop(" + exitCode + ")" );
        
        return exitCode;
    }
    
    public void controlEvent(int event) {
        System.out.println( "NestedWrapperListener.controlEvent(" + event + ")" );
    }
    
    /**************************************************************************
     * Main Method
     *************************************************************************/
    public static void main( String[] args )
    {
        System.out.println( Main.getRes().getString( "NestedWrapperListener.Initializing..." ) );
        
        System.out.println( Main.getRes().getString("An error saying that the WrapperManager has already been started should be displayed and the JVM will exit." ));
        
        // Start the application.  If the JVM was launched from the native
        //  Wrapper then the application will wait for the native Wrapper to
        //  call the application's start method.  Otherwise the start method
        //  will be called immediately.
        WrapperManager.start( new NestedWrapperListener(), args );
    }
}

