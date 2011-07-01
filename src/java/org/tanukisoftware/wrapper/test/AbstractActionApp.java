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

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;
import java.lang.reflect.InvocationTargetException;
import java.lang.reflect.Method;
import java.util.Enumeration;
import java.util.Properties;

import org.tanukisoftware.wrapper.WrapperManager;
import org.tanukisoftware.wrapper.WrapperProcess;
import org.tanukisoftware.wrapper.WrapperProcessConfig;
import org.tanukisoftware.wrapper.WrapperServiceException;
import org.tanukisoftware.wrapper.WrapperWin32Service;
import org.tanukisoftware.wrapper.event.WrapperControlEvent;
import org.tanukisoftware.wrapper.event.WrapperEvent;
import org.tanukisoftware.wrapper.event.WrapperEventListener;

/**
 * @author Leif Mortenson <leif@tanukisoftware.com>
 */
public abstract class AbstractActionApp
    implements WrapperEventListener
{
    private DeadlockPrintStream m_out;
    private DeadlockPrintStream m_err;
    
    private Thread m_runner;
    private Thread m_consoleRunner;
    
    private boolean m_ignoreControlEvents;
    private boolean m_users;
    private boolean m_groups;
    
    private boolean m_nestedExit;
    
    private long m_eventMask = 0xffffffffffffffffL;
    private String m_serviceName = "testWrapper";
    private String m_consoleTitle = "Java Service Wrapper";
    private String m_childCommand = "ls";
    private boolean m_childDetached = true;
    
    /*---------------------------------------------------------------
     * Constructors
     *-------------------------------------------------------------*/
    protected AbstractActionApp() {
        m_runner = new Thread( Main.getRes().getString( "WrapperActionTest_Runner" ) )
        {
            public void run()
            {
                while ( true )
                {
                    if ( m_users )
                    {
                        System.out.println( Main.getRes().getString( "The current user is: {0}", WrapperManager.getUser( m_groups ) ) );
                        System.out.println( Main.getRes().getString( "The current interactive user is: {0}" , WrapperManager.getInteractiveUser( m_groups ) ) );
                    }
                    synchronized( AbstractActionApp.class )
                    {
                        try
                        {
                            AbstractActionApp.class.wait( 5000 );
                        }
                        catch ( InterruptedException e )
                        {
                        }
                    }
                }
            }
        };
        m_runner.setDaemon( true );
        m_runner.start();
    }
    
    /*---------------------------------------------------------------
     * WrapperEventListener Methods
     *-------------------------------------------------------------*/
    /**
     * Called whenever a WrapperEvent is fired.  The exact set of events that a
     *  listener will receive will depend on the mask supplied when
     *  WrapperManager.addWrapperEventListener was called to register the
     *  listener.
     *
     * Listener implementations should never assume that they will only receive
     *  events of a particular type.   To assure that events added to future
     *  versions of the Wrapper do not cause problems with user code, events
     *  should always be tested with "if ( event instanceof {EventClass} )"
     *  before casting it to a specific event type.
     *
     * @param event WrapperEvent which was fired.
     */
    public void fired( WrapperEvent event )
    {
        System.out.println( Main.getRes().getString( "Received event: {0}", event ) );
        if ( event instanceof WrapperControlEvent )
        {
            System.out.println( Main.getRes().getString( "  Consume and ignore." ) );
            ((WrapperControlEvent)event).consume();
        }
    }
    
    /*---------------------------------------------------------------
     * Methods
     *-------------------------------------------------------------*/
    protected boolean ignoreControlEvents()
    {
        return m_ignoreControlEvents;
    }
    
    protected boolean isNestedExit()
    {
        return m_nestedExit;
    }
    
    protected void setEventMask( long eventMask )
    {
        m_eventMask = eventMask;
    }
    
    protected void setServiceName( String serviceName )
    {
        m_serviceName = serviceName;
    }
    
    protected void setConsoleTitle( String consoleTitle )
    {
        m_consoleTitle = consoleTitle;
    }
    
    protected void setChildParams( String childCommand, boolean childDetached )
    {
        m_childCommand = childCommand;
        m_childDetached = childDetached;
    }
    
    protected void prepareSystemOutErr()
    {
        m_out = new DeadlockPrintStream( System.out );
        System.setOut( m_out );
        m_err = new DeadlockPrintStream( System.err );
        System.setErr( m_err );
    }
    protected boolean doAction( String action )
    {
        if ( action.equals( "stop0" ) )
        {
            WrapperManager.stop( 0 );
            
        }
        else if ( action.equals( "stop1" ) )
        {
            WrapperManager.stop( 1 );
            
        }
        else if ( action.equals( "exit0" ) )
        {
            System.exit( 0 );
            
        }
        else if ( action.equals( "exit1" ) )
        {
            System.exit( 1 );
            
        }
        else if ( action.equals( "nestedexit1" ) )
        {
            m_nestedExit = true;
            WrapperManager.stop( 1 );
            
        }
        else if ( action.equals( "stopimmediate0" ) )
        {
            WrapperManager.stopImmediate( 0 );
        }
        else if ( action.equals( "stopimmediate1" ) )
        {
            WrapperManager.stopImmediate( 1 );
        }
        else if ( action.equals( "stopandreturn0" ) )
        {
            WrapperManager.stopAndReturn( 0 );
        }
        else if ( action.equals( "halt0" ) )
        {
            // Execute runtime.halt(0) using reflection so this class will
            //  compile on 1.2.x versions of Java.
            Method haltMethod;
            try
            {
                haltMethod = Runtime.class.getMethod( "halt", new Class[] { Integer.TYPE } );
            }
            catch ( NoSuchMethodException e )
            {
                System.out.println( Main.getRes().getString( "halt not supported by current JVM." ) );
                haltMethod = null;
            }
            
            if ( haltMethod != null )
            {
                Runtime runtime = Runtime.getRuntime();
                try
                {
                    haltMethod.invoke( runtime, new Object[] { new Integer( 0 ) } );
                }
                catch ( IllegalAccessException e )
                {
                    System.out.println( Main.getRes().getString( "Unable to call runtime.halt: {0}", e.getMessage() ) );
                }
                catch ( InvocationTargetException e )
                {
                    System.out.println( Main.getRes().getString( "Unable to call runtime.halt: {0}", e.getMessage() ) );
                }
            }
        }
        else if ( action.equals( "halt1" ) )
        {
            // Execute runtime.halt(1) using reflection so this class will
            //  compile on 1.2.x versions of Java.
            Method haltMethod;
            try
            {
                haltMethod = Runtime.class.getMethod( "halt", new Class[] { Integer.TYPE } );
            }
            catch ( NoSuchMethodException e )
            {
                System.out.println( Main.getRes().getString( "halt not supported by current JVM." ) );
                haltMethod = null;
            }
            
            if ( haltMethod != null )
            {
                Runtime runtime = Runtime.getRuntime();
                try
                {
                    haltMethod.invoke( runtime, new Object[] { new Integer( 1 ) } );
                }
                catch ( IllegalAccessException e )
                {
                    System.out.println( Main.getRes().getString( "Unable to call runtime.halt: {0}", e.getMessage() ) );
                }
                catch ( InvocationTargetException e )
                {
                    System.out.println( Main.getRes().getString( "Unable to call runtime.halt: {0}", e.getMessage() ) );
                }
            }
        }
        else if ( action.equals( "restart" ) )
        {
            WrapperManager.restart();
            
        }
        else if ( action.equals( "restartandreturn" ) )
        {
            WrapperManager.restartAndReturn();
            
        }
        else if ( action.equals( "access_violation" ) )
        {
            // The bug we used to cause this is not in most modern VMs so this is not shown by default.
            WrapperManager.accessViolation();
            
        }
        else if ( action.equals( "access_violation_native" ) )
        {
            WrapperManager.accessViolationNative();
            
        }
        else if ( action.equals( "appear_hung" ) )
        {
            WrapperManager.appearHung();
            
        }
        else if ( action.equals( "deadlock" ) )
        {
            if ( WrapperManager.isStandardEdition() )
            {
                System.out.println( Main.getRes().getString( "Creating a 2-object deadlock...") );
                DeadLock.create2ObjectDeadlock();
            }
            else
            {
                System.out.println( Main.getRes().getString( "Deadlock checks require the Standard Edition.") );
            }
        }
        else if ( action.equals( "outofmemory" ) )
        {
            throw new OutOfMemoryError();
        }
        else if ( action.equals( "ignore_events" ) )
        {
            m_ignoreControlEvents = true;
        }
        else if ( action.equals( "dump" ) )
        {
            WrapperManager.requestThreadDump();
            
        }
        else if ( action.equals( "deadlock_out" ) )
        {
            System.out.println( Main.getRes().getString( "Deadlocking System.out and System.err ..." ) );
            m_out.setDeadlock( true );
            m_err.setDeadlock( true );
            
        }
        else if ( action.equals( "users" ) )
        {
            if ( !m_users )
            {
                System.out.println( Main.getRes().getString( "Begin polling the current and interactive users." ) );
                m_users = true;
            }
            else if ( m_groups )
            {
                System.out.println( Main.getRes().getString("Stop polling for group info." ) );
                m_groups = false;
            }
            else
            {
                System.out.println( Main.getRes().getString("Stop polling the current and interactive users." ) );
                m_users = false;
            }
            
            synchronized( AbstractActionApp.class )
            {
                AbstractActionApp.class.notifyAll();
            }
        }
        else if ( action.equals( "groups" ) )
        {
            if ( ( !m_users ) || ( !m_groups ) )
            {
                System.out.println( Main.getRes().getString( "Begin polling the current and interactive users with group info." ) );
                m_users = true;
                m_groups = true;
            }
            else
            {
                System.out.println( Main.getRes().getString( "Stop polling for group info." ) );
                m_groups = false;
            }
            
            synchronized( AbstractActionApp.class )
            {
                AbstractActionApp.class.notifyAll();
            }
        }
        else if ( action.equals( "console" ) )
        {
            if ( m_consoleRunner == null )
            {
                m_consoleRunner = new Thread( "console-runner" )
                {
                    public void run()
                    {
                        System.out.println();
                        System.out.println( Main.getRes().getString( "Start prompting for actions." ) );
                        try
                        {
                            BufferedReader r = new BufferedReader(new InputStreamReader(System.in));
                            String line;
                            try
                            {
                                do {
                                    System.out.println( Main.getRes().getString( "Input an action ('help' for a list of actions):") );
                                    line = r.readLine();
                                    if ((line != null) && (!line.equals(""))) {
                                        System.out.println(Main.getRes().getString( "Read action: {0}", line ) );
                                        if ( !doAction( line ) )
                                        {
                                            if ( !line.equals( "help" ) )
                                            {
                                            System.out.println( Main.getRes().getString( "Unknown action: {0}", line ) );
                                            }
                                            printActions();
                                        }
                                    }
                                } while (true);
                            }
                            catch ( IOException e )
                            {
                                e.printStackTrace();
                            }
                        }
                        finally
                        {
                            System.out.println( Main.getRes().getString( "Stop prompting for actions." ) );
                            System.out.println();
                            m_consoleRunner = null;
                        }
                    }
                };
                m_consoleRunner.setDaemon( true );
                m_consoleRunner.start();
            }
        }
        else if ( action.equals( "idle" ) )
        {
            System.out.println( Main.getRes().getString( "Run idle." ) );
            m_users = false;
            m_groups = false;
            
            synchronized( AbstractActionApp.class )
            {
                AbstractActionApp.class.notifyAll();
            }
        }
        else if ( action.equals( "properties" ) )
        {
            System.out.println( Main.getRes().getString( "Dump System Properties:" ) );
            Properties props = System.getProperties();
            for ( Enumeration en = props.propertyNames(); en.hasMoreElements(); )
            {
                String name = (String)en.nextElement();
                System.out.println( "  " + name + "=" + props.getProperty( name ) );
            }
            System.out.println();
        }
        else if ( action.equals( "configuration" ) )
        {
            System.out.println( Main.getRes().getString( "Dump Wrapper Properties:" ) );
            Properties props = WrapperManager.getProperties();
            for ( Enumeration en = props.propertyNames(); en.hasMoreElements(); )
            {
                String name = (String)en.nextElement();
                System.out.println( "  " + name + "=" + props.getProperty( name ) );
            }
            System.out.println();
        }
        else if ( action.equals( "listener" ) )
        {
            System.out.println( Main.getRes().getString( "Updating Event Listeners:" ) );
            WrapperManager.removeWrapperEventListener( this );
            WrapperManager.addWrapperEventListener( this, m_eventMask );
        }
        else if ( action.equals( "service_list" ) )
        {
            /*
            for ( int i = 0; i < 1000; i++ )
            {
                WrapperWin32Service[] services = WrapperManager.listServices();
            }
            */
            WrapperWin32Service[] services = WrapperManager.listServices();
            if ( services == null )
            {
                System.out.println( Main.getRes().getString( "Services not supported by current platform." ) );
            }
            else
            {
                System.out.println( Main.getRes().getString( "Registered Services:" ) );
                for ( int i = 0; i < services.length; i++ )
                {
                    System.out.println( "  " + services[i] );
                }
            }
        }
        else if ( action.equals( "service_interrogate" ) )
        {
            try
            {
                /*
                for ( int i = 0; i < 10000; i++ )
                {
                    WrapperWin32Service service = WrapperManager.sendServiceControlCode(
                        m_serviceName, WrapperManager.SERVICE_CONTROL_CODE_INTERROGATE );
                }
                */
                WrapperWin32Service service = WrapperManager.sendServiceControlCode(
                    m_serviceName, WrapperManager.SERVICE_CONTROL_CODE_INTERROGATE );
                System.out.println( Main.getRes().getString( "Service after interrogate: {0}", service ) );
            }
            catch ( WrapperServiceException e )
            {
                e.printStackTrace();
            }
        }
        else if ( action.equals( "service_start" ) )
        {
            try
            {
                WrapperWin32Service service = WrapperManager.sendServiceControlCode(
                    m_serviceName, WrapperManager.SERVICE_CONTROL_CODE_START );
                System.out.println( Main.getRes().getString( "Service after start: {0}", service ) );
            }
            catch ( WrapperServiceException e )
            {
                e.printStackTrace();
            }
        }
        else if ( action.equals( "service_stop" ) )
        {
            try
            {
                WrapperWin32Service service = WrapperManager.sendServiceControlCode(
                    m_serviceName, WrapperManager.SERVICE_CONTROL_CODE_STOP );
                System.out.println( Main.getRes().getString( "Service after stop: {0}", service ) );
            }
            catch ( WrapperServiceException e )
            {
                e.printStackTrace();
            }
        }
        else if ( action.equals( "service_user" ) )
        {
            try
            {
                for ( int i = 128; i < 256; i+=10 )
                {
                    WrapperWin32Service service = WrapperManager.sendServiceControlCode(
                        m_serviceName, i );
                    System.out.println( Main.getRes().getString( "Service after user code {0} : {1}", new Integer( i ), service ) );
                }
            }
            catch ( WrapperServiceException e )
            {
                e.printStackTrace();
            }
        }
        else if ( action.equals( "console_title" ) )
        {
            if ( !WrapperManager.isWindows() )
            {
                System.out.println( Main.getRes().getString( "Setting the console title not supported on UNIX platforms." ) );
                // The call is fine but it doesn't do anything.
            }
            
            WrapperManager.setConsoleTitle( m_consoleTitle );
        }
        else if ( action.equals( "child_exec" ) )
        {
            Thread childRunner = new Thread()
            {
                public void run()
                {
                    try
                    {
                        WrapperProcessConfig wpConfig = new WrapperProcessConfig();
                        wpConfig.setDetached( m_childDetached );
                        final WrapperProcess wProcess = WrapperManager.exec( m_childCommand, wpConfig );
                        System.out.println( Main.getRes().getString( "Launched child process with PID={0} : {1}", new Integer( wProcess.getPID() ), m_childCommand ) );
                        
                        Thread outRunner = new Thread()
                        {
                            public void run()
                            {
                                try
                                {
                                    BufferedReader br = new BufferedReader( new InputStreamReader( wProcess.getInputStream() ) );
                                    String line;
                                    while( ( line = br.readLine( ) ) != null )
                                    {
                                        System.out.println( wProcess.getPID() + " out: " + line );
                                    }
                                    br.close();
                                    System.out.println( wProcess.getPID() + Main.getRes().getString( " out EOF" ) );
                                }
                                catch ( IOException e )
                                {
                                    System.out.println( wProcess.getPID() + Main.getRes().getString( " read stdout failed:" ) );
                                    e.printStackTrace();
                                }
                            }
                        };
                        Thread errRunner = new Thread()
                        {
                            public void run()
                            {
                                try
                                {
                                    BufferedReader br = new BufferedReader( new InputStreamReader( wProcess.getErrorStream() ) );
                                    String line;
                                    while( ( line = br.readLine( ) ) != null )
                                    {
                                        System.out.println( wProcess.getPID() + " err: " + line );
                                    }
                                    br.close();
                                    System.out.println( wProcess.getPID() + Main.getRes().getString( " err EOF" ) );
                                }
                                catch ( IOException e )
                                {
                                    System.out.println( wProcess.getPID() + Main.getRes().getString( " read stderr failed:" ) );
                                    e.printStackTrace();
                                }
                            }
                        };
                        
                        outRunner.start();
                        errRunner.start();
                        
                        // Wait for the stdout and stderr reader threads to complete before we say the process completed to avoid confusion.
                        outRunner.join();
                        errRunner.join();
                        
                        System.out.println( Main.getRes().getString( "Child with PID={0} terminated with exitCode={1} : {2} ", new Integer( wProcess.getPID() ), new Integer( wProcess.waitFor() ), m_childCommand ) );
                    }
                    catch ( Throwable t )
                    {
                        t.printStackTrace();
                    }
                }
            };
            childRunner.start();
        }
        else if ( action.equals( "gc" ) )
        {
            System.out.println( Main.getRes().getString( "Begin GC..." ) );
            System.gc();
            System.out.println( Main.getRes().getString( "GC complete." ) );
        }
        else if ( action.equals( "is_professional" ) )
        {
            System.out.println( Main.getRes().getString( "Professional Edition: " ) + WrapperManager.isProfessionalEdition() );
        }
        else if ( action.equals( "is_standard" ) )
        {
            System.out.println( Main.getRes().getString( "Standard Edition: " ) + WrapperManager.isStandardEdition() );
        }
        else
        {
            // Unknown action
            return false;
        
        }
        
        return true;
    }
    
    /*---------------------------------------------------------------
     * Static Methods
     *-------------------------------------------------------------*/
    protected static void printActions()
    {
        System.err.println( "" );
        System.err.println( Main.getRes().getString( "[ACTIONS]" ) );
        System.err.println( Main.getRes().getString( "   help                     : Shows this help message" ) );
        System.err.println( Main.getRes().getString( "  Actions which should cause the Wrapper to exit cleanly:" ) );
        System.err.println( Main.getRes().getString( "   stop0                    : Calls WrapperManager.stop(0)" ) );
        System.err.println( Main.getRes().getString( "   exit0                    : Calls System.exit(0)" ) );
        System.err.println( Main.getRes().getString( "   stopimmediate0           : Calls WrapperManager.stopImmediate(0)" ) );
        System.err.println( Main.getRes().getString( "   stopandreturn0           : Calls WrapperManager.stopAndReturn(0)" ) );
        System.err.println( Main.getRes().getString( "  Actions which should cause the Wrapper to exit in an error state:" ) );
        System.err.println( Main.getRes().getString( "   stop1                    : Calls WrapperManager.stop(1)" ) );
        System.err.println( Main.getRes().getString( "   exit1                    : Calls System.exit(1)" ) );
        System.err.println( Main.getRes().getString( "   nestedexit1              : Calls System.exit(1) within WrapperListener.stop(1) callback" ) );
        System.err.println( Main.getRes().getString( "   stopimmediate1           : Calls WrapperManager.stopImmediate(1)" ) );
        System.err.println( Main.getRes().getString( "  Actions which should cause the Wrapper to restart the JVM:" ) );
        System.err.println( Main.getRes().getString( "   access_violation_native  : Calls WrapperManager.accessViolationNative()" ) );
        System.err.println( Main.getRes().getString( "   appear_hung              : Calls WrapperManager.appearHung()" ) );
        System.err.println( Main.getRes().getString( "   halt0                    : Calls Runtime.getRuntime().halt(0)" ) );
        System.err.println( Main.getRes().getString( "   halt1                    : Calls Runtime.getRuntime().halt(1)" ) );
        System.err.println( Main.getRes().getString( "   restart                  : Calls WrapperManager.restart()" ) );
        System.err.println( Main.getRes().getString( "   restartandreturn         : Calls WrapperManager.restartAndReturn()" ) );
        System.err.println( Main.getRes().getString( "  Additional Tests:" ) );
        System.err.println( Main.getRes().getString( "   ignore_events            : Makes this application ignore control events." ) );
        System.err.println( Main.getRes().getString( "   dump                     : Calls WrapperManager.requestThreadDump()" ) );
        System.err.println( Main.getRes().getString( "   deadlock_out             : Deadlocks the JVM's System.out and err streams." ) );
        System.err.println( Main.getRes().getString( "   users                    : Start polling the current and interactive users." ) );
        System.err.println( Main.getRes().getString( "   groups                   : Start polling the current and interactive users with groups." ) );
        System.err.println( Main.getRes().getString( "   console                  : Prompt for actions in the console." ) );
        System.err.println( Main.getRes().getString( "   idle                     : Do nothing just run in idle mode." ) );
        System.err.println( Main.getRes().getString( "   properties               : Dump all System Properties to the console." ) );
        System.err.println( Main.getRes().getString( "   configuration            : Dump all Wrapper Configuration Properties to the console." ) );
        System.err.println( Main.getRes().getString( "   gc                       : Perform a GC sweep." ) );
        System.err.println( "" );
    }
}

