package org.tanukisoftware.wrapper.demo;

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

import java.awt.Color;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.PrintStream;

import javax.swing.JOptionPane;
import javax.swing.text.SimpleAttributeSet;
import javax.swing.text.StyleConstants;
import org.tanukisoftware.wrapper.WrapperActionServer;
import org.tanukisoftware.wrapper.WrapperJNIError;
import org.tanukisoftware.wrapper.WrapperLicenseError;
import org.tanukisoftware.wrapper.WrapperManager;
import org.tanukisoftware.wrapper.WrapperListener;
import org.tanukisoftware.wrapper.WrapperProcessConfig;
import org.tanukisoftware.wrapper.WrapperResources;
import org.tanukisoftware.wrapper.WrapperSystemPropertyUtil;

/**
 * This is a Test / Example program which can be used to test the main features
 * of the Wrapper.
 * <p>
 * It is also an example of Integration Method #3, where you implement the
 * WrapperListener interface manually.
 * <p>
 * <b>NOTE</b> that in most cases you will want to use Method #1, using the
 * WrapperSimpleApp helper class to integrate your application. Please see the
 * <a href="http://wrapper.tanukisoftware.com/doc/english/integrate.html">
 * integration</a> section of the documentation for more details.
 * 
 * @author Leif Mortenson <leif@tanukisoftware.com>
 */
public class DemoApp implements WrapperListener
{
    private WrapperActionServer m_actionServer;

    private static DemoAppMainFrame m_frame;

    private static boolean m_isTestCaseRunning;
    private static Process m_testCase;
    private static PrintStream m_childPrintStream;
    private static WrapperResources m_res;
    private String m_confFile;

    /*---------------------------------------------------------------
     * Constructors
     *-------------------------------------------------------------*/
    private DemoApp()
    {
        m_isTestCaseRunning = false;
        m_confFile = "../conf/demoapp.conf";

    }

    protected DemoAppMainFrame getFrame()
    {
        return m_frame;
    }

    protected void setTestCaseRunning( boolean val )
    {
        m_isTestCaseRunning = val;
    }

    protected boolean isTestCaseRunning()
    {
        return m_isTestCaseRunning;
    }


    public static WrapperResources getRes()
    {
        if ( m_res == null )
        {
            // Synchronize and then recheck to avoid this method always synchronizing.
            synchronized( DemoApp.class )
            {
                if ( m_res == null )
                {
                    m_res = WrapperManager.loadWrapperResources( "wrapperTestApp",
                        WrapperSystemPropertyUtil.getStringProperty( "wrapper.lang.folder", "../lang" ) );
                }
            }
        }
        return m_res;
    }

    /*---------------------------------------------------------------
     * Inner Classes
     *-------------------------------------------------------------*/

    /*---------------------------------------------------------------
     * WrapperListener Methods
     *-------------------------------------------------------------*/

    public void controlEvent( int event )
    {
        System.out.println( getRes().getString( "TestWrapper: controlEvent({0})", new Integer( event ) ) );

        if ( event == WrapperManager.WRAPPER_CTRL_LOGOFF_EVENT )
        {
            if ( WrapperManager.isLaunchedAsService() || WrapperManager.isIgnoreUserLogoffs() )
            {
                System.out.println( getRes().getString( "TestWrapper:   Ignoring logoff event" ) );
                // Ignore
            }
            else
            {
                WrapperManager.stop( 0 );
            }
        }
        else if ( event == WrapperManager.WRAPPER_CTRL_C_EVENT )
        {
            // WrapperManager.stop(0);
            // May be called before the runner is started.
        }
        else
        {
            WrapperManager.stop( 0 );
        }
    }

