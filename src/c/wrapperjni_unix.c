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

#ifndef WIN32
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <grp.h>
#include <pthread.h>
#include <pwd.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <unistd.h>
#include "wrapper_i18n.h"
#include "wrapperjni.h"

static pid_t wrapperProcessId = -1;
pthread_mutex_t controlEventQueueMutex = PTHREAD_MUTEX_INITIALIZER;


int wrapperLockControlEventQueue() {
    int count = 0;
    struct timespec ts;
    /* Only wait for up to 30 seconds to make sure we don't get into a deadlock situation.
     *  This could happen if a signal is encountered while locked. */
    while (pthread_mutex_trylock(&controlEventQueueMutex) == EBUSY) {
        if (count >= 3000) {
            _tprintf(TEXT("WrapperJNI Error: Timed out waiting for control event queue lock.\n"));
            fflush(NULL);
            return -1;
        }

        ts.tv_sec = 0;
        ts.tv_nsec = 10000000; /* 10ms (nanoseconds) */
        nanosleep(&ts, NULL);
        count++;
    }

    if (count > 0) {
        if (wrapperJNIDebugging) {
            /* This is useful for making sure that the JNI call is working. */
            _tprintf(TEXT("WrapperJNI Debug: wrapperLockControlEventQueue looped %d times before lock.\n"), count);
            fflush(NULL);
        }
    }
    return 0;
}

int wrapperReleaseControlEventQueue() {
    if (pthread_mutex_unlock(&controlEventQueueMutex)) {
        _tprintf(TEXT("WrapperJNI Error: Failed to unlock the event queue mutex.\n"));
        fflush(NULL);
    }
    return 0;
}

/**
 * Handle interrupt signals (i.e. Crtl-C).
 */
void handleInterrupt(int sig_num) {
    wrapperJNIHandleSignal(org_tanukisoftware_wrapper_WrapperManager_WRAPPER_CTRL_C_EVENT);
    signal(SIGINT, handleInterrupt);
}

/**
 * Handle termination signals (i.e. machine is shutting down).
 */
void handleTermination(int sig_num) {
    wrapperJNIHandleSignal(org_tanukisoftware_wrapper_WrapperManager_WRAPPER_CTRL_TERM_EVENT);
    signal(SIGTERM, handleTermination);
}

/**
 * Handle hangup signals.
 */
void handleHangup(int sig_num) {
    wrapperJNIHandleSignal(org_tanukisoftware_wrapper_WrapperManager_WRAPPER_CTRL_HUP_EVENT);
    signal(SIGHUP, handleHangup);
}

/**
 * Handle usr1 signals.
 *
 * SIGUSR1 & SIGUSR2 are used by the JVM for internal garbage collection sweeps.
 *  These signals MUST be passed on to the JVM or the JVM will hang.
 */
/*
 void handleUsr1(int sig_num) {
    wrapperJNIHandleSignal(org_tanukisoftware_wrapper_WrapperManager_WRAPPER_CTRL_USR1_EVENT);
    signal(SIGUSR1, handleUsr1);
}
 */

/**
 * Handle usr2 signals.
 *
 * SIGUSR1 & SIGUSR2 are used by the JVM for internal garbage collection sweeps.
 *  These signals MUST be passed on to the JVM or the JVM will hang.
 */
