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

import java.awt.BorderLayout;
import java.awt.Dimension;
import java.awt.Font;
import java.awt.GridBagConstraints;
import java.awt.GridBagLayout;
import java.awt.Insets;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.awt.event.KeyEvent;
import java.awt.event.WindowEvent;
import java.awt.event.WindowListener;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.Reader;
import java.net.URL;
import java.util.Locale;

import javax.swing.JButton;
import javax.swing.JComponent;
import javax.swing.JEditorPane;
import javax.swing.JFrame;
import javax.swing.JLabel;
import javax.swing.JMenu;
import javax.swing.JMenuBar;
import javax.swing.JMenuItem;
import javax.swing.JOptionPane;
import javax.swing.JPanel;
import javax.swing.JScrollPane;
import javax.swing.JTabbedPane;
import javax.swing.UIManager;
import javax.swing.WindowConstants;
import javax.swing.text.html.HTMLDocument;
import org.tanukisoftware.wrapper.WrapperManager;

public class DemoAppMainFrame extends JFrame implements ActionListener, WindowListener
{
    /**
     * Serial Version UID.
     */
    private DemoApp m_this;
    private static final long serialVersionUID = -3847376282833547574L;

    protected JEditorPane getlogTextArea()
    {
        return m_logTextArea;
    }

    protected JEditorPane getDescTextArea()
    {
        return jEditorPane2;
    }

    JScrollPane m_logPane;
    protected JEditorPane m_logTextArea;
    JScrollPane jScrollPane2;
    JEditorPane jEditorPane2;
    JTabbedPane jTabbedPane2;

    DemoAppMainFrame( DemoApp m_this )
    {

        super( DemoApp.getRes().getString( "Wrapper Demo Application" ) );
        this.m_this = m_this;
        init();
        setLocationRelativeTo( null );
        // setSize( 450, 500 );
        setResizable( true );
    }

