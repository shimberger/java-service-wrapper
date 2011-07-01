package org.tanukisoftware.wrapper.demo;

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

import java.awt.Color;
import java.awt.Cursor;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.MouseEvent;
import java.awt.event.MouseListener;
import java.io.IOException;

import javax.swing.Box;
import javax.swing.JButton;
import javax.swing.JDialog;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JPanel;
import javax.swing.border.EmptyBorder;

import org.tanukisoftware.wrapper.WrapperManager;
import org.tanukisoftware.wrapper.test.Main;

public class AboutDialog
    extends JDialog
{
    private static final long serialVersionUID = 1L;

    public AboutDialog( JFrame parent )
    {
        super( parent, DemoApp.getRes().getString("About Dialog"), true );
        
        JPanel main = new JPanel();
        main.setBorder( new EmptyBorder( 10, 20, 10, 20 ) );
        
        this.setResizable( false );
        Box b = Box.createVerticalBox();
        b.add( Box.createGlue() );
        b.add( new JLabel( DemoApp.getRes().getString("Demo Application for the Java Service Wrapper" ) ) );
        b.add( new JLabel( "By Tanuki Software Ltd." ) );
        final JLabel url = new JLabel();
        url.setText( "<html><u>http://wrapper.tanukisoftware.com</u></html>" );
        url.setForeground( Color.BLUE );
        url.addMouseListener( new MouseListener()
        {

            public void mouseReleased( MouseEvent e )
            {
                // TODO Auto-generated method stub
            }

            public void mousePressed( MouseEvent e )
            {
                // TODO Auto-generated method stub
            }

            public void mouseExited( MouseEvent e )
            {
                url.setCursor( new Cursor( Cursor.DEFAULT_CURSOR ) );
            }

            public void mouseEntered( MouseEvent e )
            {
                url.setCursor( new Cursor( Cursor.HAND_CURSOR ) );
            }

            public void mouseClicked( MouseEvent e )
            {

                if ( e.getClickCount() > 0 )
                {
                    String url = "http://wrapper.tanukisoftware.com";
                    String cmd;
                    if ( WrapperManager.isWindows() )
                    {
                        cmd = "cmd.exe /c start " + url;
                    }
                    else if ( WrapperManager.isMacOSX() )
                    {
                        cmd = "open " + url;
                    }
                    else
                    {
                        cmd = "firefox " + url;
                    }
                    
                    try
                    {
                        Runtime.getRuntime().exec( cmd );
                    }
                    catch ( IOException ex )
                    {
                        System.out.println( DemoApp.getRes().getString( "Failed to launch external browser to view web page using command:" ) );
                        System.out.println( "    " + cmd );
                        System.out.println( DemoApp.getRes().getString("  Error: ") + ex.getMessage() );
                        System.out.println();
                        System.out.println( DemoApp.getRes().getString( "Please enter URL into your browser: " ) + url );
                        System.out.println();
                    }
                }

            }
        } );

        b.add( url );
        b.add( Box.createGlue() );
        main.add( b, "Center" );

        JPanel p2 = new JPanel();
        JButton ok = new JButton( "Ok " );
        p2.add( ok );
        main.add( p2, "South" );

        ok.addActionListener( new ActionListener()
        {
            public void actionPerformed( ActionEvent evt )
            {
                setVisible( false );
            }
        } );
        
        getContentPane().add( main, "Center" );
        
        this.setLocation( this.getParent().getLocation() );
        this.pack();
    }

}
