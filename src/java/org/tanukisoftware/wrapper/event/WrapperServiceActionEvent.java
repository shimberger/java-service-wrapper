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
 * WrapperServicePauseResumeEvents are used to notify the listener that the Wrapper
 *  is requesting that the Java application be paused or resumed.  This does not
 *  mean that it should exit, only that it should internally go into an idle state.
 *
 * See the wrapper.pausable and wrapper.pausable.stop_jvm properties for more
 *  information.
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 *
 * @since Wrapper 3.5.0
 */
public abstract class WrapperServiceActionEvent
    extends WrapperServiceEvent
{
    /**
     * Serial Version UID.
     */
    private static final long serialVersionUID = 7901768955067874864L;
    
    /**
     * Action result of a configured timer being fired.
     *  See wrapper.timer.<n>.action.
     */
    public static final int SOURCE_CODE_TIMER                   = 1;
    
    /**
     * Action result of a configured filter being fired.
     *  See wrapper.filter.action.<n>.
     */
    public static final int SOURCE_CODE_FILTER                  = 2;
    
    /**
     * Action result of a command from a command file.
     *  See wrapper.commandfile.
     */
    public static final int SOURCE_CODE_COMMANDFILE             = 3;
    
    /**
     * Action result of an event command's block timeout expired.
     *  See wrapper.event.<event_name>.command.block.action.
     */
    public static final int SOURCE_CODE_COMMAND_BLOCK_TIMEOUT   = 8;
    
    /**
     * Action resulted from the Windows Service Manager.  This can happen
     *  from a number of sources including the command line, Service Control
     *  Panel, etc.
     */
    public static final int SOURCE_CODE_WINDOWS_SERVICE_MANAGER = 9;
    
    /**
     * Code which keeps track of how the service was paused.
     */
    private int m_sourceCode;
    
    /*---------------------------------------------------------------
     * Constructors
     *-------------------------------------------------------------*/
    /**
     * Creates a new WrapperServiceActionEvent.
     *
     * @param sourceCode Source Code specifying where the action originated.
     */
    public WrapperServiceActionEvent( int sourceCode )
    {
        m_sourceCode = sourceCode;
    }
    
    /*---------------------------------------------------------------
     * Methods
     *-------------------------------------------------------------*/
    /**
     * Returns the Source Code.
     *
     * @return The Source Code.
     */
    public int getSourceCode()
    {
        return m_sourceCode;
    }
    
    /**
     * Returns a string representation of the event.
     *
     * @return A string representation of the event.
     */
    public String toString()
    {
        return "WrapperServiceActionEvent[sourceCode=" + getSourceCode() + "]";
    }
}