    private void init()
    {
        JMenuBar menuBar = new JMenuBar();
        JMenu menu = new JMenu( "?" );
        JMenuItem about = new JMenuItem( DemoApp.getRes().getString( "About.." ) );
        // this.setLayout(new BorderLayout());
        about.setActionCommand( "about" );
        about.addActionListener( this );

        addWindowListener( this );

        JMenu jMenu1 = new JMenu();
        jMenu1.setText( DemoApp.getRes().getString( "Test" ) );

        JMenuItem jMenuItem1 = new JMenuItem();
        jMenuItem1.setText( DemoApp.getRes().getString( "Start Test" ) );
        jMenuItem1.setEnabled( false );

        jMenuItem1.setActionCommand( "start" );
        jMenuItem1.addActionListener( this );
        jMenu1.add( jMenuItem1 );

        JMenuItem jMenuItem2 = new JMenuItem();
        jMenuItem2.setText( DemoApp.getRes().getString( "Stop Test" ) );

        jMenuItem2.setActionCommand( "finish" );
        jMenuItem2.addActionListener( this );
        jMenu1.add( jMenuItem2 );

        JMenuItem jMenuItem3 = new JMenuItem();
        jMenuItem3.setAccelerator( javax.swing.KeyStroke.getKeyStroke( java.awt.event.KeyEvent.VK_F4, java.awt.event.InputEvent.ALT_MASK ) );
        jMenuItem3.setText( DemoApp.getRes().getString( "Close" ) );
        jMenuItem3.addActionListener( new ActionListener()
        {

            public void actionPerformed( ActionEvent e )
            {
                m_this.doAction( "finish" );
                WrapperManager.stopAndReturn( 0 );

            }
        } );

        jMenu1.add( jMenuItem3 );
        this.setJMenuBar( menuBar );
        menu.add( about );
        menuBar.add( jMenu1 );
        menuBar.add( menu );
        this.setDefaultCloseOperation( WindowConstants.DO_NOTHING_ON_CLOSE );

        GridBagLayout gridBag1 = new GridBagLayout();
        GridBagLayout gridBag2 = new GridBagLayout();
        GridBagConstraints c1 = new GridBagConstraints();
        GridBagConstraints c2 = new GridBagConstraints();
        // this.setLayout(new FlowLayout());

        // this.setLayout(layout);
        JPanel panel1 = new JPanel();

        // panel1.setBackground( new java.awt.Color( 235, 124, 25 ) );
        panel1.setLayout( gridBag1 );

        JTabbedPane tabbedPane = new JTabbedPane();
        // tabbedPane.setLayout(new BorderLayout());
        tabbedPane.addTab( DemoApp.getRes().getString( "Failure Detections" ), panel1 );
        tabbedPane.setMnemonicAt( 0, KeyEvent.VK_1 );

        buildCommand( panel1, gridBag1, c1, 1, DemoApp.getRes().getString( "Crash" ), "crash", DemoApp.getRes().getString( "Simulate a Application Crash" ) );
        buildCommand( panel1, gridBag1, c1, 1, DemoApp.getRes().getString( "Out of Memory" ), "out_of_mem", DemoApp.getRes().getString( "Simulate a Out Of Memory Error" ) );
        buildCommand( panel1, gridBag1, c1, 1, DemoApp.getRes().getString( "Frozen" ), "frozen", DemoApp.getRes().getString( "Simulate a Frozen JVM" ) );
        buildCommand( panel1, gridBag1, c1, 2, DemoApp.getRes().getString( "Deadlock" ), "deadlock", DemoApp.getRes().getString( "Simulate a Thread Deadlock" ) );

        JPanel panel2 = new JPanel();
        panel2.setLayout( gridBag2 );
        // panel2.setBackground( Color.yellow );
        tabbedPane.addTab( DemoApp.getRes().getString( "Feature Demo" ), panel2 );
        tabbedPane.setMnemonicAt( 1, KeyEvent.VK_2 );
        buildCommand( panel2, gridBag2, c2, 3, DemoApp.getRes().getString( "Email" ), "mail", DemoApp.getRes().getString( "Activates the email functionality" ) );

        buildCommand( panel2, gridBag2, c2, 3, DemoApp.getRes().getString( "WrapperExec" ), "exec", DemoApp.getRes().getString( "Creates a managed Child Process" ) );
        String os = System.getProperty( "os.name" );

        if ( os.indexOf( "Windows" ) >= 0 )
        {

            buildCommand( panel2, gridBag2, c2, 2, DemoApp.getRes().getString( "Customization" ), "customize", DemoApp.getRes().getString( "Creates a customized Binary of the wrapper" ) );
            buildCommand( panel2, gridBag2, c2, 1, DemoApp.getRes().getString( "Service" ), "service", DemoApp.getRes().getString( "Installs and starts this app as Windows Service" ) );
        }
        else
        {
            buildCommand( panel2, gridBag2, c2, 1, DemoApp.getRes().getString( "Daemon" ), "daemon", DemoApp.getRes().getString( "Installs and starts this app as Daemon" ) );
        }

        m_logTextArea = new JEditorPane();
        jEditorPane2 =  new JEditorPane();
        m_logTextArea.setContentType( "text/html;" );
        jEditorPane2.setContentType( "text/html; charset=UTF-8" );
        jEditorPane2.setEditable( false );
        // Set CSS format rule
        Font font = UIManager.getFont( "Label.font" );
        String bodyRule = "body { font-family: " + font.getFamily() + "; " + "font-size: 14pt; }";
        ( ( HTMLDocument )jEditorPane2.getDocument() ).getStyleSheet().addRule( bodyRule );
        m_logTextArea.setEditable( false );
        //setMinimumSize( new java.awt.Dimension( 699, 300 ) );
        jTabbedPane2 = new javax.swing.JTabbedPane();
        m_logPane = new JScrollPane( m_logTextArea );
        jScrollPane2 = new JScrollPane( jEditorPane2 );
        //jTabbedPane2.setPreferredSize( new Dimension( this.getMinimumSize().width, 400 ) );
        jTabbedPane2.setPreferredSize(new java.awt.Dimension(800, 400));
        jTabbedPane2.addTab( DemoApp.getRes().getString( "Description" ), jScrollPane2 );
        jTabbedPane2.addTab( DemoApp.getRes().getString( "Wrapper Output" ), m_logPane );
        getContentPane().setLayout( new BorderLayout() );
        getContentPane().add( tabbedPane, java.awt.BorderLayout.PAGE_START );
        getContentPane().add( jTabbedPane2, java.awt.BorderLayout.CENTER );

        this.setVisible( true );
        this.pack();
        tabbedPane.setMaximumSize( new Dimension( tabbedPane.getMaximumSize().width, tabbedPane.getSize().height ) );

    }

    private void buildCommand( JComponent container, GridBagLayout gridBag, GridBagConstraints c, int level, String label, String command, Object description )
    {
        JButton button = new JButton( label );
        button.setActionCommand( command );

        c.fill = GridBagConstraints.BOTH;
        c.gridwidth = 1;
        c.gridx = 0;
        c.insets = new Insets( 10, 10, 10, 10 );
        gridBag.setConstraints( button, c );
        container.add( button );
        button.addActionListener( this );
        // button.addMouseListener( this );
        // Timer t = new Timer(10, this);

        JButton buttonhelp = new JButton( "?" );
        buttonhelp.setActionCommand( "help" + command );

        c.fill = GridBagConstraints.NONE;
        c.gridwidth = 1;
        c.gridx = 1;
        c.insets = new Insets( 10, 10, 10, 10 );
        gridBag.setConstraints( buttonhelp, c );
        container.add( buttonhelp );
        buttonhelp.addActionListener( this );

        c.fill = GridBagConstraints.NONE;
        c.gridwidth = GridBagConstraints.REMAINDER;

        JComponent desc;
        if ( description instanceof String )
        {
            desc = new JLabel( ( String )description );
        }
        else if ( description instanceof JComponent )
        {
            desc = ( JComponent )description;
        }
        else
        {
            desc = new JLabel( description.toString() );
        }
        c.gridx = 2;
        c.insets = new Insets( 10, 10, 10, 10 );
        gridBag.setConstraints( desc, c );
        container.add( desc );

        if ( level == 2 )
        {
            if ( !WrapperManager.isStandardEdition() )
            {
                button.setEnabled( false );
                button.setToolTipText( DemoApp.getRes().getString( "Requires the Standard Edition." ) );
            }
        }
        else if ( level == 3 )
        {
            if ( !WrapperManager.isProfessionalEdition() )
            {
                button.setEnabled( false );
                button.setToolTipText( DemoApp.getRes().getString( "Requires the Professional Edition." ) );
            }
        }
    }

