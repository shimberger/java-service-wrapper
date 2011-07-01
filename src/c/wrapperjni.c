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

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <fcntl.h>
#ifdef WIN32
 #include <windows.h>
 #include <tchar.h>
 #include <io.h>
 #define dup2 _dup2
#endif
#include "wrapper_i18n.h"
#include "wrapperinfo.h"
#include "wrapperjni.h"

int wrapperJNIDebugging = JNI_FALSE;

#define CONTROL_EVENT_QUEUE_SIZE 10
int controlEventQueue[CONTROL_EVENT_QUEUE_SIZE];
int controlEventQueueLastReadIndex = 0;
int controlEventQueueLastWriteIndex = 0;


/** Flag to keep track of whether StdOut has been redirected. */
int redirectedStdOut = FALSE;

/** Flag to keep track of whether StdErr has been redirected. */
int redirectedStdErr = FALSE;

const char utf8ClassJavaLangString[] = {106, 97,118, 97, 47, /* java/ */
                                        108, 97,110,103, 47, /* lang/ */
                                        83,116,114,105,110,103, 0}; /* "java/lang/String" */
const char utf8MethodInit[] = {60,105,110,105,116, 62, 0}; /* "<init>" */
const char utf8Sig_BrV[] = {40, 91, 66, 41, 86, 0}; /* "([B)V" */
const char utf8Sigr_B [] = { 40, 41, 91, 66, 0}; /* "()[B") */
const char utf8MethodGetBytes[] = {103, 101, 116, 66, 121, 116, 101, 115, 0}; /* getBytes */
const char utf8ClassJavaLangOutOfMemoryError[] = {106, 97,118, 97, 47, /* java/ */
                                                  108, 97,110, 103, 47, /* lang/ */
                                                  79, 117, 116, 79, 102, 77, 101, 109, 111, 114, 121, 69, 114, 114, 111, 114, 0}; /* OutOfMemoryError */
const char utf8SigJ[] = {74, 0}; /* "J" */
const char utf8VrV[] = {40, 41, 86, 0}; /* "()V" */
const char utf8SigmId[] = { 109, 95, 73, 100, 0}; /* "m_Id" */
const char utf8SigOrgTanukisoftwareWrapperWrapperResources[] = {111, 114, 103, 47, /* org/ */
                                                                116, 97, 110, 117, 107, 105, 115, 111, 102, 116, 119, 97, 114, 101, 47, /* tanukisoftware/ */
                                                                119, 114, 97, 112, 112, 101, 114, 47, /* wrapper/ */
                                                                87,  114, 97, 112, 112, 101, 114, 82, 101, 115, 111, 117, 114, 99, 101, 115, 0}; /* "WrapperResources" */
/*
 * For UTF8 constants, '_' in the name means an array, 'r' preceeds the return
 *  portion of a method declaration, 'V' is Void.  The rest is like the
 *  Java format.
 */
char *utf8ClassJavaLangError;
#ifdef WIN32
#else
char *utf8ClassOrgTanukisoftwareWrapperWrapperUNIXUser;
char *utf8MethodSetGroup;
char *utf8MethodAddGroup;
char *utf8SigIIStringStringStringStringrV;
char *utf8SigIStringrV;
#endif

char *utf8SigLjavaLangStringrV;
char *utf8ClassJavaLangSystem;
char *utf8MethodGetProperties;
char *utf8SigVrLjavaUtilProperties;
char *utf8MethodGetProperty;
char *utf8SigLjavaLangStringrLjavaLangString;

/**
 * Create an error message from GetLastError() using the
 *  FormatMessage API Call...
 */
#ifdef WIN32
TCHAR lastErrBuf[1024];
TCHAR* getLastErrorText() {
    DWORD dwRet;
    LPTSTR lpszTemp = NULL;

    dwRet = FormatMessage(FORMAT_MESSAGE_ALLOCATE_BUFFER |
                           FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_ARGUMENT_ARRAY,
                           NULL,
                           GetLastError(),
                           LANG_NEUTRAL,
                           (LPTSTR)&lpszTemp,
                           0,
                           NULL);

    /* supplied buffer is not long enough */
    if (!dwRet || ((long)1023 < (long)dwRet+14)) {
        lastErrBuf[0] = TEXT('\0');
    } else {
        lpszTemp[lstrlen(lpszTemp)-2] = TEXT('\0');  /*remove cr and newline character */
        _sntprintf( lastErrBuf, 1024, TEXT("%s (0x%x)"), lpszTemp, GetLastError());
    }

    if (lpszTemp) {
        GlobalFree((HGLOBAL) lpszTemp);
    }

    return lastErrBuf;
}
int getLastError() {
    return GetLastError();
}
#else
TCHAR* getLastErrorText() {

#ifdef UNICODE
    char* c;
    TCHAR* t;
    size_t req;
    c = strerror(errno);
    req = mbstowcs(NULL, c, 0);
    t = malloc(req);
    if (!t) {
        return NULL;
    }
    mbstowcs(t, c, req);
    return t;

#else
    return strerror(errno);
#endif
}
int getLastError() {
    return errno;
}
#endif