    public Integer start( String[] args )
    {
        String command;
        if ( args.length <= 0 )
        {
            command = "dialog";
        }
        else
        {
            command = args[0];
        }
        if ( command.equals( "dialog" ) )
        {
            System.out.println( "Demo: start()" );
            System.out.println( getRes().getString( "Demo: Showing dialog..." ) );
            try
            {
                m_frame = new DemoAppMainFrame( this );
                m_frame.setVisible( true );
                try
                {
                    m_testCase = this.callAction( "start" );
                }
                catch ( IOException e )
                {
                    // TODO Auto-generated catch block
                    e.printStackTrace();
                }
            }
            catch ( java.lang.InternalError e )
            {
                System.out.println( getRes().getString( "Demo: " ) );
                System.out.println( getRes().getString( "Demo: ERROR - Unable to display the GUI:" ) );
                System.out.println( "Demo:           " + e.toString() );
                System.out.println( "Demo: " );
                System.out.println( getRes().getString( "Demo: This demo requires a display to show its GUI.  Exiting..." ) );
                command = "console";
            }
            catch ( java.awt.AWTError e )
            {
                System.out.println( "Demo: " );
                System.out.println( getRes().getString( "Demo: ERROR - Unable to display the GUI:" ) );
                System.out.println( "Demo:           " + e.toString() );
                System.out.println( "Demo: " );
                System.out.println( getRes().getString( "Demo: This demo requires a display to show its GUI.  Exiting..." ) );
                command = "console";
            }
            catch ( java.lang.UnsupportedOperationException e )
            {
                // java.awt.HeadlessException does not exist in Java versions
                // prior to 1.4
                if ( e.getClass().getName().equals( "java.awt.HeadlessException" ) )
                {
                    System.out.println( "Demo: " );
                    System.out.println( getRes().getString( "Demo: ERROR - Unable to display the GUI:" ) );
                    System.out.println( "Demo:           " + e.toString() );
                    System.out.println( "Demo: " );
                    System.out.println( getRes().getString( "Demo: This demo requires a display to show its GUI.  Exiting..." ) );
                    command = "console";
                }
                else
                {
                    throw e;
                }
            }
        }
        else if ( command.equals( "start" ) )
        {
            Thread commandRunner = new Thread( "DemoApp-Command-Runner" ) {
                public void run() {
                    BufferedReader br = new BufferedReader( new InputStreamReader( System.in ) );
                    String command = "";
                    System.out.println( getRes().getString( "Started and waiting for a command from the Demo Application" ) );
                    while ( command.compareToIgnoreCase( "finish" ) != 0 )
                    {
                        try
                        {
                            command = br.readLine();
                            System.out.println( getRes().getString( "Read action: {0}", command ) );
        
                            if ( command.equals( "crash" ) )
                            {
                                Thread.sleep( 5000 );
                                System.out.println( getRes().getString( "Going to cause the JVM to crash." ) );
        
                                WrapperManager.accessViolationNative();
                            }
                            else if ( command.equals( "frozen" ) )
                            {
                                WrapperManager.appearHung();
                                System.out.println( getRes().getString( "Waiting until wrapper stops this JVM." ) );
                                while ( true )
                                {
                                }
                            }
                            else if ( command.equals( "out_of_mem" ) )
                            {
                                int ii = 5 - 3;
                                System.out.println( getRes().getString( "Going to cause a Out of Memory Error" ) );
                                Thread.sleep( 5000 );
                                // System.out.println("got java.lang.OutOfMemoryError");
                                if ( 5 > ii )
                                {
                                    getOutOfMemError();
                                }
                                System.out.println( getRes().getString( "Application should get restarted now..." ) );
                                try
                                {
                                    Thread.sleep( 5000 );
                                }
                                catch ( InterruptedException e )
                                {
                                    // TODO Auto-generated catch block
                                    e.printStackTrace();
                                }
                            }
                            else if ( command.equals( "deadlock" ) )
                            {
                                if ( WrapperManager.isStandardEdition() )
                                {
                                    System.out.println( getRes().getString( "Deadlock Tester Running..." ) );
                                    Object obj1 = new Object();
                                    Object obj2 = new Object();
                                    int exitCode = 1;
                                    DeadLock dl = new DeadLock( 1, obj1, obj2 );
                                    switch ( exitCode )
                                    {
                                        case 1:
                                            System.out.println( getRes().getString( "2-object deadlock." ) );
                                            dl.create2ObjectDeadlock();
                                            break;
                                        case 2:
                                            System.out.println( getRes().getString( "Wait then 2-object deadlock." ) );
                                            try
                                            {
                                                Thread.sleep( 10000 );
                                            }
                                            catch ( InterruptedException e )
                                            {
                                            }
                                            dl.create2ObjectDeadlock();
                                            break;
                                        case 3:
                                            System.out.println( getRes().getString( "3-object deadlock." ) );
                                            dl.create3ObjectDeadlock();
                                            break;
            
                                        default:
                                            System.out.println( getRes().getString( "Done." ) );
                                    }
                                    // Always wait a couple seconds to make sure the above
                                    // threads have time to start.
                                    try
                                    {
                                        System.out.println( getRes().getString( "Sleeping for 5 sec..." ) );
                                        Thread.sleep( 5000 );
                                    }
                                    catch ( InterruptedException e )
                                    {
                                    }
                                    System.out.println( getRes().getString( "Main Complete." ) );
                                }
                                else
                                {
                                    System.out.println( getRes().getString( "Deadlock checks require at least the Standard Edition." ) );
                                }
                            }
                            else if ( command.indexOf( "exec" ) == 0 )
                            {
                                try
                                {
                                    String input = command.substring(5);
                                    System.out.println( getRes().getString( "Starting a simple application: " ) + input );
                                   
                                    if ( input != null && input.length() > 0 )
                                    {
                                        WrapperManager.exec( input, new WrapperProcessConfig().setDetached( false ) );
                                        System.out.println( getRes().getString( "Successfully executed!") );
                                    }
                                }
                                catch ( SecurityException e )
                                {
                                    e.printStackTrace();
                                }
                                catch ( NullPointerException e )
                                {
                                    e.printStackTrace();
                                }
                                catch ( IllegalArgumentException e )
                                {
                                    e.printStackTrace();
                                }
                                catch ( UnsatisfiedLinkError e )
                                {
                                    e.printStackTrace();
                                }
                                catch ( IOException e )
                                {
                                    e.printStackTrace();
                                }
                                catch ( WrapperJNIError e )
                                {
                                    e.printStackTrace();
                                }
                                catch ( WrapperLicenseError e )
                                {
                                    e.printStackTrace();
                                }
                            }
        
                        }
                        catch ( IOException e )
                        {
                            // TODO Auto-generated catch block
                            e.printStackTrace();
                        }
                        catch ( InterruptedException e )
                        {
                            // TODO Auto-generated catch block
                            e.printStackTrace();
                        }
                    }
                }
            };
            commandRunner.start();
            
        }
        return null;
    }

