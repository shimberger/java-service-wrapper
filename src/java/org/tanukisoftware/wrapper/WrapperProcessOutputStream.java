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

import java.io.IOException;
import java.io.OutputStream;

/**
 * The OutputStream Class of a WrapperProcess, representing all the data the 
 * ChildProcess read from the Wrapper.
 *
 * @author Christian Mueller <christian.mueller@tanukisoftware.co.jp>
 * @since Wrapper 3.4.0
 */
public class WrapperProcessOutputStream
    extends OutputStream
{
    private long m_ptr;
    private boolean m_closed;

    /*---------------------------------------------------------------
     * Constructors
     *-------------------------------------------------------------*/
    /**
     * This class can only be instantiated by native code.
     */
    private WrapperProcessOutputStream()
    {
    }

    /*---------------------------------------------------------------
     * Native Methods
     *-------------------------------------------------------------*/
    private native void nativeWrite( int b );
    private native void nativeClose();

    /*---------------------------------------------------------------
     * Methods
     *-------------------------------------------------------------*/
    /**
     * Writes a byte to the Stream.
     *
     * @param b byte to write.
     *
     * @throws IOException in case the stream has been already closed or any
     *                     other IO error.
     * @throws WrapperLicenseError If the function is called other than in
     *                             the Professional Edition or from a Standalone JVM.
     *                     
     */
    public void write( int b )
        throws IOException
    {
        synchronized( this )
        {
            if ( !m_closed )
            {
                nativeWrite( b );
            }
            else
            {
                throw new IOException( WrapperManager.getRes().getString( "Stream is closed." ) );
            }
        }
    }

    /**
     * Closes the OutputStream.
     *
     * @throws IOException If there were any problems closing the stream.
     * @throws WrapperLicenseError If the function is called other than in
     *                             the Professional Edition or from a Standalone JVM.
     */
     public void close()
        throws IOException
     {
        if ( !m_closed )
        {
            nativeClose();
            m_closed = true;
        }
    }
}
