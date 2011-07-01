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

import java.io.File;

/**
 * WrapperLogFileChangedEvent are fired whenever the log file used by the
 *  Wrapper is changed.  This can happen due to nightly log rotation for
 *  example.
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 */
public class WrapperLogFileChangedEvent
    extends WrapperLoggingEvent
{
    private final File m_logFile;
    
    /*---------------------------------------------------------------
     * Constructors
     *-------------------------------------------------------------*/
    /**
     * Creates a new WrapperLogFileChangedEvent.
     */
    public WrapperLogFileChangedEvent( File logFile )
    {
        m_logFile = logFile;
    }
    
    /*---------------------------------------------------------------
     * Methods
     *-------------------------------------------------------------*/
    /**
     * Returns the updated log file name.
     *
     * @return The updated log file name.
     */
    public File getLogFile()
    {
        return m_logFile;
    }
    
    /*---------------------------------------------------------------
     * Method
     *-------------------------------------------------------------*/
    /**
     * Returns a string representation of the event.
     *
     * @return A string representation of the event.
     */
    public String toString()
    {
        return "WrapperLogFileChangedEvent[logFile=" + getLogFile() + "]";
    }
}
