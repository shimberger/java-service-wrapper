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

import java.text.MessageFormat;

/**
 * A resource bundle which is used to help localize applications to the default
 *  locale of the JVM.   Resources are stored in MO files using the standard UNIX
 *  gettext resources.
 *
 * For example,<P>
 * <CODE>
 * WrapperResources res = WrapperManager.loadWrapperResources( "myapp", "../lang/" );
 * </CODE>
 *
 * To use the WrapperResources, make a call to any of the <CODE>getString()</CODE>
 *  methods.  If the resource files are not found, or the specific key is not found
 *  then the key is returned unmodified.
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 */
public final class WrapperResources
{
    private long m_Id;
    
    /*---------------------------------------------------------------
     * Constructors
     *-------------------------------------------------------------*/
    /**
     * WrapperResources instances are created using the WrapperManager.loadWrapperResources method.
     */
    protected WrapperResources ()
    {
    }
    
    /*---------------------------------------------------------------
     * Finalizers
     *-------------------------------------------------------------*/
    protected void finalize()
        throws Throwable
    {
        try
        {
            // close open files
            nativeDestroyResource();
        }
        catch ( UnsatisfiedLinkError e )
        {
            // Ignore.  This will happen if there is no native library.
        }
        finally
        {
            super.finalize();
        }
    }
    
    /*---------------------------------------------------------------
     * Native Methods
     *-------------------------------------------------------------*/
    private native String nativeGetLocalizedString(String key);
    private native void nativeDestroyResource();

    /*---------------------------------------------------------------
     * Methods
     *-------------------------------------------------------------*/
    /**
     * Request a localized version of the specified key.
     *
     * @param key Resource to be localized.
     *
     * @return The localized version of the key.
     */
    public String getString( String key )
    {
        try
        {
            return nativeGetLocalizedString( key );
        }
        catch ( UnsatisfiedLinkError e )
        {
            return key;
        }
    }
    
    /**
     * Request a localized version of the specified key.
     * <p>
     * Individual tokens will be replaced with the specified parameters using the
     *  Java MessageFormat format method.
     *
     * @param key Resource to be localized.
     * @param arguments An array of argumens to be replaced in the resource.
     *
     * @return The localized version of the key.
     *
     * @see java.text.MessageFormat
     */
    public String getString( String key, Object[] arguments )
    {
        return MessageFormat.format( getString( key ), arguments );
    }
    
    /**
     * Request a localized version of the specified key.
     * <p>
     * Individual tokens will be replaced with the specified parameters using the
     *  Java MessageFormat format method.
     *
     * @param key Resource to be localized.
     * @param arg0 An argument to be replaced in the resource.
     *
     * @return The localized version of the key.
     *
     * @see java.text.MessageFormat
     */
    public String getString( String key, Object arg0 )
    {
        return MessageFormat.format( getString( key ), new Object[] { arg0 } );
    }
    
    /**
     * Request a localized version of the specified key.
     * <p>
     * Individual tokens will be replaced with the specified parameters using the
     *  Java MessageFormat format method.
     *
     * @param key Resource to be localized.
     * @param arg0 An argument to be replaced in the resource.
     * @param arg1 An argument to be replaced in the resource.
     *
     * @return The localized version of the key.
     *
     * @see java.text.MessageFormat
     */
    public String getString( String key, Object arg0, Object arg1 )
    {
        return MessageFormat.format( getString( key ), new Object[] { arg0, arg1 } );
    }
    
    /**
     * Request a localized version of the specified key.
     * <p>
     * Individual tokens will be replaced with the specified parameters using the
     *  Java MessageFormat format method.
     *
     * @param key Resource to be localized.
     * @param arg0 An argument to be replaced in the resource.
     * @param arg1 An argument to be replaced in the resource.
     * @param arg2 An argument to be replaced in the resource.
     *
     * @return The localized version of the key.
     *
     * @see java.text.MessageFormat
     */
    public String getString( String key, Object arg0, Object arg1, Object arg2 )
    {
        return MessageFormat.format( getString( key ), new Object[] { arg0, arg1, arg2 } );
    }
    
    /**
     * Request a localized version of the specified key.
     * <p>
     * Individual tokens will be replaced with the specified parameters using the
     *  Java MessageFormat format method.
     *
     * @param key Resource to be localized.
     * @param arg0 An argument to be replaced in the resource.
     * @param arg1 An argument to be replaced in the resource.
     * @param arg2 An argument to be replaced in the resource.
     * @param arg3 An argument to be replaced in the resource.
     *
     * @return The localized version of the key.
     *
     * @see java.text.MessageFormat
     */
    public String getString( String key, Object arg0, Object arg1, Object arg2, Object arg3 )
    {
        return MessageFormat.format( getString( key ), new Object[] { arg0, arg1, arg2, arg3 } );
    }
    
    /**
     * Request a localized version of the specified key.
     * <p>
     * Individual tokens will be replaced with the specified parameters using the
     *  Java MessageFormat format method.
     *
     * @param key Resource to be localized.
     * @param arg0 An argument to be replaced in the resource.
     * @param arg1 An argument to be replaced in the resource.
     * @param arg2 An argument to be replaced in the resource.
     * @param arg3 An argument to be replaced in the resource.
     * @param arg4 An argument to be replaced in the resource.
     *
     * @return The localized version of the key.
     *
     * @see java.text.MessageFormat
     */
    public String getString( String key, Object arg0, Object arg1, Object arg2, Object arg3, Object arg4 )
    {
        return MessageFormat.format( getString( key ), new Object[] { arg0, arg1, arg2, arg3, arg4 } );
    }
}

