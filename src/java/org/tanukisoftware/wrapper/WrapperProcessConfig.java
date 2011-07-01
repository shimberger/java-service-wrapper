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

import java.io.File;
import java.io.IOException;
import java.util.Iterator;
import java.util.Map;
import java.util.HashMap;
import org.tanukisoftware.wrapper.WrapperLicenseError;

/**
 * With WrapperProcessConfig Class the startup configuration for the Process
 *  can be passed to the WrapperManager.exec methods.  The configuration makes
 *  it possible to control the way the OS spawns the child process, specify
 *  environment variables, working directory, and how the Wrapper should handle
 *  process when the JVM exits.  Please review each of the methods a more
 *  detailed explanation of how they work.
 * <p>
 * The setter methods are designed to be optionally be chained as follows:
 * <code><pre>
 * WrapperProcess proc = WrapperManager.exec( "command", new WrapperProcessConfig().setDetached( true ).setStartType( WrapperProcessConfig.POSIX_SPAWN ) );
 * </pre></code>
 *
 * @author Christian Mueller <christian.mueller@tanukisoftware.co.jp>
 * @since Wrapper 3.4.0
 */
public final class WrapperProcessConfig
{
    public static final int POSIX_SPAWN = 1;
    public static final int FORK_EXEC = 2;
    public static final int VFORK_EXEC = 3;
    public static final int DYNAMIC = 4;

    private boolean m_isDetached;
    private File m_defdir;
    private int m_startType;
    private Map m_environment;
    private int m_softShutdownTimeout;

    private native String[] nativeGetEnv();
    private static native boolean isSupportedNative( int startType );
    

    /*---------------------------------------------------------------
     * Constructors
     *-------------------------------------------------------------*/
    /**
     * Creates a default configuration.
     *
     * @throws WrapperLicenseError If the Professional Edition of the Wrapper
     *                              is not being used.
     */
    public WrapperProcessConfig()
    {
        if ( !WrapperManager.isProfessionalEdition() )
        {
            throw new WrapperLicenseError( "Requires the Professional Edition." );
        }

        m_isDetached = false;
        m_defdir = null;
        m_startType = DYNAMIC;
        m_environment = null;
        m_softShutdownTimeout = 5;
    }

    /*---------------------------------------------------------------
     * Methods
     *-------------------------------------------------------------*/
    /**
     * Indicates whether the specified start type is supported on the current
     *  plattform.
     *
     * @param startType The start type to test.
     *
     * @return true if supported, false otherwise. On Windows, this method always returns
     *              true.
     *
     * @throws WrapperLicenseError If the function is called other than in
     *                             the Professional Edition or from a Standalone JVM.
     * @throws IllegalArgumentException If the startType is invalid.
     */
    public static boolean isSupported( int startType )
        throws WrapperLicenseError, IllegalArgumentException
    {
        if ( !WrapperManager.isProfessionalEdition() )
        {
            throw new WrapperLicenseError(  WrapperManager.getRes().getString( "Requires the Professional Edition." ) );
        }
        verifyStartType( startType );
        return isSupportedNative( startType );
    }
    
    /**
     * Returns the detached flag.
     *
     * @return The detached flag.
     */
    public boolean isDetached()
    {
        return m_isDetached;
    }
    
    /**
     * Sets the detached flag.  This makes it possible to control whether or
     *  not the Wrapper will terminate any child processes launched by a JVM
     *  when that JVM exits or crashes.
     *
     * @param detached If false the Wrapper will remember that the process was
     *                 launched and then make sure that it is terminated when
     *                 the JVM exits.
     *
     * @return This configration to allow chaining.
     */
    public WrapperProcessConfig setDetached( boolean detached )
    {
        m_isDetached = detached;
        return this;
    }

    /**
     * Returns the start type.
     *
     * @return The start type.
     */
    public int getStartType()
    {
        return m_startType;
    }
    