    String getHTMLDescription( String action )
    {
        try
        {
            String rsname = "html/" + action + "_" + Locale.getDefault().getLanguage() + ".html";
            // System.out.println( Locale.getDefault().getLanguage() +
            // " trying ot open " +rsname);
            URL url = this.getClass().getResource( rsname );
            Reader br;
            try
            {

                br = new BufferedReader( new InputStreamReader( url.openStream(), "utf8" ) );

            }
            catch ( NullPointerException npe1 )
            {
                try
                {
                    // String test = this.getClass().getResource( "html/" +
                    // action + "_en.html" ).getFile();
                    br = new BufferedReader( new InputStreamReader( this.getClass().getResource( "html/" + action + "_en.html" ).openStream(), "utf8" ) );
                }
                catch ( NullPointerException npe2 )
                {
                    return "<html>" + DemoApp.getRes().getString( "No description for {0} found...", action ) + "</html>";
                }
            }
            char[] buffer = new char[ 4096 ];
            String t = "";
            while ( br.read( buffer ) != -1 )
            {
                t = t.concat( new String( buffer ) );
            }
            br.close();
            return t;
        }
        catch ( IOException e )
        {
            e.printStackTrace();
        }
        return "<html>" + DemoApp.getRes().getString( "No description for {0} found...", action ) + "</html>";
    }

    /**************************************************************************
     * ActionListener Methods
     *************************************************************************/
    public void actionPerformed( ActionEvent event )
    {
        String action = event.getActionCommand();
        if ( !action.startsWith( "help" ) )
        {
            if ( action.equals( "start" ) )
            {
                this.getJMenuBar().getMenu( 0 ).getItem( 0 ).setEnabled( false );
                this.getJMenuBar().getMenu( 0 ).getItem( 1 ).setEnabled( true );
                m_this.doAction( action );
            }
            else if ( action.equals( "finish" ) )
            {
                this.getJMenuBar().getMenu( 0 ).getItem( 0 ).setEnabled( true );
                this.getJMenuBar().getMenu( 0 ).getItem( 1 ).setEnabled( false );
                m_this.doAction( action );
            }

            else if ( action.equals( "about" ) )
            {
                // Create the mask.
                createAboutScreen();
            }
            else if ( action.equals( "daemon" ) )
            {
                jEditorPane2.setText( getHTMLDescription( action ) );
                jEditorPane2.setCaretPosition( 0 );
                // Create the mask.

                this.jTabbedPane2.setSelectedIndex( 1 );
                m_this.doAction( action );
            }
            else
            {
                jEditorPane2.setText( getHTMLDescription( action ) );
                jEditorPane2.setCaretPosition( 0 );
                this.jTabbedPane2.setSelectedIndex( 1 );

                m_this.doAction( action );
            }
        }
        else
        {
            jEditorPane2.setText( getHTMLDescription( action.substring( 4 ) ) );
            // System.out.println(getHTMLDescription( action.substring( 4 ) ));
            jEditorPane2.setCaretPosition( 0 );
            this.jTabbedPane2.setSelectedIndex( 0 );
        }
    }

    private void createAboutScreen()
    {
        new AboutDialog( this ).setVisible( true );
    }

    /**************************************************************************
     * WindowListener Methods
     *************************************************************************/
    public void windowOpened( WindowEvent e )
    {
    }

    public void windowClosing( WindowEvent e )
    {
        if ( !m_this.isTestCaseRunning()
                || JOptionPane.showConfirmDialog( this, DemoApp.getRes().getString( "A test case is still running.\nDo you really want to exit and stop this one??" ) ) == JOptionPane.YES_OPTION )
        {
            System.out.println( DemoApp.getRes().getString( "Stopping..." ) );
            m_this.doAction( "finish" );
            WrapperManager.stopAndReturn( 0 );
        }
    }

    public void windowClosed( WindowEvent e )
    {
    }

    public void windowIconified( WindowEvent e )
    {
    }

    public void windowDeiconified( WindowEvent e )
    {
    }

    public void windowActivated( WindowEvent e )
    {
    }

    public void windowDeactivated( WindowEvent e )
    {
    }
}