/*
 void handleUsr2(int sig_num) {
    wrapperJNIHandleSignal(org_tanukisoftware_wrapper_WrapperManager_WRAPPER_CTRL_USR2_EVENT);
    signal(SIGUSR2, handleUsr2);
}
*/

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeInit
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeInit(JNIEnv *env, jclass jClassWrapperManager, jboolean debugging) {
    TCHAR *retLocale;
    wrapperJNIDebugging = debugging;

    /* Set the locale so we can display MultiByte characters. */
    retLocale = _tsetlocale(LC_ALL, TEXT(""));
#if defined(UNICODE)
    if (retLocale) {
        free(retLocale);
    }
#endif

    if (wrapperJNIDebugging) {
        /* This is useful for making sure that the JNI call is working. */
        _tprintf(TEXT("WrapperJNI Debug: Inside native WrapperManager initialization method\n"));
        fflush(NULL);
    }

    /* Set handlers for signals */
    signal(SIGINT,  handleInterrupt);
    signal(SIGTERM, handleTermination);
    signal(SIGHUP,  handleHangup);
    /*
    signal(SIGUSR1, handleUsr1);
    signal(SIGUSR2, handleUsr2);
    */

    initCommon(env, jClassWrapperManager);

    /* Store the current process Id */
    wrapperProcessId = getpid();
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeRedirectPipes
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeRedirectPipes(JNIEnv *evn, jclass clazz) {
    int fd;
    
    fd = _topen(TEXT("/dev/null"), O_RDWR, 0);
    if (fd != -1) {
        if (!redirectedStdErr) {
            _ftprintf(stderr, TEXT("WrapperJNI: Redirecting %s to /dev/null\n"), TEXT("StdErr")); fflush(NULL);
            if (dup2(fd, STDERR_FILENO) == -1) {
                _ftprintf(stderr, TEXT("WrapperJNI: Failed to redirect %s to /dev/null  (Err: %s)\n"), TEXT("StdErr"), getLastErrorText()); fflush(NULL);
            } else {
                redirectedStdErr = TRUE;
            }
        }
        
        if (!redirectedStdOut) {
            _tprintf(TEXT("WrapperJNI: Redirecting %s to /dev/null\n"), TEXT("StdOut")); fflush(NULL);
            if (dup2(fd, STDOUT_FILENO) == -1) {
                _tprintf(TEXT("WrapperJNI: Failed to redirect %s to /dev/null  (Err: %s)\n"), TEXT("StdOut"), getLastErrorText()); fflush(NULL);
            } else {
                redirectedStdOut = TRUE;
            }
        }
    } else {
        _ftprintf(stderr, TEXT("WrapperJNI: Failed to open /dev/null  (Err: %s)\n"), getLastErrorText()); fflush(NULL);
    }
    
    return 0;
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeGetJavaPID
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeGetJavaPID(JNIEnv *env,
        jclass clazz) {
    return (int) getpid();
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeRequestThreadGroup
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeRequestThreadDump(
        JNIEnv *env, jclass clazz) {
    if (wrapperJNIDebugging) {
        _tprintf(TEXT("WrapperJNI Debug: Sending SIGQUIT event to process group %d.\n"),
            (int)wrapperProcessId);
        fflush(NULL);
    }
    if (kill(wrapperProcessId, SIGQUIT) < 0) {
        _tprintf(TEXT("WrapperJNI Error: Unable to send SIGQUIT to JVM process: %s\n"),
            getLastErrorText());
        fflush(NULL);
    }
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeSetConsoleTitle
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeSetConsoleTitle(JNIEnv *env, jclass clazz, jstring jstringTitle) {
    if (wrapperJNIDebugging) {
        _tprintf(TEXT("WrapperJNI Debug: Setting the console title not supported on UNIX platforms.\n"));
        fflush(NULL);
    }
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeGetUser
 * Signature: (Z)Lorg/tanukisoftware/wrapper/WrapperUser;
 */
/*#define UVERBOSE*/
JNIEXPORT jobject JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeGetUser(JNIEnv *env, jclass clazz, jboolean groups) {
    jclass wrapperUserClass;
    jmethodID constructor;
    jmethodID setGroup;
    jmethodID addGroup;
    uid_t uid;
    struct passwd *pw;
    gid_t ugid;
    jstring jstringUser;
    jstring jstringRealName;
    jstring jstringHome;
    jstring jstringShell;
    jobject wrapperUser = NULL;
    struct group *aGroup;
    int member;
    int i;
    gid_t ggid;
    jstring jstringGroupName;

    /* Look for the WrapperUser class. Ignore failures as JNI throws an exception. */
    if ((wrapperUserClass = (*env)->FindClass(env, utf8ClassOrgTanukisoftwareWrapperWrapperUNIXUser)) != NULL) {

        /* Look for the constructor. Ignore failures. */
        if ((constructor = (*env)->GetMethodID(env, wrapperUserClass, utf8MethodInit, utf8SigIIStringStringStringStringrV)) != NULL) {

            uid = geteuid();
            pw = getpwuid(uid);
            ugid = pw->pw_gid;

            /* Create the arguments to the constructor as java objects */
            /* User */
            jstringUser = JNU_NewStringFromNativeChar(env, pw->pw_name);
            if (jstringUser) {
                /* Real Name */
                jstringRealName = JNU_NewStringFromNativeChar(env, pw->pw_gecos);
                if (jstringRealName) {
                    /* Home */
                    jstringHome = JNU_NewStringFromNativeChar(env, pw->pw_dir);
                    if (jstringHome) {
                        /* Shell */
                        jstringShell = JNU_NewStringFromNativeChar(env, pw->pw_shell);
                        if (jstringShell) {
                            /* Now create the new wrapperUser using the constructor arguments collected above. */
                            wrapperUser = (*env)->NewObject(env, wrapperUserClass, constructor,
                                    uid, ugid, jstringUser, jstringRealName, jstringHome, jstringShell);

                            /* If the caller requested the user's groups then look them up. */
                            if (groups) {
                                /* Set the user group. */
                                if ((setGroup = (*env)->GetMethodID(env, wrapperUserClass, utf8MethodSetGroup, utf8SigIStringrV)) != NULL) {
                                    if ((aGroup = getgrgid(ugid)) != NULL) {
                                        ggid = aGroup->gr_gid;

                                        /* Group name */
                                        jstringGroupName = JNU_NewStringFromNativeChar(env, aGroup->gr_name);
                                        if (jstringGroupName) {
                                            /* Add the group to the user. */
                                            (*env)->CallVoidMethod(env, wrapperUser, setGroup,
                                                    ggid, jstringGroupName);

                                            (*env)->DeleteLocalRef(env, jstringGroupName);
                                        } else {
                                            /* Exception Thrown */
                                        }
                                    }
                                } else {
                                    /* Exception Thrown */
                                }

                                /* Look for the addGroup method. Ignore failures. */
                                if ((addGroup = (*env)->GetMethodID(env, wrapperUserClass, utf8MethodAddGroup, utf8SigIStringrV)) != NULL) {
                                    setgrent();
                                    while ((aGroup = getgrent()) != NULL) {
                                        /* Search the member list to decide whether or not the user is a member. */
                                        member = 0;
                                        i = 0;
                                        while ((member == 0) && aGroup->gr_mem[i]) {
                                            if (strcmp(aGroup->gr_mem[i], pw->pw_name) == 0) {
                                               member = 1;
                                            }
                                            i++;
                                        }

                                        if (member) {
                                            ggid = aGroup->gr_gid;

                                            /* Group name */
                                            jstringGroupName = JNU_NewStringFromNativeChar(env, aGroup->gr_name);
                                            if (jstringGroupName) {
                                                /* Add the group to the user. */
                                                (*env)->CallVoidMethod(env, wrapperUser, addGroup,
                                                        ggid, jstringGroupName);

                                                (*env)->DeleteLocalRef(env, jstringGroupName);
                                            } else {
                                                /* Exception Thrown */
                                            }
                                        }
                                    }
                                    endgrent();
                                } else {
                                    /* Exception Thrown */
                                }
                            }

                            (*env)->DeleteLocalRef(env, jstringShell);
                        } else {
                            /* Exception Thrown */
                        }

                        (*env)->DeleteLocalRef(env, jstringHome);
                    } else {
                        /* Exception Thrown */
                    }

                    (*env)->DeleteLocalRef(env, jstringRealName);
                } else {
                    /* Exception Thrown */
                }

                (*env)->DeleteLocalRef(env, jstringUser);
            } else {
                /* Exception Thrown */
            }
        } else {
            /* Exception Thrown */
        }

        (*env)->DeleteLocalRef(env, wrapperUserClass);
    }

    return wrapperUser;
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeGetInteractiveUser
 * Signature: (Z)Lorg/tanukisoftware/wrapper/WrapperUser;
 */
JNIEXPORT jobject JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeGetInteractiveUser(JNIEnv *env, jclass clazz, jboolean groups) {
    /* If the DISPLAY environment variable is set then assume that this user
     *  has access to an X display, in which case we will return the same thing
     *  as nativeGetUser. */
    if (getenv("DISPLAY")) {
        /* This is an interactive JVM since it has access to a display. */
        return Java_org_tanukisoftware_wrapper_WrapperManager_nativeGetUser(env, clazz, groups);
    } else {
        /* There is no DISPLAY variable, so assume that this JVM is non-interactive. */
        return NULL;
    }
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeListServices
 * Signature: ()[Lorg/tanukisoftware/wrapper/WrapperWin32Service;
 */
JNIEXPORT jobjectArray JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeListServices(JNIEnv *env, jclass clazz) {
    /** Not supported on UNIX platforms. */
    return NULL;
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeSendServiceControlCode
 * Signature: (Ljava/lang/String;I)Lorg/tanukisoftware/wrapper/WrapperWin32Service;
 */
JNIEXPORT jobject JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeSendServiceControlCode(JNIEnv *env, jclass clazz, jbyteArray serviceName, jint controlCode) {
    /** Not supported on UNIX platforms. */
    return NULL;
}
#endif