    /**
     * Sets the start type.
     * <p>
     * The start type is used to control how the subprocess will be started by
     *  the OS.  This property has no effect on Windows.
     * <ul>
     *  <li>FORK_EXEC - The most common UNIX/LINUX way to create a child
     *    process.  On some operating systems (esp. Solaris) this call causes
     *    results in the operating system momentarily duplicating the JVM's
     *    memory before launching the child process.  If the JVM is large then
     *    this can result in system level memory errors that can cause the
     *    child process to fail or even the JVM to crash.</li>
     *  <li>VFORK_EXEC - The vfork function differs from fork only in that the
     *    child process can share code and data with the parent process.  This
     *    speeds cloning activity significantly.  Care is taken in this
     *    implementation to avoid the kind of integrety problems that are
     *    possible with this method.  On some systems, vfork is the same as
     *    fork.</li>
     *  <li>POSIX_SPAWN - The process will be spawned in such a way that no
     *    memory duplication takes place.  This makes it possible to spawn
     *    child processes when the JVM is very large on Solaris systems.
     *    (See <a href='http://www.opengroup.org/onlinepubs/009695399/functions/posix_spawn.html'>http://www.opengroup.org/onlinepubs/009695399/functions/posix_spawn.html</a>)
     *    This is available on LINUX, SOLARIS (10+), AIX, z/OS and MACOS.
     *    It will not be possible to set the working directory when using
     *    this start type.</li>
     * <li>DYNAMIC - The ideal forking method will be used for the current
     *    platform.
     *    It will not be possible to set the working directory when using
     *    this start type as the start type used on some platforms does not
     *    support setting a working directory.</li>
     * </ul>
     *
     * @param startType The start type to use when launching the child process.
     *
     * @return This configration to allow chaining.
     *
     * @throws IllegalArgumentException If the startType is invalid.
     */
    public WrapperProcessConfig setStartType( int startType )
        throws IllegalArgumentException
    {
        verifyStartType( startType );

        m_startType = startType;
        return this;
    }
    
    /**
     * Returns the working directory.
     *
     * @return The working directory.
     */
    public File getWorkingDirectory()
    {
        return m_defdir;
    }
    
    /**
     * Sets the working directory.
     *
     * @param workingDirectory The working directory of the subprocess, or null
     *                         if the subprocess should inherit the working
     *                         directory of the JVM.
     *                         Please note, when using the POSIX_SPAWN or DYNAMIC start
     *                         type, it is not possible to set the working
     *                         directory.  Doing so will result in an error when running exec.
     *
     * @return This configration to allow chaining.
     *
     * @throws IOException If the specified working directory can not be resolved.
     */
    public WrapperProcessConfig setWorkingDirectory( File workingDirectory )
        throws IOException
    {
        if ( workingDirectory != null )
        {
            if ( !workingDirectory.exists() )
            {
                throw new IllegalArgumentException( WrapperManager.getRes().getString( "Working directory does not exist." ) );
            }
            else if ( !workingDirectory.isDirectory() )
            {
                throw new IllegalArgumentException( WrapperManager.getRes().getString( "Must be a directory." ) );
            }
        }

        m_defdir = workingDirectory.getCanonicalFile();

        return this;
    }

    /**
     * Returns a Map containing the environment which will be used to launch
     *  the child process.
     * <p>
     * If this Map is modified those changes will be reflected when the process
     *  is launched.  Alternately, the environment can be set with the
     *  setEnvironment method.  Clearing the Map will result in an empty
     *  environment being used.
     *  @throws WrapperLicenseError If the function is called other than in
     *                             the Professional Edition or from a Standalone JVM.
     */
    public Map getEnvironment()
        throws WrapperLicenseError
    {
        if ( m_environment == null )
        {
            m_environment = getDefaultEnvironment();
        }

        return m_environment;
    }