/**
 * Create a jstring from a Wide Char string.  The jstring must be freed up by caller.
 *
 * @param env The current JNIEnv.
 * @param strW The Wide string to convert.
 *
 * @return The new jstring or NULL if there were any exceptions thrown.
 */
jstring JNU_NewStringNative(JNIEnv *env, const TCHAR *strW) {
    jstring result;

    jclass jClassString;
    jmethodID MID_String_init;
    jbyteArray jBytes;
    size_t len;
    char* msgMB;
#ifdef UNICODE
    int size;
#endif
#ifdef UNICODE
    /* We need to special case empty strings as some of the functions don't work correctly for them. */
    len = _tcslen(strW);
    if (len > 0) {
 #ifdef WIN32
        size = WideCharToMultiByte(CP_UTF8, 0, strW, -1, NULL, 0, NULL, NULL);
        if (size == 0) {
            /* Failed. */
            _tprintf(TEXT("WrapperJNI Warn: Failed to convert string \"%s\": %s\n"), strW, GetLastError()); fflush(NULL);
            return NULL;
        }
        msgMB = malloc(sizeof(char) * size);
        if (!msgMB) {
            throwOutOfMemoryError(env, TEXT("JNSN1"));
            return NULL;
        }
        WideCharToMultiByte(CP_UTF8, 0, strW, -1, msgMB, size, NULL, NULL);
        result = (*env)->NewStringUTF(env, msgMB);
        free(msgMB);
        return result;
 #else
        size = wcstombs(NULL, strW, 0) + 1;
        msgMB = malloc(sizeof(char) * size);
        if (!msgMB) {
            throwOutOfMemoryError(env, TEXT("JNSN2"));
            return NULL;
        }
        wcstombs(msgMB, strW, size);
 #endif
    } else {
        /* Empty string. */
        msgMB = malloc(sizeof(char) * 1);
        if (!msgMB) {
            throwOutOfMemoryError(env, TEXT("JNSN3"));
            return NULL;
        }
        msgMB[0] = '\0';
    }
#else
    msgMB = (TCHAR*)strW;
#endif
    result = NULL;
    if ((*env)->EnsureLocalCapacity(env, 2) < 0) {
        throwOutOfMemoryError(env, TEXT("JNSN4"));
#ifdef UNICODE
        if (msgMB) {
            free(msgMB);
        }
#endif
        return NULL; /* out of memory error */
    }
    len = strlen(msgMB);
    if ((jBytes = (*env)->NewByteArray(env, (jsize)len))) {
        (*env)->SetByteArrayRegion(env, jBytes, 0, (jsize)len, (jbyte*)msgMB);
        if ((jClassString = (*env)->FindClass(env, utf8ClassJavaLangString))) {
            if ((MID_String_init = (*env)->GetMethodID(env, jClassString, utf8MethodInit, utf8Sig_BrV))) {
                result = (*env)->NewObject(env, jClassString, MID_String_init, jBytes);
            } else {
                /* Exception Thrown */
            }

            (*env)->DeleteLocalRef(env, jClassString);
        } else {
            /* Exception Thrown */
        }

        (*env)->DeleteLocalRef(env, jBytes);
    } else {
        /* Exception Thrown */
    }

#ifdef UNICODE
    if (msgMB) {
        free(msgMB);
    }
#endif

    return result;
}


#ifdef WIN32
/* So far this function is only used by windows. if we want to use it for unix as well, first
   provide correct wchar handling... */
