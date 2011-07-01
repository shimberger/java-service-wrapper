package org.tanukisoftware.wrapper;

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

import java.io.InputStream;
import java.io.IOException;
import java.io.OutputStream;

/**
 * A WrapperProcess is returned by a call to WrapperManager.exec and can
 *  be used to reference the new child process after it was launched.
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 * @since Wrapper 3.4.0
 */
public class WrapperProcess
{
    private WrapperProcessOutputStream m_wpis;
    private WrapperProcessInputStream m_wpos;
    private WrapperProcessInputStream m_wpes;

    /* The PID of the process. */
    private int m_pid;
    private int m_exitcode;
    private boolean m_isDetached;
    private int m_softShutdownTimeout;

    /*---------------------------------------------------------------
     * Constructors
     *-------------------------------------------------------------*/
    /**
     * The default constructor
     */
    private WrapperProcess()
    {
        m_exitcode = Integer.MIN_VALUE;
    }

    /*---------------------------------------------------------------
     * Native Methods
     *-------------------------------------------------------------*/
    private native boolean nativeIsAlive();
    private native void nativeDestroy();
    private native void nativeExitValue();
    private native void nativeWaitFor();

    /*---------------------------------------------------------------
     * Methods
     *-------------------------------------------------------------*/
    /**
     * Finalize method.
     */
    protected void finalize()
        throws Throwable
    {
        try
        {
            //System.out.println( m_pid + " finalized");
            m_wpes.close();
            m_wpis.close();
            m_wpos.close();
        }
        finally 
        {
            super.finalize();
        }
    }

    /**
     * Returns the PID of the process.
     *
     * @return The PID of the process.
     */
    public int getPID()
    {
        return m_pid;
    }

    /**
     * Gets the input stream of the subprocess. The stream obtains data piped
     *  from the standard output stream of the process represented by this
     *  WrapperProcess object.
     * <p>
     * Implementation note: It is a good idea for the input stream to be
     *  buffered.
     * 
     * @return The input stream connected to the normal output of the
     *         subprocess.
     *
     * @throws IOException If we are unable to access the stream.
     */
    public InputStream getInputStream()
        throws IOException
    {
        return m_wpos;
    }

    /**
     * Gets the error stream of the subprocess. The stream obtains data piped
     *  from the error output stream of the process represented by this
     *  WrapperProcess object.
     * <p>
     * Implementation note: It is a good idea for the input stream to be
     *  buffered.
     * 
     * @return The input stream connected to the error stream of the
     *         subprocess.
     *
     * @throws IOException If we are unable to access the stream.
     */
    public InputStream getErrorStream()
        throws IOException
    {
        return m_wpes;
    }

    /**
     * Gets the output stream of the subprocess. Output to the stream is piped
     *  into the standard input stream of the process represented by this
     *  WrapperProcess object.
     * <p>
     * Implementation note: It is a good idea for the output stream to be
     *  buffered.
     * 
     * @return The output stream connected to the normal input of the
     *         subprocess.
     *
     * @throws IOException If we are unable to access the stream.
     */
    public OutputStream getOutputStream()
        throws IOException
    {
        return m_wpis;
    }


    /**
     * Causes the current thread to wait, if necessary, until the process
     *  represented by this Process object has terminated. This method returns
     *  immediately if the subprocess has already terminated. If the subprocess
     *  has not yet terminated, the calling thread will be blocked until the
     *  subprocess exits. 
     *
     * @return The exit value of the process. By convention, 0 indicates normal
     *         termination. 
     *
     * @throws InterruptedException If the current thread is interrupted by
     *                              another thread while it is waiting, then
     *                              the wait is ended and an
     *                              InterruptedException is thrown.
     */
    public int waitFor()
        throws InterruptedException
    {
        synchronized( this )
        {
            if ( m_exitcode == Integer.MIN_VALUE )
            {
                nativeWaitFor();
            }
            return m_exitcode;
        }
    }

    /**
     * Returns the exit value for the subprocess.
     *
     * @return The exit value of the subprocess represented by this
     *         WrapperProcess object.  By convention, the value 0
     *         indicates normal termination.
     *
     * @throws IllegalThreadStateException if the process is still alive 
     *                                     and has not yet teminated.
     */
    public int exitValue()
        throws IllegalThreadStateException
    {
        if ( m_exitcode == Integer.MIN_VALUE )
        {
            nativeExitValue();
        }
        return m_exitcode;
    }

    /**
     * Returns true if the process is still alive.
     * @throws WrapperLicenseError If the function is called other than in
     *                             the Professional Edition or from a Standalone JVM.
     * @return True if the process is alive, false if it has terminated.
     */
    public boolean isAlive()
    {
        return nativeIsAlive();
    }

    /**
     * Kills the subprocess. The subprocess represented by this Process object
     *  is forcibly terminated if it is still running.
     * @throws WrapperLicenseError If the function is called other than in
     *                             the Professional Edition or from a Standalone JVM.
     */
    public void destroy()
    {
        nativeDestroy();
    }
}
