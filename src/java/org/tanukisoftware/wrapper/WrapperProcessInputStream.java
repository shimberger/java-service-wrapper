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

import java.io.ByteArrayInputStream;
import java.io.InputStream;
import java.io.IOException;

/**
 * The InputStream Class of a WrapperProcess, representing all the data the 
 * ChildProcess writes to the Wrapper.
 *
 * @author Christian Mueller <christian.mueller@tanukisoftware.co.jp>
 * @since Wrapper 3.4.0
 */
public class WrapperProcessInputStream
    extends InputStream
{
    private long m_ptr;
    private boolean m_closed;
    private ByteArrayInputStream m_bais;

    /*---------------------------------------------------------------
     * Constructors
     *-------------------------------------------------------------*/
    /**
     * This class can only be instantiated by native code.
     */
    private WrapperProcessInputStream()
    {
    }

    /*---------------------------------------------------------------
     * Native Methods
     *-------------------------------------------------------------*/
    private native int nativeRead( boolean blocking );
    private native void nativeClose();
    private native int nativeRead2( byte[] b, int off, int len, boolean blocking );

    /*---------------------------------------------------------------
     * Methods
     *-------------------------------------------------------------*/
    /**
     * Closes the InputStream
     * @throws IOException in case of any file errors
     * @throws WrapperLicenseError If the function is called other than in
     *                             the Professional Edition or from a Standalone JVM.
     */
    public void close()
        throws IOException
    {
        synchronized( this )
        {
            if ( !m_closed )
            {
                nativeClose();
                m_closed = true;
            }
        }
    }
    public boolean markSupported()
    {
        return false;
    }

    public boolean ready()
    {
        if ( !m_closed || (m_bais != null && m_bais.available() > 0 ))
        {
            return true;
        } 
        else 
        {
            return false;
        }
    }

    /**
     * Read a character from the Stream and moves the position in the stream
     * @return single sign from the stream
     * @throws IOException in case the stream has been already closed or any other file error
     * @throws WrapperLicenseError If the function is called other than in
     *                             the Professional Edition or from a Standalone JVM.
     */
    public int read()
        throws IOException
    {
        
            if ( !m_closed )
            {
                return nativeRead( true );
            }
            else
            {
                if ( m_bais != null )
                {
                    return m_bais.read();
                }
                else
                {
                    throw new IOException(WrapperManager.getRes().getString( "Stream is closed." ) );
                }
            }
        
    }

    public int read( byte b[ ]) throws IOException 
    {
        return read( b, 0, b.length );
    }

    
    
    public int read( byte b[], int off, int len ) throws IOException
    {
        int c;
        synchronized (this) {
            if ( b == null )
            {
                throw new NullPointerException();
            }
            else if ( off < 0 || len < 0 || len > b.length - off )
            {
                throw new IndexOutOfBoundsException();
            }
            else if ( len == 0 )
            {
                return 0;
            }
            if ( !ready() )
            {
                return -1;
            }
            if (!m_closed)
            {
                c = nativeRead2( b, off, len, false );
                if ( c == -1 ) /* a process can terminate only once */
                {
                    c = nativeRead2( b, off, len, true );
                }
            }
            else
            {
                c = m_bais.read( b, off, len );
            }
            return c == 0 ? -1 : c;
        }
    }

    /*---------------------------------------------------------------
     * Private Methods
     *-------------------------------------------------------------*/
    /**
     * This method gets called when a spawned Process has terminated 
     *  and the pipe buffer gets read and stored in an byte array.
     *  This way we can close the Filedescriptor and keep the number 
     *  of open FDs as small as possible.
     */
    private void readAndCloseOpenFDs()
    {
        synchronized( this )
        {
            int i;
            int msg;
            if ( m_closed ) 
            {
                return;
            }
            try
            {
                byte[] buffer = new byte[0];
                i = 0;
                while ( ( msg = nativeRead( false ) ) != -1 )
                {
                    int newSize = buffer.length + 1;
                    byte[] temp = new byte[newSize];
                    System.arraycopy( buffer, 0, temp, 0, buffer.length );
                    buffer = temp;
                    buffer[i++] = (byte) msg;
                }
                m_bais = new ByteArrayInputStream( buffer );
                close(); 
            }
            catch( IOException ioe )
            {
                System.out.println( WrapperManager.getRes().getString( "WrapperProcessStream encountered a ReadError: " ) );
                ioe.printStackTrace();
            }
        }
    }
}

  