void JNU_SetByteArrayRegion(JNIEnv *env, jbyteArray* jarray, jsize start, jsize len, const TCHAR *buffer) {

    char* msg;
#if defined(UNICODE) && defined(WIN32)
    int size;
    size = WideCharToMultiByte(CP_OEMCP, 0, buffer, -1, NULL, 0, NULL, NULL);
    msg = malloc(size);
    if (!msg) {
        throwOutOfMemoryError(env, TEXT("JSBAR1"));
        return;
    }
    WideCharToMultiByte(CP_OEMCP,0, buffer,-1, msg, size, NULL, NULL);
#else
     msg = (TCHAR*) buffer;
#endif
    (*env)->SetByteArrayRegion(env, *jarray, start, len, (jbyte*) msg);
#if defined(UNICODE) && defined(WIN32)
    free(msg);
#endif
}
#endif

/**
 * Converts a jstring into a newly malloced TCHAR array.
 *
 * @param end The JNIEnv.
 * @param jstr The jstring.
 *
 * @return The requested Wide String, or NULL if there was a problem.  It is
 *         the responsibility of the caller to free up the returned string.
 */
TCHAR *JNU_GetStringNativeChars(JNIEnv *env, jstring jstr) {
    jbyteArray jByteArrayBytes = 0;
    jthrowable jThrowableE;
    jclass jClassString = NULL;
    jmethodID jMethodIdStringGetBytes = NULL;
#ifdef UNICODE
    int size;
    TCHAR* tresult;
#endif
    char *result = 0;

    if ((*env)->EnsureLocalCapacity(env, 2) < 0) {
        throwOutOfMemoryError(env, TEXT("GSNC1"));
        return NULL; /* out of memory error */
    }
    if ((jClassString = (*env)->FindClass(env, utf8ClassJavaLangString)) != NULL &&
            (jMethodIdStringGetBytes = (*env)->GetMethodID(env, jClassString, utf8MethodGetBytes, utf8Sigr_B)) != NULL){
        jByteArrayBytes = (*env)->CallObjectMethod(env, jstr, jMethodIdStringGetBytes);
        jThrowableE = (*env)->ExceptionOccurred(env);
        if (!jThrowableE) {
            jint len = (*env)->GetArrayLength(env, jByteArrayBytes);
            result = (char *)malloc(sizeof(char) * (len + 1));
            if (!result) {
                throwOutOfMemoryError(env, TEXT("GSNC2"));
            } else {
                (*env)->GetByteArrayRegion(env, jByteArrayBytes, 0, len, (jbyte *)result);
                result[len] = 0; /* NULL-terminate */
            }
        } else {
            (*env)->DeleteLocalRef(env, jThrowableE);
        }
        (*env)->DeleteLocalRef(env, jByteArrayBytes);
        (*env)->DeleteLocalRef(env, jClassString);
    }
#ifdef UNICODE
#ifdef WIN32
    size = MultiByteToWideChar(CP_OEMCP, 0, result, -1, NULL, 0);
    tresult = malloc(size*sizeof(LPWSTR));
    if (!tresult) {
        free(result);
        throwOutOfMemoryError(env, TEXT("GSNC3"));
        return NULL;
    }
    MultiByteToWideChar(CP_OEMCP, 0, result,-1, tresult, size);
    free(result);
    return tresult;
#else
    size = mbstowcs(NULL, result, 0) + 1;
    size *= sizeof(TCHAR);
    tresult = malloc(size);
    if (!tresult) {
        free(result);
        throwOutOfMemoryError(env, TEXT("GSNC3"));
        return NULL;
    }
    mbstowcs(tresult, result, size);
    free(result);
    return tresult;
#endif
#else
    return result;
#endif
}

jstring JNU_NewStringFromNativeChar(JNIEnv *env, const char *str) {
    jstring result;
    jclass jClassString;
    jmethodID MID_String_init;
    jbyteArray bytes = 0;
    size_t len;


    result = NULL;
    if ((*env)->EnsureLocalCapacity(env, 2) < 0) {
        throwOutOfMemoryError(env, TEXT("JNSN2"));
        return NULL; /* out of memory error */
    }
    len = strlen(str);
    bytes = (*env)->NewByteArray(env, (jsize)len);
    if (bytes != NULL) {
        (*env)->SetByteArrayRegion(env, bytes, 0, (jsize)len,(jbyte*) str);
        jClassString = (*env)->FindClass(env, utf8ClassJavaLangString);
        MID_String_init = (*env)->GetMethodID(env, jClassString, utf8MethodInit, utf8Sig_BrV);
        result = (*env)->NewObject(env, jClassString, MID_String_init, bytes);
        (*env)->DeleteLocalRef(env, bytes);

    } /* else fall through */
    return result;
}