    /**
     * Sets the environment for the child process.
     *
     * @param environment A Map containing the environment to use when launching
     *                    the process.  Passing in an empty Map will result in
     *                    an empty Environment being used.  A null native will
     *                    cause the process to be launched using the same
     *                    environment as the JVM.
     *
     * @return This configration to allow chaining.
     *
     * @throws IllegalArgumentException If any of the names or values are not
     *                                  Strings or if a name is empty.
     */
    public WrapperProcessConfig setEnvironment( Map environment )
    {
        if ( environment != null )
        {
            for ( Iterator iter = environment.entrySet().iterator(); iter.hasNext(); )
            {
                Map.Entry entry = (Map.Entry)iter.next();
                Object key = entry.getKey();
                if ( !( key instanceof String ) )
                {
                    throw new IllegalArgumentException( WrapperManager.getRes().getString( "Map entry names must be Strings." ) );
                } 
                else if ( ( (String)key ).length() <= 0 )
                {
                    throw new IllegalArgumentException( WrapperManager.getRes().getString( "Map entry names must not be empty Strings." ) );
                }
                else if ( ( (String)key ).indexOf( '=' ) != -1 )
                {
                    throw new IllegalArgumentException( WrapperManager.getRes().getString( "Map entry names must not contain an equal sign ('=')." ) );
                }
                Object value = entry.getKey();
                if ( !( value instanceof String ) )
                {
                    throw new IllegalArgumentException( WrapperManager.getRes().getString( "Map entry values must be Strings." ) );
                }
            }
        }
        m_environment = environment;

        return this;
    }

    /**
     * Sets the timeout for the soft shtudown in seconds.
     * When WrapperProcess.destroy() is called the wrapper will first try to
     * stop the application softly giving it time to stop itself properly.
     * If the specified timeout however ellapsed, the Child Process will be 
     * terminated by hard.
     * If 0 was specified, the wrapper will instantly force the termination.
     * If -1 was specified, the wrapper will wait indefinitely for the child
     * to perform the stop.
     * The default value of this property is 5 - giving a process 5 sec to 
     * react on the shutdown request.
     *
     * @param softShutdownTimeout The max timeout for an application to stop, before 
     *                            killing forcibly
     *
     * @return This configration to allow chaining.
     *
     * @throws IllegalArgumentException If the value of the specified timeout is invalid.
     */
    public WrapperProcessConfig setSoftShutdownTimeout( int softShutdownTimeout )
        throws IOException
    {
        if ( softShutdownTimeout < -1 ) {
            throw new IllegalArgumentException( WrapperManager.getRes().getString( "{0} is not a valid value for a timeout.", 
                                   new Integer ( softShutdownTimeout ) ) );
        }
        m_softShutdownTimeout = softShutdownTimeout;
        return this;
    }


    /*---------------------------------------------------------------
     * Private Methods
     *-------------------------------------------------------------*/
    /**
     * Makes sure that the specified startType is valid.
     *
     * @param startType Start type to test.
     *
     * @throws IllegalArgumentException If the startType is invalid.
     */
    private static void verifyStartType( int startType )
        throws IllegalArgumentException
    {
        switch( startType )
        {
        case POSIX_SPAWN:
        case VFORK_EXEC:
        case FORK_EXEC:
        case DYNAMIC:
            break;
            
        default:
            throw new IllegalArgumentException( WrapperManager.getRes().getString( "Unknown start type: {0}", 
                    new Integer( startType ) ) );
        }
    }
    
    /**
     * Returns a Map containing the environment of the current Java process.
     */
    private Map getDefaultEnvironment()
    {
        Map environment = new HashMap();
        String[] nativeEnv = nativeGetEnv();
        for ( int i = 0; i < nativeEnv.length; i++ )
        {
            int pos = nativeEnv[i].indexOf( '=' );
            String name = nativeEnv[i].substring( 0, pos );
            String value = nativeEnv[i].substring( pos + 1 );
            environment.put( name, value );
        }

        return environment;
    }
    
    /**
     * Called by the native code to get the environment.
     */
    private String[] getNativeEnv()
    {
        if ( m_environment == null )
        {
            return nativeGetEnv();
        }
        else
        {
            String[] nativeEnv = new String[ m_environment.size() ];
            Iterator iter = m_environment.entrySet().iterator();
            int i = 0;
            while ( iter.hasNext() )
            {
                Map.Entry pairs = (Map.Entry)iter.next();
                nativeEnv[ i++ ] = pairs.getKey() + "=" + pairs.getValue(); 
            }
            return nativeEnv;
        }
    }
}