    public int stop( int exitCode )
    {
        System.out.println( "Demo: stop(" + exitCode + ")" );

        if ( m_actionServer != null )
        {
            try
            {
                m_actionServer.stop();
            }
            catch ( Exception e )
            {
                System.out.println( getRes().getString( "Demo: Unable to stop the action server: {0}", e.getMessage() ) );
            }
        }
        if ( m_frame != null )
        {
            if ( !WrapperManager.hasShutdownHookBeenTriggered() )
            {
                m_frame.setVisible( false );
                m_frame.dispose();
            }
            m_frame = null;
        }
        return exitCode;
    }

    private Process callAction( String action ) throws IOException
    {
        Process p = null;
        
        if ( action.equals( "mail" ) )
        {
            MailDialog md = new MailDialog();
            md.setVisible( true );

            if ( md.getResult() != 0 )
            {
                callAction( "finish" );
                try
                {
                    Thread.sleep( 3000 );
                    if ( m_isTestCaseRunning )
                    {
                        System.out.println( getRes().getString( "destroy!" ) );
                        m_testCase.destroy();
                    }
                }
                catch ( InterruptedException e )
                {
                    // TODO Auto-generated catch block
                    e.printStackTrace();
                }
                String wrapperBin;
                if ( WrapperManager.isWindows() )
                {
                    wrapperBin = "../bin/wrapper.exe";
                }
                else
                {
                    wrapperBin = "../bin/wrapper";
                }
                String arg = wrapperBin + " -c " + m_confFile + " wrapper.console.flush=TRUE wrapper.console.format=LPM  wrapper.app.parameter.1=start " + md.getEvents()
                        + " wrapper.event.default.email.debug=TRUE wrapper.event.default.email.smtp.host=" + md.getServer() + " wrapper.event.default.email.smtp.port="
                        + md.getPort() + " wrapper.event.default.email.sender=" + md.getSender() + " wrapper.event.default.email.recipient=" + md.getRecipients();
                // System.out.println( "execing: " + arg );
                p = Runtime.getRuntime().exec( arg );
                m_childPrintStream = new PrintStream( p.getOutputStream() );

                if ( p != null )
                {
                    m_frame.jTabbedPane2.setSelectedIndex( 1 );
                    m_isTestCaseRunning = true;
                    BufferedReader br = new BufferedReader( new InputStreamReader( p.getInputStream() ) );
                    Runnable setTextRun = new LoggerThread( br, this );
                    Thread t = new Thread( setTextRun );
                    t.start();
                    // return true;
                }
            }
        }
        else if ( action.equals( "daemon" ) )
        {
            System.out.println( getRes().getString( "Going to install an application as daemon - this requires root privileges" ) );
            p = Runtime.getRuntime().exec( " ../bin/testwrapper install" );
            if ( p != null )
            {
                m_frame.jTabbedPane2.setSelectedIndex( 1 );
               
                BufferedReader br = new BufferedReader( new InputStreamReader( p.getInputStream() ) );
                Runnable setTextRun = new LoggerThread( br, this, true );
                Thread t = new Thread( setTextRun );
                t.start();
                try
                {
                    p.waitFor();
                }
                catch ( InterruptedException e )
                {
                    // TODO Auto-generated catch block
                    e.printStackTrace();
                }
                p = null;
                p = Runtime.getRuntime().exec( "../bin/testwrapper remove" );
                if ( p != null )
                {
                    m_frame.jTabbedPane2.setSelectedIndex( 1 );
                   
                    BufferedReader br2 = new BufferedReader( new InputStreamReader( p.getInputStream() ) );
                    Runnable setTextRun2 = new LoggerThread( br2, this, true );
                    Thread t2 = new Thread( setTextRun2 );
                    t2.start();
                    try
                    {
                        p.waitFor();
                    }
                    catch ( InterruptedException e )
                    {
                        // TODO Auto-generated catch block
                        e.printStackTrace();
                    }
                  
                    // return true;
                } 
                
            } 
            p = null;
            // WrapperManager.exec( "sudo ../test/demoapp remove" );

        }
        else if ( action.equals( "service" ) )
        {
            p = Runtime.getRuntime().exec( "..\\bin\\wrapper.exe -it " + m_confFile + " wrapper.console.flush=TRUE" );
            // WrapperManager.exec( "sudo ../test/demoapp remove" );
            if ( p != null )
            {
                m_frame.jTabbedPane2.setSelectedIndex( 1 );
               
                BufferedReader br = new BufferedReader( new InputStreamReader( p.getInputStream() ) );
                Runnable setTextRun = new LoggerThread( br, this, true );
                Thread t = new Thread( setTextRun );
                t.start();
                // return true;
            } 
            p = null;

        }
        else if ( action.equals( "customize" ) )
        {
            CustomizeDialog cd = new CustomizeDialog();
            cd.setVisible( true );

            if ( cd.getResult() == 1 )
            {
                String arg = "\"" + cd.getSelectedSource() + "\"" + " --customize --target \"" + cd.getSelectedDestination() + "\"";

                if ( cd.getSelectedIcon() != null && cd.getSelectedIcon().length() > 0 )
                {
                    arg = arg.concat( " --icon \"" + cd.getSelectedIcon() + "\"" );
                }

                if ( cd.getSelectedSplashScreen() != null && cd.getSelectedSplashScreen().length() > 0 )
                {
                    arg = arg.concat( " --splash \"" + cd.getSelectedSplashScreen() + "\"" );
                }
                p = Runtime.getRuntime().exec( arg );
                if ( p != null )
                {
                    m_frame.jTabbedPane2.setSelectedIndex( 1 );

                    BufferedReader br = new BufferedReader( new InputStreamReader( p.getInputStream() ) );
                    Runnable setTextRun = new LoggerThread( br, this, true);
                    Thread t = new Thread( setTextRun );
                    t.start();
                    // return true;
                } 
                p = null;
            }
        }
        else if ( action.equals( "start" ) )
        {
            String wrapperBin;
            if ( WrapperManager.isWindows() )
            {
                wrapperBin = "../bin/wrapper.exe";
            }
            else
            {
                wrapperBin = "../bin/wrapper";
            }
            String arg = wrapperBin + " -c " + m_confFile + " wrapper.console.flush=TRUE wrapper.console.format=LPM wrapper.app.parameter.1=" + action;
            // System.out.println( "calling: " + arg );

            if ( !m_isTestCaseRunning )
            {
                // System.out.println( "execing: " + arg );
                p = Runtime.getRuntime().exec( arg );

                m_childPrintStream = new PrintStream( p.getOutputStream() );
            }
            if ( p != null )
            {
                m_frame.jTabbedPane2.setSelectedIndex( 1 );
                m_isTestCaseRunning = true;
                BufferedReader br = new BufferedReader( new InputStreamReader( p.getInputStream() ) );
                Runnable setTextRun = new LoggerThread( br, this );
                Thread t = new Thread( setTextRun );
                t.start();
            }
            // ps.close();
        }
        else if ( action.equals( "finish" ) )
        {
            if ( m_isTestCaseRunning )
            {
                m_childPrintStream.println( action );
    
                m_childPrintStream.flush();
                m_childPrintStream.close();
                // ps.close();
            }
        }
        else if ( action.equals( "exec" ) )
        { 
 
            String input = "";
            if ( System.getProperty( "os.name" ).indexOf( "Windows" ) >= 0 )
            {
                input = ( String )JOptionPane.showInputDialog( m_frame, getRes().getString( "Please enter the Command, you wish to execute:" ),
                          getRes().getString( "Child Process Execution" ), JOptionPane.QUESTION_MESSAGE, null, null,
                          ( Object )"notepad" );
            }
            else
            {
                input = ( String )JOptionPane.showInputDialog( m_frame, getRes().getString( "Please enter the Command, you wish to execute:" ),
                          getRes().getString( "Child Process Execution" ), JOptionPane.QUESTION_MESSAGE, null, null,
                          ( Object )"xclock" );
            }
            if ( input != null && input.length() > 0 )
            {
                String wrapperBin;
                if ( WrapperManager.isWindows() )
                {
                    wrapperBin = "../bin/wrapper.exe";
                }
                else
                {
                    wrapperBin = "../bin/wrapper";
                }
                String arg = wrapperBin + " -c " + m_confFile + " wrapper.console.flush=TRUE wrapper.console.format=LPM wrapper.app.parameter.1=start";
                //System.out.println( "calling: " + arg );
                if ( !m_isTestCaseRunning )
                {
                    p = Runtime.getRuntime().exec( arg );

                    m_childPrintStream = new PrintStream( p.getOutputStream() );
                    DemoApp.m_frame.getJMenuBar().getMenu( 0 ).getItem( 0 ).setEnabled( false );
                    DemoApp.m_frame.getJMenuBar().getMenu( 0 ).getItem( 1 ).setEnabled( true );
                }
                if ( p != null )
                {
                    m_frame.jTabbedPane2.setSelectedIndex( 1 );
                    m_isTestCaseRunning = true;
                    BufferedReader br = new BufferedReader( new InputStreamReader( p.getInputStream() ) );
                    Runnable setTextRun = new LoggerThread( br, this );
                    Thread t = new Thread( setTextRun );
                    t.start();
                    // return true;
                }

                m_childPrintStream.println( action + " " + input);

                m_childPrintStream.flush();
            }

        }
        else
        {
            String wrapperBin;
            if ( WrapperManager.isWindows() )
            {
                wrapperBin = "../bin/wrapper.exe";
            }
            else
            {
                wrapperBin = "../bin/wrapper";
            }
            String arg = wrapperBin + " -c " + m_confFile + " wrapper.console.flush=TRUE wrapper.console.format=LPM wrapper.app.parameter.1=start";
            //System.out.println( "calling: " + arg );
            if ( !m_isTestCaseRunning )
            {
                p = Runtime.getRuntime().exec( arg );

                m_childPrintStream = new PrintStream( p.getOutputStream() );
                DemoApp.m_frame.getJMenuBar().getMenu( 0 ).getItem( 0 ).setEnabled( false );
                DemoApp.m_frame.getJMenuBar().getMenu( 0 ).getItem( 1 ).setEnabled( true );
            }
            if ( p != null )
            {
                m_frame.jTabbedPane2.setSelectedIndex( 1 );
                m_isTestCaseRunning = true;
                BufferedReader br = new BufferedReader( new InputStreamReader( p.getInputStream() ) );
                Runnable setTextRun = new LoggerThread( br, this );
                Thread t = new Thread( setTextRun );
                t.start();
                // return true;
            }

            m_childPrintStream.println( action );

            m_childPrintStream.flush();
            // ps.close();
        }
        return p == null ? m_testCase : p;
    }

