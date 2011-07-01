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

import java.awt.GridBagConstraints;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;

import javax.swing.JButton;
import javax.swing.JCheckBox;
import javax.swing.JDialog;
import javax.swing.JLabel;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JTextField;

import org.tanukisoftware.wrapper.WrapperManager;

public class MailDialog extends JDialog
{
    /**
     * 
     */
    private static final long serialVersionUID = -5989594238193947229L;
    private JPanel jPanel1, jPanel2, jPanel3, jPanel4;
    private JCheckBox jCheckBox1, jCheckBox2, jCheckBox3, jCheckBox4, jCheckBox5, jCheckBox6, jCheckBox7, jCheckBox8, jCheckBox9, jCheckBox10, jCheckBox11, jCheckBox12,
            jCheckBox13, jCheckBox14;
    private JTextField jTextField1, jTextField2, jTextField3, jTextField4;
    private JLabel jLabel1, jLabel2, jLabel3, jLabel4;
    private JButton jButton1, jButton2;
    private GridBagConstraints gridBagConstraints;
    private int result;
    private int m_port;
    private String m_recipient, m_sender, m_server, m_events;

    public String getEvents()
    {
        return this.m_events;
    }

    public String getRecipients()
    {
        return this.m_recipient;
    }

    public String getSender()
    {
        return this.m_sender;
    }

    public String getServer()
    {
        return this.m_server;
    }

    public int getPort()
    {
        return this.m_port;
    }

    public int getResult()
    {
        return this.result;
    }

