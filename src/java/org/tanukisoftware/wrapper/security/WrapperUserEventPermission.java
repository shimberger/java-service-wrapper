package org.tanukisoftware.wrapper.security;

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

import java.security.AccessControlException;
import java.security.BasicPermission;
import java.security.Permission;
import java.util.ArrayList;
import java.util.StringTokenizer;

import org.tanukisoftware.wrapper.WrapperManager;

/**
 * WrapperEventPermissions are used to grant the right to register to start
 * receiving events from the Wrapper.
 * <p>
 * Some of these permissions can result in performance degredations if used
 * impropperly.
 * <p>
 * The following are examples of how to specify the permission within a policy
 * file.
 * 
 * <pre>
 *   grant codeBase "file:../lib/-" {
 *     // Grant various permissions to a specific service.
 *     permission org.tanukisoftware.wrapper.security.WrapperEventPermission "service";
 *     permission org.tanukisoftware.wrapper.security.WrapperEventPermission "service, core";
 *     permission org.tanukisoftware.wrapper.security.WrapperEventPermission "*";
 *   };
 * </pre>
 * <p>
 * Possible eventTypes include the following:
 * <table border='1' cellpadding='2' cellspacing='0'>
 * <tr>
 * <th>Permission Event Type Name</th>
 * <th>What the Permission Allows</th>
 * <th>Risks of Allowing this Permission</th>
 * </tr>
 * 
 * <tr>
 * <td>service</td>
 * <td>Register to obtain events whenever the Wrapper service receives any
 * service events.</td>
 * <td>Malicious code could receive this event and never return and thus cause
 * performance and timeout problems with the Wrapper. Normal use of these events
 * are quite safe however.</td>
 * </tr>
 * 
 * <tr>
 * <td>control</td>
 * <td>Register to obtain events whenever the Wrapper receives any system
 * control signals.</td>
 * <td>Malicious code could trap and consome control events, thus preventing an
 * application from being shut down cleanly.</td>
 * </tr>
 * 
 * <tr>
 * <td>core</td>
 * <td>Register to obtain events on the core workings of the Wrapper.</td>
 * <td>Malicious code or even well meaning code can greatly affect the
 * performance of the Wrapper simply by handling these methods slowly. Some of
 * these events are fired from within the core timing code of the Wrapper. They
 * are useful for testing and performance checks, but in general they should not
 * be used by most applications.</td>
 * </tr>
 * </table>
 * 
 * @author Leif Mortenson <leif@tanukisoftware.com>
 */


public final class WrapperUserEventPermission extends BasicPermission
{
    /**
     * Serial Version UID.
     */
    private static final long serialVersionUID = 8916489326587298168L;
    private final int EVENT_MIN = 1;
    private final int EVENT_MAX = 32767;
    private ArrayList m_eventArr;