    /*---------------------------------------------------------------
     * Static Methods
     *-------------------------------------------------------------*/
    protected boolean doAction( String action )
    {
        //System.out.println( "doAction " + action );
        try
        {
            m_testCase = this.callAction( action );
        }
        catch ( IOException e )
        {
            // TODO Auto-generated catch block
            e.printStackTrace();
        }
        return false;
    }

    static void getOutOfMemError()
    {
        try
        {
            throw new java.lang.OutOfMemoryError( getRes().getString( "BANG" ) );
        }
        catch ( OutOfMemoryError e )
        {
            e.printStackTrace();
        }
    }

    /*---------------------------------------------------------------
     * Main Method
     *-------------------------------------------------------------*/
    /**
     * IMPORTANT: Please read the Javadocs for this class at the top of the page
     * before you start to use this class as a template for integrating your own
     * application. This will save you a lot of time.
     */
    public static void main( String[] args )
    {
        System.out.println( getRes().getString( "DemoApp: Initializing..." ) );
        WrapperManager.start( ( new DemoApp() ), args );
    }
}

class LoggerThread implements Runnable
{
    BufferedReader br;
    DemoApp m_this;
    boolean optional;

    public LoggerThread( BufferedReader br, DemoApp m_this )
    {
        this.br = br;
        this.m_this = m_this;
        this.optional = false;
    }
    public LoggerThread( BufferedReader br, DemoApp m_this, boolean optional )
    {
        this.br = br;
        this.m_this = m_this;
        this.optional = optional;
    }

