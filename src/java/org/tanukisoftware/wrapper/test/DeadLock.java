package org.tanukisoftware.wrapper.test;

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

import org.tanukisoftware.wrapper.WrapperManager;

/**
 * This is test is designed to simulate a series of deadlock cases which will
 *  can be detected and restarted by the Standard Edition.
 *
 * @author Leif Mortenson <leif@tanukisoftware.com>
 */
public class DeadLock
{
    private int m_id;
    private Object m_obj1;
    private Object m_obj2;
    
    private DeadLock( int id, Object obj1, Object obj2 )
    {
        m_id = id;
        m_obj1 = obj1;
        m_obj2 = obj2;
        
        Thread runner = new Thread( "Locker-" + m_id )
        {
            public void run()
            {
                System.out.println( Main.getRes().getString( "Locker-{0}: Started", new Integer( m_id ) ) );
                
                try
                {
                    lockFirst();
                }
                catch ( Throwable t )
                {
                    t.printStackTrace();
                    
                }
                
                System.out.println( Main.getRes().getString( "Locker-{0}: Complete", new Integer( m_id ) ) );
            }
        };
        runner.start();
    }
    
    /*---------------------------------------------------------------
     * Methods
     *-------------------------------------------------------------*/
    private void lockSecond()
    {
        System.out.println( Main.getRes().getString( "Locker-{0}: Try locking {1}...", new Integer( m_id ) , m_obj2.toString() ) );
        synchronized( m_obj2 )
        {
            System.out.println( Main.getRes().getString("Locker-{0}: Oops! Locked {1}", new Integer( m_id ), m_obj2.toString() ) );
        }
    }
    
    private void lockFirst()
    {
        System.out.println( Main.getRes().getString( "Locker-{0}: Locking {1}", new Integer( m_id ) , m_obj1.toString() ) );
        synchronized( m_obj1 )
        {
            System.out.println( Main.getRes().getString("Locker-{0}: Locked {1}", new Integer( m_id ), m_obj1.toString() ) );
            
            try
            {
                Thread.sleep( 2000 );
            }
            catch ( InterruptedException e )
            {
            }
            
            lockSecond();
        }
    }
    
    /*---------------------------------------------------------------
     * Static Methods
     *-------------------------------------------------------------*/
    public static void create2ObjectDeadlock()
    {
        Object obj1 = new Object();
        Object obj2 = new Object();
        
        new DeadLock( 1, obj1, obj2 );
        new DeadLock( 2, obj2, obj1 );
    }
    
    public static void create3ObjectDeadlock()
    {
        Object obj1 = new Object();
        Object obj2 = new Object();
        Object obj3 = new Object();
        
        new DeadLock( 1, obj1, obj2 );
        new DeadLock( 2, obj2, obj3 );
        new DeadLock( 3, obj3, obj1 );
    }
    
    /*---------------------------------------------------------------
     * Main Method
     *-------------------------------------------------------------*/
    public static void main( String[] args )
    {
        System.out.println( Main.getRes().getString("Deadlock Tester Running..."));

        Object obj1 = new Object();
        Object obj2 = new Object();
        Object obj3 = new Object();
        
        int exitCode = WrapperManager.getJVMId();
        switch ( exitCode )
        {
        case 1:
            System.out.println( Main.getRes().getString("2-object deadlock."));
            create2ObjectDeadlock();
            break;
            
        case 2:
            System.out.println( Main.getRes().getString("Wait then 2-object deadlock."));
            try
            {
                Thread.sleep( 10000 );
            }
            catch ( InterruptedException e )
            {
            }
            create2ObjectDeadlock();
            break;
            
        case 3:
            System.out.println( Main.getRes().getString("3-object deadlock."));
            create3ObjectDeadlock();
            break;
            
        default:
            System.out.println( Main.getRes().getString("Done."));
        }
        
        // Always wait a couple seconds to make sure the above threads have time to start.
        try
        {
            Thread.sleep( 2000 );
        }
        catch ( InterruptedException e )
        {
        }
        System.out.println( Main.getRes().getString("Main Complete."));
    }
}
