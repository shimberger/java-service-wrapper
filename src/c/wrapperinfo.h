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

#ifndef _WRAPPERINFO_H
#define _WRAPPERINFO_H
#ifdef WIN32
#include <tchar.h>
#endif



extern TCHAR *wrapperVersionRoot;
extern TCHAR *wrapperVersion;
extern TCHAR *wrapperBits;
extern TCHAR *wrapperArch;
extern TCHAR *wrapperOS;
extern TCHAR *wrapperReleaseDate;
extern TCHAR *wrapperReleaseTime;
extern TCHAR *wrapperBuildDate;
extern TCHAR *wrapperBuildTime;

#endif