    public void run()
    {
        try
        {
            String str;
            int j = 0;
            if ( !optional ) {
                m_this.getFrame().getlogTextArea().getDocument().remove( 0, m_this.getFrame().getlogTextArea().getDocument().getLength() );
            }
            while ( ( str = br.readLine() ) != null )
            {
                if ( j++ > 0 && !str.equals( "" ) )
                {
                    if ( m_this != null && m_this.getFrame() != null )
                    {
                        SimpleAttributeSet sas = new SimpleAttributeSet();
                        String insString = getStyle( str, sas );
                        
                        
                        m_this.getFrame().getlogTextArea().getDocument().insertString( m_this.getFrame().getlogTextArea().getDocument().getEndPosition().getOffset() - 1, insString + "\n",
                                sas );
                        int p1 = m_this.getFrame().getlogTextArea().getDocument().getLength();
                        m_this.getFrame().getlogTextArea().setCaretPosition( p1 );
                    }
                }
            }
            if (!optional)
            {
                m_this.setTestCaseRunning( false );
            }
        }
        catch ( Exception x )
        {
            x.printStackTrace();
        }
    }

    private String getStyle( String insString, SimpleAttributeSet sas )
    {

        String returnVal = insString;

        StyleConstants.setFontFamily( sas, "monospaced" );
        StyleConstants.setFontSize( sas, 12 );
        StyleConstants.setBold( sas, true );

        if ( insString.indexOf( "STATUS |" ) >= 0 || insString.indexOf( "NOTICE |" ) >= 0)
        {
            StyleConstants.setForeground( sas, Color.black );
            returnVal = insString.substring( 9 );
        }
        else if ( insString.indexOf( "DEBUG  |" ) >= 0 )
        {
            StyleConstants.setForeground( sas, Color.blue );
            returnVal = insString.substring( 9 );
        }
        else if ( insString.indexOf( "INFO   |" ) >= 0 )
        {
            StyleConstants.setForeground( sas, new Color( 52, 169, 88 ) );
            returnVal = insString.substring( 9 );
        }
        else if ( insString.indexOf( "WARN   |" ) >= 0 )
        {
            StyleConstants.setForeground( sas, new Color( 230, 140, 20 ) );
            returnVal = insString.substring( 9 );
        }
        else if ( insString.indexOf( "FATAL  |" ) >= 0 )
        {
            StyleConstants.setForeground( sas, Color.red );
            returnVal = insString.substring( 9 );
        }
        else if ( insString.indexOf( "ERROR  |" ) >= 0 )
        {
            StyleConstants.setForeground( sas, Color.red );
            returnVal = insString.substring( 9 );
        }

        if ( insString.indexOf( "WARNING" ) >= 0 )
        {
            StyleConstants.setForeground( sas, new Color( 230, 140, 20 ) );
        }
        if ( insString.indexOf( "WrapperManager Error:" ) >= 0 || insString.indexOf( "java.lang.OutOfMemoryError:" ) >= 0 )
        {
            StyleConstants.setForeground( sas, Color.red );
        }
        return returnVal;

    }
}
