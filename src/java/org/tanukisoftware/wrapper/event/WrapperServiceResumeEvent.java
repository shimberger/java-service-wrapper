package org.tanukisoftware.wrapper.event;

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
 * WrapperServiceResumeEvents are used to notify the listener that the Wrapper
 *  is requesting that the Java application be resumed.
 *
 * See the wrapper.pausable and wrapper.pausable.stop_jvm properties for more
 *  information.
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 *
 * @since Wrapper 3.5.0
 */
public class WrapperServiceResumeEvent
    extends WrapperServiceActionEvent
{
    /**
     * Serial Version UID.
     */
    private static final long serialVersionUID = 338313484021328312L;
    
    /*---------------------------------------------------------------
     * Constructors
     *-------------------------------------------------------------*/
    /**
     * Creates a new WrapperServiceResumeEvent.
     *
     * @param sourceCode Source Code specifying where the resume action originated.
     */
    public WrapperServiceResumeEvent( int sourceCode )
    {
        super( sourceCode );
    }
    
    /*---------------------------------------------------------------
     * Methods
     *-------------------------------------------------------------*/
    /**
     * Returns a string representation of the event.
     *
     * @return A string representation of the event.
     */
    public String toString()
    {
        return "WrapperServiceResumeEvent[sourceCode=" + getSourceCode() + "]";
    }
}
