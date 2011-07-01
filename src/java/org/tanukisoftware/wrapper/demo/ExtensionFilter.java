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

import java.io.File;

import javax.swing.filechooser.FileFilter;

class ExtensionFilter extends FileFilter
{
    String description;

    String extensions[];

    public ExtensionFilter( String description, String extension )
    {
        this( description, new String[] { extension } );
    }

    public ExtensionFilter( String description, String extensions[] )
    {
        if ( description == null )
        {
            this.description = extensions[0];
        }
        else
        {
            this.description = description;
        }
        this.extensions = ( String[] )extensions.clone();
        toLower( this.extensions );
    }

    private void toLower( String array[] )
    {
        for ( int i = 0, n = array.length; i < n; i++ )
        {
            array[i] = array[i].toLowerCase();
        }
    }

    public String getDescription()
    {
        return description;
    }

    public boolean accept( File file )
    {
        if ( file.isDirectory() )
        {
            return true;
        }
        else
        {
            String path = file.getAbsolutePath().toLowerCase();
            for ( int i = 0, n = extensions.length; i < n; i++ )
            {
                String extension = extensions[i];
                if ( ( path.endsWith( extension ) && ( path.charAt( path.length() - extension.toLowerCase().length() - 1 ) ) == '.' ) )
                {
                    return true;
                }
            }
        }
        return false;
    }
}
