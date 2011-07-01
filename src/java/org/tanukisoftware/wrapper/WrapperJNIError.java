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

/**
 * WrapperJNIErrors are thrown when user code encounters problems accessing
 *  native Wrapper features.
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 */
public class WrapperJNIError
    extends Error
{
    /**
     * Serial Version UID.
     */
    private static final long serialVersionUID = 4163224795268336447L;

    /*---------------------------------------------------------------
     * Constructors
     *-------------------------------------------------------------*/
    /**
     * Creates a new WrapperJNIError.
     *
     * @param message Message describing the exception.
     */
    WrapperJNIError( String message )
    {
        super( message );
    }
    
    /**
     * Creates a new WrapperJNIError.
     *
     * @param message Message describing the exception.
     */
    WrapperJNIError( byte[] message )
    {
        this( new String( message ) );
    }

    /*---------------------------------------------------------------
     * Methods
     *-------------------------------------------------------------*/
    /**
     * Return string representation of the Error.
     *
     * @return String representation of the Error.
     */
    public String toString()
    {
        return this.getClass().getName() + " " + getMessage(); 
    }
}

