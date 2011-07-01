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
 * WrapperServicePauseEvents are used to notify the listener that the Wrapper
 *  is requesting that the Java application be paused.  This does not mean that
 *  it should exit, only that it should internally go into an idle state.
 *
 * See the wrapper.pausable and wrapper.pausable.stop_jvm properties for more
 *  information.
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 *
 * @since Wrapper 3.5.0
 */
public class WrapperServicePauseEvent
    extends WrapperServiceActionEvent
{
    /**
     * Serial Version UID.
     */
    private static final long serialVersionUID = 1308747091110200773L;
    
    /*---------------------------------------------------------------
     * Constructors
     *-------------------------------------------------------------*/
    /**
     * Creates a new WrapperServicePauseEvent.
     *
     * @param sourceCode Source Code specifying where the pause action originated.
     */
    public WrapperServicePauseEvent( int sourceCode )
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
        return "WrapperServicePauseEvent[sourceCode=" + getSourceCode() + "]";
    }
}