    /**
     * This method evaluates the passed in permission's action String and stores them in 
     * chunks in an array
     * @param action the permission's actions
     */
    private void parseValids( String action )
    {
        int lastValue = 0, currentValue;
        m_eventArr = new ArrayList();
        if ( action.compareTo( "*" ) == 0 )
        {
            m_eventArr.add( new String( EVENT_MIN + "-" + EVENT_MAX ) );
            return;
        }
        StringTokenizer strok = new StringTokenizer( action.trim(), "," );
        while ( strok.hasMoreTokens() )
        {
            String element = strok.nextToken();
            if ( element.indexOf( '*' ) >= 0 )
            {
                throw new AccessControlException( WrapperManager.getRes().getString( "can't define '*' inside a sequence." ) );
            }
            int range = element.indexOf( "-" );
            if ( range >= 0 )
            {
                if ( range == 0 )
                {
                    if ( m_eventArr.size() != 0 )
                    {
                        throw new AccessControlException( WrapperManager.getRes().getString( "Value {0} has to be first element in sequence.", element ) );
                    }
                    else
                    {
                        lastValue = Integer.parseInt( element.substring( 1 ) );
                        if ( lastValue <= EVENT_MIN || lastValue > EVENT_MAX )
                        {
                            throw new AccessControlException( WrapperManager.getRes().getString( "Value {0} is out of bounds.", new Integer( lastValue ) ) );
                        }
                        m_eventArr.add( new String( EVENT_MIN + "-" + lastValue ) );
                    }
                }
                else if ( range == element.length() - 1 )
                {
                    currentValue = Integer.parseInt( element.substring( 0, element.length() - 1 ) );
                    if ( currentValue <= EVENT_MIN || currentValue > EVENT_MAX )
                    {
                        throw new AccessControlException( WrapperManager.getRes().getString( "Value {0} is out of bounds.", new Integer( lastValue ) ) );
                    }
                    if ( currentValue < lastValue )
                    {
                        throw new AccessControlException( WrapperManager.getRes().getString( "Value {0} is not sorted.", new Integer( currentValue ) ) );
                    }
                    lastValue = currentValue;
                    if ( strok.hasMoreTokens() )
                    {
                        throw new AccessControlException( WrapperManager.getRes().getString( "Value {0} has to be last element in sequence.", element ) );
                    }
                    m_eventArr.add( currentValue + "-" + EVENT_MAX );
                }
                else
                {
                    currentValue = Integer.parseInt( element.substring( 0, range ) );
                    if ( currentValue <= EVENT_MIN || currentValue > EVENT_MAX )
                    {
                        throw new AccessControlException( WrapperManager.getRes().getString( "Value {0} is out of bounds.", new Integer( lastValue ) ) );
                    }
                    if ( currentValue < lastValue )
                    {
                        throw new AccessControlException( WrapperManager.getRes().getString( "Value {0} is not sorted.", new Integer( currentValue ) ) );
                    }
                    lastValue = currentValue;
                    currentValue = Integer.parseInt( element.substring( range + 1 ) );
                    if ( currentValue <= EVENT_MIN || currentValue > EVENT_MAX )
                    {
                        throw new AccessControlException( WrapperManager.getRes().getString( "Value {0} is out of bounds.", new Integer( lastValue ) ) );
                    }
                    if ( currentValue < lastValue )
                    {
                        throw new AccessControlException( WrapperManager.getRes().getString( "Value {0} is not sorted.", new Integer( currentValue ) ) );
                    }
                    m_eventArr.add( lastValue + "-" + currentValue );
                    lastValue = currentValue;
                }
            }
            else
            {
                currentValue = Integer.parseInt( element );
                if ( currentValue < lastValue )
                {
                    throw new java.security.AccessControlException( WrapperManager.getRes().getString( "Value {0} is not sorted.", new Integer( currentValue ) ) );
                }
                lastValue = currentValue;
                m_eventArr.add( element );
            }
        }
    }

    /*---------------------------------------------------------------
     * Constructors
     *-------------------------------------------------------------*/
    /**
     * Creates a new WrapperEventPermission for the specified service.
     * 
     * @param action
     *            The event type or event types to be registered.
     */
    public WrapperUserEventPermission( String action )
    {
        super( "fireUserEvent", String.valueOf( action ) );
        parseValids( action );
    }
    /**
     * Creates a new WrapperEventPermission for the specified service.
     *
     * @param action The event type or event types to be registered.
     */
    public WrapperUserEventPermission( String name, String action )
    {
        super( name, action );
        parseValids( action );
    }

    /**
     * Return the canonical string representation of the eventTypes.
     *  Always returns present eventTypes in the following order: 
     *  start, stop, pause, continue, interrogate. userCode.
     *
     * @return the canonical string representation of the eventTypes.
     */
    public String getActions()
    {
        String s = "";
        for ( int i = 0; i < m_eventArr.size(); i++ )
        {
            if ( i > 0 )
            {
                s = s.concat( "," );
            }
            s = s.concat( (String)m_eventArr.get( i ) );
        }
        return s;
    }

    /**
     * Checks if this WrapperEventPermission object "implies" the
     *  specified permission.
     * <P>
     * More specifically, this method returns true if:<p>
     * <ul>
     *  <li><i>p2</i> is an instanceof FilePermission,<p>
     *  <li><i>p2</i>'s eventTypes are a proper subset of this object's eventTypes,
     *      and<p>
     *  <li><i>p2</i>'s service name is implied by this object's service name.
     *      For example, "MyApp*" implies "MyApp".
     * </ul>
     *
     * @param p2 the permission to check against.
     *
     * @return true if the specified permission is implied by this object,
     */
    public boolean implies( Permission p )
    {
        int current, min, max, check, border;
        String element;
        check = Integer.parseInt( p.getActions() );
        for ( int i = 0; i < m_eventArr.size(); i++ )
        {
            element = (String)m_eventArr.get( i );
            border = element.indexOf( '-' );
            if ( border >= 0 )
            {
                min = Integer.parseInt( element.substring( 0, border ) );
                max = Integer.parseInt( element.substring( border + 1 ) );
                if ( min <= check && check <= max )
                {
                    return true;
                }
            }
            else
            {
                current = Integer.parseInt( element );
                if ( current == check )
                {
                    return true;
                }
            }
        }
        return false;
    }
}