/**
 * Returns a new buffer containing the UTF8 characters for the specified native string.
 *
 * It is the responsibility of the caller to free the returned buffer.
 */
char *getUTF8Chars(JNIEnv *env, const char *nativeChars) {
    jstring js;
    jsize jlen;
    const char *stringChars;
    jboolean isCopy;
    char *utf8Chars = NULL;

    js = JNU_NewStringFromNativeChar(env, nativeChars);
    if (js != NULL) {
        jlen = (*env)->GetStringUTFLength(env, js);
        utf8Chars = malloc(jlen + 1);
        if (!utf8Chars) {
            throwOutOfMemoryError(env, TEXT("GUTFC1"));
            return NULL;
        }

        stringChars = ((*env)->GetStringUTFChars(env, js, &isCopy));
        if (stringChars != NULL) {
            memcpy(utf8Chars, stringChars, jlen);
            utf8Chars[jlen] = '\0';

            (*env)->ReleaseStringUTFChars(env, js, stringChars);
        } else {
            throwOutOfMemoryError(env, TEXT("GUTFC2"));
            free(utf8Chars);
            return NULL;
        }
        (*env)->DeleteLocalRef(env, js);
    }
    return utf8Chars;
}

void initUTF8Strings(JNIEnv *env) {
    /* Now do the rest of the strings using our helper function. */
    /*utf8ClassJavaLangOutOfMemoryError = getUTF8Chars(env, "java/lang/OutOfMemoryError"); */
    utf8ClassJavaLangError = getUTF8Chars(env, "java/lang/Error");
    /* utf8MethodGetBytes = getUTF8Chars(env, "getBytes"); */
    utf8SigLjavaLangStringrV = getUTF8Chars(env, "(Ljava/lang/String;)V");
    /* utf8Sigr_B = getUTF8Chars(env, "()[B"); */
#ifdef WIN32
#else
    utf8ClassOrgTanukisoftwareWrapperWrapperUNIXUser = getUTF8Chars(env, "org/tanukisoftware/wrapper/WrapperUNIXUser");
    utf8MethodSetGroup = getUTF8Chars(env, "setGroup");
    utf8MethodAddGroup = getUTF8Chars(env, "addGroup");
    utf8SigIIStringStringStringStringrV = getUTF8Chars(env, "(IILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
    utf8SigIStringrV = getUTF8Chars(env, "(ILjava/lang/String;)V");
#endif
    utf8ClassJavaLangSystem = getUTF8Chars(env, "java/lang/System");
    utf8MethodGetProperties = getUTF8Chars(env, "getProperties");
    utf8SigVrLjavaUtilProperties = getUTF8Chars(env, "()Ljava/util/Properties;");
    utf8MethodGetProperty = getUTF8Chars(env, "getProperty");
    utf8SigLjavaLangStringrLjavaLangString = getUTF8Chars(env, "(Ljava/lang/String;)Ljava/lang/String;");
}

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
int getSystemProperty(JNIEnv *env, const TCHAR *propertyName, TCHAR **propertyValue, int encodeNative) {
    int result;
    jclass jClassSystem;
    jmethodID jMethodIdGetProperty;
    jstring jStringKeyPropName;
    jstring jStringKeyValue;
    TCHAR *keyChars;

    /* Initialize the propertyValue to point to NULL in case we fail. */
    *propertyValue = NULL;

    if ((jClassSystem = (*env)->FindClass(env, utf8ClassJavaLangSystem)) != NULL) {
        if ((jMethodIdGetProperty = (*env)->GetStaticMethodID(env, jClassSystem, utf8MethodGetProperty, utf8SigLjavaLangStringrLjavaLangString)) != NULL) {
            jStringKeyPropName = JNU_NewStringNative(env, propertyName);
            if (jStringKeyPropName) {
                if ((jStringKeyValue = (jstring)(*env)->CallStaticObjectMethod(env, jClassSystem, jMethodIdGetProperty, jStringKeyPropName)) != NULL) {
                    /* Collect the value. */
                    if (!encodeNative) {
                        keyChars = JNU_GetStringNativeChars(env, jStringKeyValue);
                        if (keyChars) {
                            *propertyValue = malloc(sizeof(TCHAR) * (_tcslen(keyChars) + 1));
                            if (!*propertyValue) {
                                throwOutOfMemoryError(env, TEXT("GSP1"));
                                result = TRUE;
                                } else {
                                _tcsncpy(*propertyValue, keyChars, _tcslen(keyChars) + 1);
                                result = FALSE;
                            }
                            if (keyChars) {
                                free(keyChars);
                            }
                        } else {
                            /* Exception Thrown */
                            result = TRUE;
                        }
                        (*env)->DeleteLocalRef(env, jStringKeyValue);
                    } else {
                        keyChars = (TCHAR*)(*env)->GetStringUTFChars(env, jStringKeyValue, NULL);
                        if (keyChars) {
                            *propertyValue = malloc(strlen((char*)keyChars) + 1);
                            if (!*propertyValue) {
                                throwOutOfMemoryError(env, TEXT("GSP2"));
                                result = TRUE;
                            } else {
                                strncpy((char*)*propertyValue, (char*)keyChars, strlen((char*)keyChars) + 1);
                                result = FALSE;
                            }
                            (*env)->ReleaseStringUTFChars(env, jStringKeyValue, (const char *)keyChars);
                        } else {
                            /* Exception Thrown */
                            result = TRUE;
                        }
                        (*env)->DeleteLocalRef(env, jStringKeyValue);
                    }
                } else {
                    /* Property was not set. */
                    result = FALSE;
                }

                (*env)->DeleteLocalRef(env, jStringKeyPropName);
            } else {
                result = TRUE;
            }
        } else {
            result = TRUE;
        }

        (*env)->DeleteLocalRef(env, jClassSystem);
    } else {
        result = TRUE;
    }

    return result;
}

/**
 * Do common initializaion.
 */
void initCommon(JNIEnv *env, jclass jClassWrapperManager) {
    TCHAR* outfile, *errfile;
    int outfd, errfd, mode, options;

#ifdef WIN32
    mode = _S_IWRITE;
    options = _O_WRONLY | _O_APPEND | _O_CREAT;
#else
    mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
    options = O_WRONLY | O_APPEND | O_CREAT;
#endif
    initUTF8Strings(env);

    if (getSystemProperty(env, TEXT("wrapper.java.errfile"), &errfile, FALSE)) {
        /* Failed */
        return;
    }
    if (errfile) {
        _ftprintf(stderr, TEXT("WrapperJNI: Redirecting %s to file %s...\n"), TEXT("StdErr"), errfile); fflush(NULL);
        if (((errfd = _topen(errfile, options, mode)) == -1) || (dup2(errfd, STDERR_FILENO) == -1)) {
            _ftprintf(stderr, TEXT("WrapperJNI: Failed to redirect %s to file %s  (Err: %s)\n"), TEXT("StdErr"), errfile, getLastErrorText()); fflush(NULL);
            return;
        } else {
            redirectedStdErr = TRUE;
        }
    }
    if (getSystemProperty(env, TEXT("wrapper.java.outfile"), &outfile, FALSE)) {
        /* Failed */
        return;
    }
    if (outfile) {
        _tprintf(TEXT("WrapperJNI: Redirecting %s to file %s...\n"), TEXT("StdOut"), outfile); fflush(NULL);
        if (((outfd = _topen(outfile, options, mode)) == -1) || (dup2(outfd, STDOUT_FILENO) == -1)) {
            _tprintf(TEXT("WrapperJNI: Failed to redirect %s to file %s  (Err: %s)\n"), TEXT("StdOut"), errfile, getLastErrorText()); fflush(NULL);
            return;
        } else {
            redirectedStdOut = TRUE;
        }
    }
}

void throwThrowable(JNIEnv *env, char *throwableClassName, const TCHAR *lpszFmt, ...) {
    va_list vargs;
    int messageBufferSize = 0;
    TCHAR *messageBuffer = NULL;
    int count;
    jclass jThrowableClass;
    jmethodID constructor;
    jstring jMessageBuffer;
    jobject jThrowable;

    do {
        if (messageBufferSize == 0) {
            /* No buffer yet. Allocate one to get started. */
            messageBufferSize = 100;
            messageBuffer = (TCHAR*)malloc( messageBufferSize * sizeof(TCHAR));
            if (!messageBuffer) {
                throwOutOfMemoryError(env, TEXT("TT1"));
                return;
            }
        }

        /* Try writing to the buffer. */
        va_start(vargs, lpszFmt);

        count = _vsntprintf(messageBuffer, messageBufferSize, lpszFmt, vargs);

        va_end(vargs);
        if ((count < 0) || (count >= (int)messageBufferSize)) {
            /* If the count is exactly equal to the buffer size then a null TCHAR was not written.
             *  It must be larger.
             * Windows will return -1 if the buffer is too small. If the number is
             *  exact however, we still need to expand it to have room for the null.
             * UNIX will return the required size. */

            /* Free the old buffer for starters. */
            free(messageBuffer);

            /* Decide on a new buffer size. */
            if (count <= (int)messageBufferSize) {
                messageBufferSize += 50;
            } else if (count + 1 <= (int)messageBufferSize + 50) {
                messageBufferSize += 50;
            } else {
                messageBufferSize = count + 1;
            }

            messageBuffer = (TCHAR*)malloc(messageBufferSize * sizeof(TCHAR));
            if (!messageBuffer) {
                throwOutOfMemoryError(env, TEXT("TT2"));
                messageBufferSize = 0;
                return;
            }

            /* Always set the count to -1 so we will loop again. */
            count = -1;
        }
    } while (count < 0);

    /* We have the messageBuffer */
    if ((jThrowableClass = (*env)->FindClass(env, throwableClassName)) != NULL) {
        if ((constructor = (*env)->GetMethodID(env, jThrowableClass, utf8MethodInit, utf8SigLjavaLangStringrV)) != NULL) {
            if ((jMessageBuffer = JNU_NewStringNative(env, messageBuffer)) != NULL) {
                if ((jThrowable = (*env)->NewObject(env, jThrowableClass, constructor, jMessageBuffer)) != NULL) {
                    if ((*env)->Throw(env, jThrowable)) {
                        _tprintf(TEXT("WrapperJNI Error: Unable to throw %s with message: %s"), throwableClassName, messageBuffer); fflush(NULL);
                    }
                    (*env)->DeleteLocalRef(env, jThrowable);
                } else {
                    _tprintf(TEXT("WrapperJNI Error: Unable to create instance of class, '%s' to report exception: %s"),
                        throwableClassName, messageBuffer); fflush(NULL);
                }
                (*env)->DeleteLocalRef(env, jMessageBuffer);
            } else {
                _tprintf(TEXT("WrapperJNI Error: Unable to create string to report '%s' exception: %s"),
                    throwableClassName, messageBuffer); fflush(NULL);
            }
        } else {
            _tprintf(TEXT("WrapperJNI Error: Unable to find constructor for class, '%s' to report exception: %s"),
                throwableClassName, messageBuffer); fflush(NULL);
        }
        (*env)->DeleteLocalRef(env, jThrowableClass);
    } else {
        _tprintf(TEXT("WrapperJNI Error: Unable to load class, '%s' to report exception: %s"),
            throwableClassName, messageBuffer); fflush(NULL);
    }
    free(messageBuffer);
}

/**
 * Throws an OutOfMemoryError.
 *
 * @param env The current JNIEnv.
 * @param locationCode The locationCode to help tell where the error happened.
 */
void throwOutOfMemoryError(JNIEnv *env, const TCHAR* locationCode) {
    throwThrowable(env, (char*)utf8ClassJavaLangOutOfMemoryError, TEXT("Out of memory (%s)"), locationCode);

    _tprintf(TEXT("WrapperJNI Error: Out of memory (%s)\n"), locationCode);
    fflush(NULL);
}

void throwJNIError(JNIEnv *env, const TCHAR *message) {
    jclass exceptionClass;
    jmethodID constructor;
    jstring jMessage;
    jobject exception;
    char *className = "org/tanukisoftware/wrapper/WrapperJNIError";

    if ((exceptionClass = (*env)->FindClass(env, className)) != NULL) {
        /* Look for the constructor. Ignore failures. */
        if ((constructor = (*env)->GetMethodID(env, exceptionClass, "<init>", "([B)V")) != NULL) {
            jMessage = (*env)->NewByteArray(env, (jsize)_tcslen(message));
            /* The 1.3.1 jni.h file does not specify the message as const.  The cast is to
             *  avoid compiler warnings trying to pass a (const TCHAR *) as a (TCHAR *). */
            jMessage = JNU_NewStringNative(env, message);

            exception = (*env)->NewObject(env, exceptionClass, constructor, jMessage);

            if ((*env)->Throw(env, exception)) {
                _tprintf(TEXT("WrapperJNI Error: Unable to throw WrapperJNIError with message: %s"), message);
                fflush(NULL);
            }

            (*env)->DeleteLocalRef(env, jMessage);
            (*env)->DeleteLocalRef(env, exception);
        }

        (*env)->DeleteLocalRef(env, exceptionClass);
    } else {
        _tprintf(TEXT("WrapperJNI Error: Unable to load class, '%s' to report exception: %s"),
            className, message);
        fflush(NULL);
    }
}

void wrapperJNIHandleSignal(int signal) {
    if (wrapperLockControlEventQueue()) {
        /* Failed.  Should have been reported. */
        _tprintf(TEXT("WrapperJNI Error: Signal %d trapped, but ignored.\n"), signal);
        fflush(NULL);
        return;
    }
#ifdef _DEBUG
    _tprintf(TEXT(" Queue Write 1 R:%d W:%d E:%d\n"), controlEventQueueLastReadIndex, controlEventQueueLastWriteIndex, signal);
    fflush(NULL);
#endif
    controlEventQueueLastWriteIndex++;
    if (controlEventQueueLastWriteIndex >= CONTROL_EVENT_QUEUE_SIZE) {
        controlEventQueueLastWriteIndex = 0;
    }
    controlEventQueue[controlEventQueueLastWriteIndex] = signal;
#ifdef _DEBUG
    _tprintf(TEXT(" Queue Write 2 R:%d W:%d\n"), controlEventQueueLastReadIndex, controlEventQueueLastWriteIndex);
    fflush(NULL);
#endif

    if (wrapperReleaseControlEventQueue()) {
        /* Failed.  Should have been reported. */
        return;
    }
}


/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeGetLibraryVersion
 * Signature: ()Ljava/lang/String;
 */
JNIEXPORT jstring JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeGetLibraryVersion(JNIEnv *env, jclass clazz) {
    jstring version;
    version = JNU_NewStringNative(env, wrapperVersion);
    return version;
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeIsProfessionalEdition
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeIsProfessionalEdition(JNIEnv *env, jclass clazz) {
    return JNI_FALSE;
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeIsStandardEdition
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeIsStandardEdition(JNIEnv *env, jclass clazz) {
    return JNI_FALSE;
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeGetControlEvent
 * Signature: (V)I
 */
JNIEXPORT jint JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeGetControlEvent(JNIEnv *env, jclass clazz) {
    int event = 0;

    if (wrapperLockControlEventQueue()) {
        /* Failed.  Should have been reported. */
        return 0;
    }

    if (controlEventQueueLastWriteIndex != controlEventQueueLastReadIndex) {
#ifdef _DEBUG
        _tprintf(TEXT(" Queue Read 1 R:%d W:%d\n"), controlEventQueueLastReadIndex, controlEventQueueLastWriteIndex);
        fflush(NULL);
#endif
        controlEventQueueLastReadIndex++;
        if (controlEventQueueLastReadIndex >= CONTROL_EVENT_QUEUE_SIZE) {
            controlEventQueueLastReadIndex = 0;
        }
        event = controlEventQueue[controlEventQueueLastReadIndex];
#ifdef _DEBUG
        _tprintf(TEXT(" Queue Read 2 R:%d W:%d E:%d\n"), controlEventQueueLastReadIndex, controlEventQueueLastWriteIndex, event);
        fflush(NULL);
#endif
    }

    if (wrapperReleaseControlEventQueue()) {
        /* Failed.  Should have been reported. */
        return event;
    }
    return event;
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    accessViolationInner
 * Signature: (V)V
 */
JNIEXPORT void JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_accessViolationInner(JNIEnv *env, jclass clazz) {
    TCHAR *ptr;

    /* Cause access violation */
    ptr = NULL;
    ptr[0] = L'\n';
}



JNIEXPORT jobject JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeExec(JNIEnv *env, jclass jWrapperManagerClass, jobjectArray jCmdArray, jstring jCmdLine, jobject jWrapperProcessConfig, jboolean spawnChDir) {

    throwThrowable(env, "org/tanukisoftware/wrapper/WrapperLicenseError", TEXT("This function is only available in the Professional Edition of the Java Service Wrapper."));
    return NULL;

}