    public MailDialog()
    {
        jPanel1 = new javax.swing.JPanel();
        jPanel2 = new javax.swing.JPanel();
        jCheckBox1 = new javax.swing.JCheckBox();
        jCheckBox2 = new javax.swing.JCheckBox();
        jCheckBox3 = new javax.swing.JCheckBox();
        jCheckBox4 = new javax.swing.JCheckBox();
        jCheckBox5 = new javax.swing.JCheckBox();
        jCheckBox6 = new javax.swing.JCheckBox();
        jCheckBox7 = new javax.swing.JCheckBox();
        jCheckBox8 = new javax.swing.JCheckBox();
        jPanel3 = new javax.swing.JPanel();
        jTextField1 = new javax.swing.JTextField();
        jLabel1 = new javax.swing.JLabel();
        jLabel2 = new javax.swing.JLabel();
        jTextField2 = new javax.swing.JTextField();
        jLabel3 = new javax.swing.JLabel();
        jTextField3 = new javax.swing.JTextField();
        jLabel4 = new javax.swing.JLabel();
        jTextField4 = new javax.swing.JTextField();
        jPanel4 = new javax.swing.JPanel();
        jButton1 = new javax.swing.JButton();
        jButton2 = new javax.swing.JButton();
        this.m_events = "";

        this.getContentPane().setLayout( new java.awt.GridBagLayout() );

        this.setTitle( DemoApp.getRes().getString( "Wrapper DemoApp: Event Mails" ) );

        jPanel1.setLayout( new java.awt.GridBagLayout() );

        jPanel2.setBorder( javax.swing.BorderFactory.createTitledBorder( DemoApp.getRes().getString( "EventTypes" ) ) );
        jPanel2.setAlignmentX( 0.0F );
        jPanel2.setAlignmentY( 0.0F );
        jPanel2.setMinimumSize( new java.awt.Dimension( 350, 61 ) );
        jPanel2.setLayout( new java.awt.GridBagLayout() );

        jCheckBox1.setText( "wrapper_start" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        jPanel2.add( jCheckBox1, gridBagConstraints );

        jCheckBox2.setText( "jvm_prelaunch" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        jPanel2.add( jCheckBox2, gridBagConstraints );

        jCheckBox3.setText( "jvm_start" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridwidth = java.awt.GridBagConstraints.REMAINDER;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        jPanel2.add( jCheckBox3, gridBagConstraints );

        jCheckBox4.setText( "jvm_started" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 1;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        jPanel2.add( jCheckBox4, gridBagConstraints );

        jCheckBox5.setText( "jvm_stop" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 1;
        gridBagConstraints.gridy = 1;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        jPanel2.add( jCheckBox5, gridBagConstraints );

        jCheckBox6.setText( "jvm_stopped" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 2;
        gridBagConstraints.gridy = 1;
        gridBagConstraints.gridwidth = java.awt.GridBagConstraints.REMAINDER;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        jPanel2.add( jCheckBox6, gridBagConstraints );

        jCheckBox7.setText( "wrapper_stop" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 2;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        jPanel2.add( jCheckBox7, gridBagConstraints );

        jCheckBox14 = new JCheckBox();
        jCheckBox14.setText( "jvm_restart" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 1;
        gridBagConstraints.gridy = 2;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        jPanel2.add( jCheckBox14, gridBagConstraints );

        jCheckBox9 = new JCheckBox();
        jCheckBox9.setText( "jvm_failed_invocation" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 2;
        gridBagConstraints.gridy = 2;
        gridBagConstraints.gridwidth = java.awt.GridBagConstraints.REMAINDER;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        jPanel2.add( jCheckBox9, gridBagConstraints );

        jCheckBox10 = new JCheckBox();
        jCheckBox10.setText( "jvm_max_failed_invocations" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 3;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        jPanel2.add( jCheckBox10, gridBagConstraints );

        jCheckBox11 = new JCheckBox();
        jCheckBox11.setText( "jvm_kill" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 1;
        gridBagConstraints.gridy = 3;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        jPanel2.add( jCheckBox11, gridBagConstraints );

        jCheckBox12 = new JCheckBox();
        jCheckBox12.setText( "jvm_killed" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 2;
        gridBagConstraints.gridy = 3;
        gridBagConstraints.gridwidth = java.awt.GridBagConstraints.REMAINDER;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        jPanel2.add( jCheckBox12, gridBagConstraints );

        jCheckBox13 = new JCheckBox();
        jCheckBox13.setText( "jvm_unexpected_exit" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 4;
        gridBagConstraints.fill = java.awt.GridBagConstraints.HORIZONTAL;
        jPanel2.add( jCheckBox13, gridBagConstraints );

        jPanel1.add( jPanel2, new java.awt.GridBagConstraints() );

        jPanel3.setBorder( javax.swing.BorderFactory.createTitledBorder( DemoApp.getRes().getString( "Mail Setup" ) ) );
        jPanel3.setLayout( new java.awt.GridBagLayout() );

        jTextField1.setColumns( 20 );

        jTextField1.setText( WrapperManager.getProperties().getProperty( "wrapper.event.default.email.sender" ) );

        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 1;
        gridBagConstraints.gridy = 0;
        jPanel3.add( jTextField1, gridBagConstraints );

        jLabel1.setText( DemoApp.getRes().getString( "Sender:" ) );

        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 0;
        jPanel3.add( jLabel1, gridBagConstraints );

        jCheckBox8.setText( DemoApp.getRes().getString( "Attach Logfile" ) );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 2;
        gridBagConstraints.gridy = 0;
        gridBagConstraints.gridwidth = java.awt.GridBagConstraints.REMAINDER;
        gridBagConstraints.fill = java.awt.GridBagConstraints.BOTH;
        jPanel3.add( jCheckBox8, gridBagConstraints );

        jLabel2.setText( DemoApp.getRes().getString( "Recipient:" ) );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 1;
        jPanel3.add( jLabel2, gridBagConstraints );

        jTextField2.setColumns( 30 );
        jTextField2.setText( WrapperManager.getProperties().getProperty( "wrapper.event.default.email.recipient" ) );
        jTextField2.setToolTipText( DemoApp.getRes().getString( "Please separate mulitple recipients with a semicolon '';''" ) );

        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 1;
        gridBagConstraints.gridy = 1;
        jPanel3.add( jTextField2, gridBagConstraints );

        jLabel3.setText( "Mail Server:" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 2;
        jPanel3.add( jLabel3, gridBagConstraints );

        jTextField3.setColumns( 15 );
        jTextField3.setText( WrapperManager.getProperties().getProperty( "wrapper.event.default.email.smtp.host" ) );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 1;
        gridBagConstraints.gridy = 2;
        jPanel3.add( jTextField3, gridBagConstraints );

        jLabel4.setText( "Port:" );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 2;
        gridBagConstraints.gridy = 2;
        jPanel3.add( jLabel4, gridBagConstraints );

        jTextField4.setColumns( 3 );
        String port = WrapperManager.getProperties().getProperty( "wrapper.event.default.email.smtp.port" );
        jTextField4.setText( port == null ? "25" : port );
        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 3;
        gridBagConstraints.gridy = 2;
        jPanel3.add( jTextField4, gridBagConstraints );

        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 1;
        jPanel1.add( jPanel3, gridBagConstraints );
        jButton1.setText( DemoApp.getRes().getString( "OK" ) );
        jButton1.addActionListener( new ActionListener()
        {
            public void actionPerformed( ActionEvent e )
            {
                if ( jCheckBox1.isSelected() || jCheckBox2.isSelected() || jCheckBox3.isSelected() || jCheckBox4.isSelected() || jCheckBox5.isSelected() || jCheckBox6.isSelected()
                        || jCheckBox7.isSelected() || jCheckBox9.isSelected() || jCheckBox10.isSelected() || jCheckBox11.isSelected() || jCheckBox12.isSelected()
                        || jCheckBox13.isSelected() || jCheckBox14.isSelected() )
                {
                    if ( jTextField1.getText().length() == 0 || jTextField2.getText().length() == 0 || jTextField3.getText().length() == 0 || jTextField4.getText().length() == 0 )
                    {
                        JOptionPane.showMessageDialog( MailDialog.this, DemoApp.getRes().getString( "All text fields need to be filled out!" ), DemoApp.getRes().getString( "Input missing!" ), JOptionPane.OK_OPTION );
                    }
                    else
                    {
                        if ( jCheckBox1.isSelected() )
                        {
                            m_events = m_events.concat( "wrapper.event." + jCheckBox1.getText() + ".email=TRUE " );
                        }
                        if ( jCheckBox2.isSelected() )
                        {
                            m_events = m_events.concat( "wrapper.event." + jCheckBox2.getText() + ".email=TRUE " );
                        }
                        if ( jCheckBox3.isSelected() )
                        {
                            m_events = m_events.concat( "wrapper.event." + jCheckBox3.getText() + ".email=TRUE " );
                        }
                        if ( jCheckBox4.isSelected() )
                        {
                            m_events = m_events.concat( "wrapper.event." + jCheckBox4.getText() + ".email=TRUE " );
                        }
                        if ( jCheckBox5.isSelected() )
                        {
                            m_events = m_events.concat( "wrapper.event." + jCheckBox5.getText() + ".email=TRUE " );
                        }
                        if ( jCheckBox6.isSelected() )
                        {
                            m_events = m_events.concat( "wrapper.event." + jCheckBox6.getText() + ".email=TRUE " );
                        }
                        if ( jCheckBox7.isSelected() )
                        {
                            m_events = m_events.concat( "wrapper.event." + jCheckBox7.getText() + ".email=TRUE " );
                        }
                        if ( jCheckBox9.isSelected() )
                        {
                            m_events = m_events.concat( "wrapper.event." + jCheckBox9.getText() + ".email=TRUE " );
                        }
                        if ( jCheckBox10.isSelected() )
                        {
                            m_events = m_events.concat( "wrapper.event." + jCheckBox10.getText() + ".email=TRUE " );
                        }
                        if ( jCheckBox11.isSelected() )
                        {
                            m_events = m_events.concat( "wrapper.event." + jCheckBox11.getText() + ".email=TRUE " );
                        }
                        if ( jCheckBox12.isSelected() )
                        {
                            m_events = m_events.concat( "wrapper.event." + jCheckBox12.getText() + ".email=TRUE " );
                        }
                        if ( jCheckBox13.isSelected() )
                        {
                            m_events = m_events.concat( "wrapper.event." + jCheckBox13.getText() + ".email=TRUE " );
                        }
                        if ( jCheckBox14.isSelected() )
                        {
                            m_events = m_events.concat( "wrapper.event." + jCheckBox14.getText() + ".email=TRUE " );
                        }

                        if ( jCheckBox8.isSelected() )
                        {
                            m_events = m_events.concat( "wrapper.event.default.email.attach_log=TRUE" );
                        }

                        m_recipient = jTextField2.getText();
                        m_sender = jTextField1.getText();
                        m_server = jTextField3.getText();
                        m_port = Integer.parseInt( jTextField4.getText() );
                        result = 1;
                        MailDialog.this.setVisible( false );
                    }
                }
                else
                {
                    JOptionPane.showMessageDialog( MailDialog.this, DemoApp.getRes().getString( "Please select at least one event!" ), DemoApp.getRes().getString( "Input missing!" ), JOptionPane.OK_OPTION );
                }
            }
        } );
        jPanel4.add( jButton1 );

        jButton2.setText( DemoApp.getRes().getString( "Cancel" ) );
        jButton2.addActionListener( new ActionListener()
        {

            public void actionPerformed( ActionEvent e )
            {
                result = 0;
                MailDialog.this.dispose();

            }
        } );
        jPanel4.add( jButton2 );

        gridBagConstraints = new java.awt.GridBagConstraints();
        gridBagConstraints.gridx = 0;
        gridBagConstraints.gridy = 2;
        jPanel1.add( jPanel4, gridBagConstraints );

        this.getContentPane().add( jPanel1, new java.awt.GridBagConstraints() );
        this.setLocation( this.getParent().getLocation() );
        this.setResizable( false );
        this.setModal( true );
        this.pack();
        // this.setVisible(true);
    }
}
