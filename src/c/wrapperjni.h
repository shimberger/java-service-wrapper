/*
 * Copyright (c) 1999, 2011 Tanuki Software, Ltd.
 * http://www.tanukisoftware.com
 * All rights reserved.
 *
 * This software is the proprietary information of Tanuki Software.
 * You shall use it only in accordance with the terms of the
 * license agreement you entered into with Tanuki Software.
 * http://wrapper.tanukisoftware.com/doc/english/licenseOverview.html
 *
 *
 * Portions of the Software have been derived from source code
 * developed by Silver Egg Technology under the following license:
 *
 * Copyright (c) 2001 Silver Egg Technology
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sub-license, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 */

#ifndef _WRAPPERJNI_H
#define _WRAPPERJNI_H

#include "org_tanukisoftware_wrapper_WrapperManager.h"

#ifndef TRUE
#define TRUE -1
#endif

#undef gettext
#ifdef WIN32
#include <tchar.h>
#endif
#include <locale.h>
#include "wrapper_i18n.h"


#ifndef FALSE
#define FALSE 0
#endif

#ifdef WIN32
#define STDOUT_FILENO 1
#define STDERR_FILENO 2
#endif

#define strcmpIgnoreCase(str1, str2) _tcsicmp(str1, str2)


/** Flag to keep track of whether StdOut has been redirected. */
extern int redirectedStdOut;

/** Flag to keep track of whether StdErr has been redirected. */
extern int redirectedStdErr;

extern TCHAR* getLastErrorText();
extern void throwJNIError(JNIEnv *env, const TCHAR *message);
extern const char utf8ClassJavaLangString[];
extern const char utf8MethodInit[];
extern const char utf8SigJ[];
extern const char utf8VrV[];
extern const char utf8ClassJavaLangOutOfMemoryError[];
extern char *utf8ClassJavaLangSystem;
extern char *utf8MethodGetProperties;
extern char *utf8SigVrLjavaUtilProperties;

#ifdef WIN32
#else
extern char *utf8ClassOrgTanukisoftwareWrapperWrapperUNIXUser;
extern char *utf8MethodSetGroup;
extern char *utf8MethodAddGroup;
extern char *utf8SigIIStringStringStringStringrV;
extern char *utf8SigIStringrV;
extern char *utf8ClassOrgTanukisoftwareWrapperWrapperProcess;
extern char *utf8m_ptr;
extern char* utf8SigBLJavaLangStringrV;
extern char* utf8MethodSendCommand;
extern char *utf8javaIOIOException;  /*java/io/IOException*/
extern char* utf8ClassJavaLangError;
extern char* utf8javaLangNullPointerException;
extern char* utf8javalangIllegalArgumentException;
extern char* utf8javalangUnsatisfiedLinkError;
extern jstring JNU_NewStringFromNativeChar(JNIEnv *env, const char *str);

#endif

extern void initCommon();

extern int getLastError();
extern void throwOutOfMemoryError(JNIEnv *env, const TCHAR* locationCode);

extern int wrapperJNIDebugging;
extern int wrapperLockControlEventQueue();
extern int wrapperReleaseControlEventQueue();
extern void wrapperJNIHandleSignal(int signal);
extern void throwThrowable(JNIEnv *env, char *throwableClassName, const TCHAR *lpszFmt, ...);

/**
 * Create a jstring from a Wide Char string.  The jstring must be freed up by caller.
 *
 * @param env The current JNIEnv.
 * @param strW The Wide string to convert.
 *
 * @return The new jstring or NULL if there were any exceptions thrown.
 */
extern jstring JNU_NewStringNative(JNIEnv *env, const TCHAR *strW);

extern TCHAR* JNU_GetStringNativeChars(JNIEnv *env, jstring jstr);

extern void JNU_SetByteArrayRegion(JNIEnv *env, jbyteArray *jarray, jsize start, jsize len, const TCHAR *buffer);

/**
 * Looks up a System property and sets its value in the propertyValue parameter.
 *
 * It is the responsibility of the caller to free up the propertyValue buffer if it is non-NULL.
 *
 * @param env Current JNIEnv.
 * @param propertyName Name of the property.
 * @param propertyValue Value of the property, or NULL if it was not set.
 *
 * @return TRUE if there were any problems, FALSE if successful.  NULL values will still be successful.
 */
extern int getSystemProperty(JNIEnv *env, const TCHAR *propertyName, TCHAR **propertyValue, int encodeNative);
#endif
