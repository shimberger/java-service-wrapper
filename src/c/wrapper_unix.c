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

/**
 * Author:
 *   Leif Mortenson <leif@tanukisoftware.com>
 *   Ryan Shaw
 */

#ifndef WIN32

#include <wchar.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>
#include <limits.h>
#include <pthread.h>
#include <pwd.h>
#include <sys/resource.h>
#include <sys/timeb.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "wrapper_i18n.h"
#include "wrapper.h"
#include "wrapperinfo.h"
#include "property.h"
#include "logger.h"

#include <sys/resource.h>
#include <sys/time.h>

#if defined(IRIX)
#define PATH_MAX FILENAME_MAX
#endif

#ifndef USE_USLEEP
#include <time.h>
#endif

#ifndef getsid
/* getpid links ok on Linux, but is not defined correctly. */
pid_t getsid(pid_t pid);
#endif

#define max(x,y) (((x) > (y)) ? (x) : (y))
#define min(x,y) (((x) < (y)) ? (x) : (y))

int jvmOut = -1;

/* Define a global pipe descriptor so that we don't have to keep allocating
 *  a new pipe each time a JVM is launched. */
int pipedes[2];
int pipeInitialized = 0;

TCHAR wrapperClasspathSeparator = TEXT(':');

int javaIOThreadSet = FALSE;
pthread_t javaIOThreadId;
int javaIOThreadStarted = FALSE;
int stopJavaIOThread = FALSE;
int javaIOThreadStopped = FALSE;

int timerThreadSet = FALSE;
pthread_t timerThreadId;
int timerThreadStarted = FALSE;
int stopTimerThread = FALSE;
int timerThreadStopped = FALSE;

TICKS timerTicks = WRAPPER_TICK_INITIAL;

/******************************************************************************
 * Platform specific methods
 *****************************************************************************/

/**
 * exits the application after running shutdown code.
 */
void appExit(int exitCode, int argc, TCHAR** argv) {
    int i;
    /* Remove pid file.  It may no longer exist. */
    if (wrapperData->pidFilename) {
        _tunlink(wrapperData->pidFilename);
    }

    /* Remove lock file.  It may no longer exist. */
    if (wrapperData->lockFilename) {
        _tunlink(wrapperData->lockFilename);
    }

    /* Remove status file.  It may no longer exist. */
    if (wrapperData->statusFilename) {
        _tunlink(wrapperData->statusFilename);
    }

    /* Remove java status file if it was registered and created by this process. */
    if (wrapperData->javaStatusFilename) {
        _tunlink(wrapperData->javaStatusFilename);
    }

    /* Remove java id file if it was registered and created by this process. */
    if (wrapperData->javaIdFilename) {
        _tunlink(wrapperData->javaIdFilename);
    }

    /* Remove anchor file.  It may no longer exist. */
    if (wrapperData->anchorFilename) {
        _tunlink(wrapperData->anchorFilename);
    }

    /* Common wrapper cleanup code. */
    wrapperDispose();
#if defined(UNICODE)
    for (i = 0; i < argc; i++) {
        if (argv[i]) {
            free(argv[i]);
        }
    }
    if (argv) {
        free(argv);
    }
#endif
    exit(exitCode);
}

/**
 * Gets the error code for the last operation that failed.
 */
int wrapperGetLastError() {
    return errno;
}

/**
 * Writes a PID to disk.
 *
 * filename: File to write to.
 * pid: pid to write in the file.
 */
int writePidFile(const TCHAR *filename, DWORD pid, int newUmask) {
    FILE *pid_fp = NULL;
    int old_umask;

    old_umask = umask(newUmask);
    pid_fp = _tfopen(filename, TEXT("w"));
    umask(old_umask);

    if (pid_fp != NULL) {
        _ftprintf(pid_fp, TEXT("%d\n"), (int)pid);
        fclose(pid_fp);
    } else {
        return 1;
    }
    return 0;
}

/**
 * Send a signal to the JVM process asking it to dump its JVM state.
 */
void wrapperRequestDumpJVMState() {
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
        TEXT("Dumping JVM state."));
    if (kill(wrapperData->javaPID, SIGQUIT) < 0) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                   TEXT("Could not dump JVM state: %s"), getLastErrorText());
    }
}

/**
 * Called when a signal is processed.  This is actually called from within the main event loop
 *  and NOT the signal handler.  So it is safe to use the normal logging functions.
 *
 * @param sigNum Signal that was fired.
 * @param sigName Name of the signal for logging.
 * @param mode Action that should be taken.
 */
void takeSignalAction(int sigNum, const TCHAR *sigName, int mode) {
    if (wrapperData->ignoreSignals & WRAPPER_IGNORE_SIGNALS_WRAPPER) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
            TEXT("%s trapped, but ignored."), sigName);
    } else {
        switch (mode) {
        case WRAPPER_SIGNAL_MODE_RESTART:
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                TEXT("%s trapped.  Restarting JVM."), sigName);
            wrapperRestartProcess();
            break;

        case WRAPPER_SIGNAL_MODE_SHUTDOWN:
            if (wrapperData->exitRequested || wrapperData->restartRequested ||
                (wrapperData->jState == WRAPPER_JSTATE_DOWN_CLEAN) ||
                (wrapperData->jState == WRAPPER_JSTATE_STOP) ||
                (wrapperData->jState == WRAPPER_JSTATE_STOPPING) ||
                (wrapperData->jState == WRAPPER_JSTATE_STOPPED) ||
                (wrapperData->jState == WRAPPER_JSTATE_KILLING) ||
                (wrapperData->jState == WRAPPER_JSTATE_KILL) ||
                (wrapperData->jState == WRAPPER_JSTATE_DOWN_CHECK)) {

                /* Signaled while we were already shutting down. */
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                    TEXT("%s trapped.  Forcing immediate shutdown."), sigName);

                /* Disable the thread dump on exit feature if it is set because it
                 *  should not be displayed when the user requested the immediate exit. */
                wrapperData->requestThreadDumpOnFailedJVMExit = FALSE;
                wrapperKillProcess();
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                    TEXT("%s trapped.  Shutting down."), sigName);
                /* Always force the shutdown as this is an external event. */
                wrapperStopProcess(0, TRUE);
            }
            /* Don't actually kill the process here.  Let the application shut itself down */

            /* To make sure that the JVM will not be restarted for any reason,
             *  start the Wrapper shutdown process as well. */
            if ((wrapperData->wState == WRAPPER_WSTATE_STOPPING) ||
                (wrapperData->wState == WRAPPER_WSTATE_STOPPED)) {
                /* Already stopping. */
            } else {
                wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
            }
            break;

        case WRAPPER_SIGNAL_MODE_FORWARD:
            if (wrapperData->javaPID > 0) {
                if (wrapperData->isDebugging) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                        TEXT("%s trapped.  Forwarding to JVM process."), sigName);
                }
                if (kill(wrapperData->javaPID, sigNum)) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                        TEXT("Unable to forward %s signal to JVM process.  %s"), sigName, getLastErrorText());
                }
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                    TEXT("%s trapped.  Unable to forward signal to JVM because it is not running."), sigName);
            }
            break;

        default: /* WRAPPER_SIGNAL_MODE_IGNORE */
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                TEXT("%s trapped, but ignored."), sigName);
            break;
        }
    }
}

/**
 * This function goes through and checks flags for each of several signals to see if they
 *  have been fired since the last time this function was called.  This is the only thread
 *  which will ever clear these flags, but they can be set by other threads within the
 *  signal handlers at ANY time.  So only check the value of each flag once and reset them
 *  immediately to decrease the chance of missing duplicate signals.
 */
void wrapperMaintainSignals() {
    /* SIGINT */
    if (wrapperData->signalInterruptTrapped) {
        wrapperData->signalInterruptTrapped = FALSE;
        
        takeSignalAction(SIGINT, TEXT("INT"), WRAPPER_SIGNAL_MODE_SHUTDOWN);
    }
    
    /* SIGQUIT */
    if (wrapperData->signalQuitTrapped) {
        wrapperData->signalQuitTrapped = FALSE;
        
        wrapperRequestDumpJVMState();
    }
    
    /* SIGCHLD */
    if (wrapperData->signalChildTrapped) {
        wrapperData->signalChildTrapped = FALSE;
        
        wrapperGetProcessStatus(wrapperGetTicks(), TRUE);
    }
    
    /* SIGTERM */
    if (wrapperData->signalTermTrapped) {
        wrapperData->signalTermTrapped = FALSE;
        
        takeSignalAction(SIGTERM, TEXT("TERM"), WRAPPER_SIGNAL_MODE_SHUTDOWN);
    }
    
    /* SIGHUP */
    if (wrapperData->signalHUPTrapped) {
        wrapperData->signalHUPTrapped = FALSE;
        
        takeSignalAction(SIGHUP, TEXT("HUP"), wrapperData->signalHUPMode);
    }
    
    /* SIGUSR1 */
    if (wrapperData->signalUSR1Trapped) {
        wrapperData->signalUSR1Trapped = FALSE;
        
        takeSignalAction(SIGUSR1, TEXT("USR1"), wrapperData->signalUSR1Mode);
    }
    
#ifndef VALGRIND
    /* SIGUSR2 */
    if (wrapperData->signalUSR2Trapped) {
        wrapperData->signalUSR2Trapped = FALSE;
        
        takeSignalAction(SIGUSR2, TEXT("USR2"), wrapperData->signalUSR2Mode);
    }
#endif
}

/**
 * This is called from within signal handlers so NO MALLOCs are allowed here.
 */
const TCHAR* getSignalName(int signo) {
    switch (signo) {
    case SIGALRM:
        return TEXT("SIGALRM");
    case SIGINT:
        return TEXT("SIGINT");
    case SIGKILL:
        return TEXT("SIGKILL");
    case SIGQUIT:
        return TEXT("SIGQUIT");
    case SIGCHLD:
        return TEXT("SIGCHLD");
    case SIGTERM:
        return TEXT("SIGTERM");
    case SIGHUP:
        return TEXT("SIGHUP");
    case SIGUSR1:
        return TEXT("SIGUSR1");
    case SIGUSR2:
        return TEXT("SIGUSR2");
    case SIGSEGV:
        return TEXT("SIGSEGV");
    default:
        return TEXT("UNKNOWN");
    }
}

/**
 * This is called from within signal handlers so NO MALLOCs are allowed here.
 */
const TCHAR* getSignalCodeDesc(int code) {
    switch (code) {
#ifdef SI_USER
    case SI_USER:
        return TEXT("kill, sigsend or raise");
#endif

#ifdef SI_KERNEL
    case SI_KERNEL:
        return TEXT("the kernel");
#endif

    case SI_QUEUE:
        return TEXT("sigqueue");

#ifdef SI_TIMER
    case SI_TIMER:
        return TEXT("timer expired");
#endif

#ifdef SI_MESGQ
    case SI_MESGQ:
        return TEXT("mesq state changed");
#endif

    case SI_ASYNCIO:
        return TEXT("AIO completed");

#ifdef SI_SIGIO
    case SI_SIGIO:
        return TEXT("queued SIGIO");
#endif

    default:
        return TEXT("unknown");
    }
}

/**
 * Describe a signal.  This is called from within signal handlers so NO MALLOCs are allowed here.
 */
void descSignal(siginfo_t *sigInfo) {
    struct passwd *pw;
#ifdef UNICODE
    size_t req;
#endif
    TCHAR *uName;

    /* Not supported on all platforms */
    if (sigInfo == NULL) {
        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
            TEXT("Signal trapped.  No details available."));
        return;
    }

    if (wrapperData->isDebugging) {
        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
            TEXT("Signal trapped.  Details:"));

        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
#if defined(UNICODE)
            TEXT("  signal number=%d (%S), source=\"%S\""),
#else
            TEXT("  signal number=%d (%s), source=\"%s\""),
#endif
            sigInfo->si_signo,
            getSignalName(sigInfo->si_signo),
            getSignalCodeDesc(sigInfo->si_code));

        if (sigInfo->si_errno != 0) {
            log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
#if defined(UNICODE)
                TEXT("  signal err=%d, \"%S\""),
#else
                TEXT("  signal err=%d, \"%s\""),
#endif
                sigInfo->si_errno,
                strerror(sigInfo->si_errno));
        }

#ifdef SI_USER
        if (sigInfo->si_code == SI_USER) {
            pw = getpwuid(sigInfo->si_uid);
            if (pw == NULL) {
                uName = TEXT("<unknown>");
            } else {
#ifndef UNICODE
                uName = pw->pw_name;
#else
                req = mbstowcs(NULL, pw->pw_name, 0) + 1;
                uName = malloc(req * sizeof(TCHAR));
                if (!uName) {
                    outOfMemory(TEXT("DSCS"), 1);
                    return;
                }
                mbstowcs(uName, pw->pw_name, req + 1);
#endif
            }

            /* It appears that the getsid function was added in version 1.3.44 of the linux kernel. */
            log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
#ifdef UNICODE
                TEXT("  signal generated by PID: %d (Session PID: %d), UID: %d (%S)"),
#else
                TEXT("  signal generated by PID: %d (Session PID: %d), UID: %d (%s)"),
#endif
                sigInfo->si_pid, getsid(sigInfo->si_pid), sigInfo->si_uid, uName);
#ifdef UNICODE
            free(uName);
#endif
        }
#endif
    }
}

/**
 * Handle alarm signals.  We are getting them on solaris when running with
 *  the tick timer.  Not yet sure where they are coming from.
 */
void sigActionAlarm(int sigNum, siginfo_t *sigInfo, void *na) {
    pthread_t threadId;

    /* On UNIX the calling thread is the actual thread being interrupted
     *  so it has already been registered with logRegisterThread. */

    descSignal(sigInfo);

    threadId = pthread_self();

    if (wrapperData->isDebugging) {
        if (timerThreadSet && pthread_equal(threadId, timerThreadId)) {
            log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                TEXT("Timer thread received an Alarm signal.  Ignoring."));
        } else {
            log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                TEXT("Received an Alarm signal.  Ignoring."));
        }
    }
}

/**
 * Handle interrupt signals (i.e. Crtl-C).
 */
void sigActionInterrupt(int sigNum, siginfo_t *sigInfo, void *na) {
    /* On UNIX the calling thread is the actual thread being interrupted
     *  so it has already been registered with logRegisterThread. */

    descSignal(sigInfo);

    wrapperData->signalInterruptTrapped = TRUE;
}

/**
 * Handle quit signals (i.e. Crtl-\).
 */
void sigActionQuit(int sigNum, siginfo_t *sigInfo, void *na) {
    pthread_t threadId;

    /* On UNIX the calling thread is the actual thread being interrupted
     *  so it has already been registered with logRegisterThread. */

    descSignal(sigInfo);

    threadId = pthread_self();

    if (timerThreadSet && pthread_equal(threadId, timerThreadId)) {
        if (wrapperData->isDebugging) {
            log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                TEXT("Timer thread received an Quit signal.  Ignoring."));
        }
    } else {
        wrapperData->signalQuitTrapped = TRUE;
    }
}

/**
 * Handle termination signals (i.e. machine is shutting down).
 */
void sigActionChildDeath(int sigNum, siginfo_t *sigInfo, void *na) {
    pthread_t threadId;

    /* On UNIX, when a Child process changes state, a SIGCHLD signal is sent to the parent.
     *  The parent should do a wait to make sure the child is cleaned up and doesn't become
     *  a zombie process. */

    threadId = pthread_self();
    if (timerThreadSet && pthread_equal(threadId, timerThreadId)) {
        if (wrapperData->isDebugging) {
            log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                TEXT("Timer thread received a SigChild signal.  Ignoring."));
        }
    } else {
        descSignal(sigInfo);

        if (wrapperData->isDebugging) {
            log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                TEXT("Received SIGCHLD, checking JVM process status."));
        }
        
        /* This is set whenever any child signals that it has exited.
         *  Inside the code we go on to check to make sure that we only test for the JVM */
        wrapperData->signalChildTrapped = TRUE;
    }
}

/**
 * Handle termination signals (i.e. machine is shutting down).
 */
void sigActionTermination(int sigNum, siginfo_t *sigInfo, void *na) {
    /* On UNIX the calling thread is the actual thread being interrupted
     *  so it has already been registered with logRegisterThread. */

    descSignal(sigInfo);
    
    wrapperData->signalTermTrapped = TRUE;
}

/**
 * Handle hangup signals.
 */
void sigActionHangup(int sigNum, siginfo_t *sigInfo, void *na) {
    /* On UNIX the calling thread is the actual thread being interrupted
     *  so it has already been registered with logRegisterThread. */

    descSignal(sigInfo);
    
    wrapperData->signalHUPTrapped = TRUE;
}

/**
 * Handle USR1 signals.
 */
void sigActionUSR1(int sigNum, siginfo_t *sigInfo, void *na) {
    /* On UNIX the calling thread is the actual thread being interrupted
     *  so it has already been registered with logRegisterThread. */

    descSignal(sigInfo);
    
    wrapperData->signalUSR1Trapped = TRUE;
}

/**
 * Handle USR2 signals.
 */
void sigActionUSR2(int sigNum, siginfo_t *sigInfo, void *na) {
    /* On UNIX the calling thread is the actual thread being interrupted
     *  so it has already been registered with logRegisterThread. */

    descSignal(sigInfo);
    
    wrapperData->signalUSR2Trapped = TRUE;
}

/**
 * Registers a single signal handler.
 */
int registerSigAction(int sigNum, void (*sigAction)(int, siginfo_t *, void *)) {
    struct sigaction newAct;

    newAct.sa_sigaction = sigAction;
    sigemptyset(&newAct.sa_mask);
    newAct.sa_flags = SA_SIGINFO;

    if (sigaction(sigNum, &newAct, NULL)) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
            TEXT("Unable to register signal handler for signal %d.  %s"), sigNum, getLastErrorText());
        return 1;
    }
    return 0;
}

/**
 * The main entry point for the javaio thread which is started by
 *  initializeJavaIO().  Once started, this thread will run for the
 *  life of the process.
 *
 * This thread will only be started if we are configured to use a
 *  dedicated thread to read JVM output.
 */
void *javaIORunner(void *arg) {
    sigset_t signal_mask;
    int nextSleep;
#ifndef VALGRIND
    int rc;
#endif

    javaIOThreadStarted = TRUE;
    
    /* Immediately register this thread with the logger. */
    logRegisterThread(WRAPPER_THREAD_JAVAIO);

    /* mask signals so the javaIO doesn't get any of these. */
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGTERM);
    sigaddset(&signal_mask, SIGINT);
    sigaddset(&signal_mask, SIGQUIT);
    sigaddset(&signal_mask, SIGALRM);
    sigaddset(&signal_mask, SIGHUP);
    sigaddset(&signal_mask, SIGUSR1);
#ifndef VALGRIND
    sigaddset(&signal_mask, SIGUSR2);
    rc = pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
    if (rc != 0) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Could not mask signals for javaIO thread."));
    }
#endif

    if (wrapperData->isJavaIOOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("JavaIO thread started."));
    }

    nextSleep = TRUE;
    /* Loop until we are shutting down, but continue as long as there is more output from the JVM. */
    while ((!stopJavaIOThread) || (!nextSleep)) {
        if (nextSleep) {
            /* Sleep for a hundredth of a second. */
            wrapperSleep(10);
        }
        nextSleep = TRUE;
        
        if (wrapperData->pauseThreadJavaIO) {
            wrapperPauseThread(wrapperData->pauseThreadJavaIO, TEXT("javaio"));
            wrapperData->pauseThreadJavaIO = 0;
        }
        
        if (wrapperReadChildOutput()) {
            if (wrapperData->isDebugging) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                    TEXT("Pause reading child process output to share cycles."));
            }
            nextSleep = FALSE;
        }
    }

    javaIOThreadStopped = TRUE;
    if (wrapperData->isJavaIOOutputEnabled) {
        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("JavaIO thread stopped."));
    }
    return NULL;
}

/**
 * Creates a process whose job is to loop and simply increment a ticks
 *  counter.  The tick counter can then be used as a clock as an alternative
 *  to using the system clock.
 */
int initializeJavaIO() {
    int res;

    if (wrapperData->isJavaIOOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Launching JavaIO thread."));
    }

    res = pthread_create(&javaIOThreadId,
        NULL, /* No attributes. */
        javaIORunner,
        NULL); /* No parameters need to be passed to the thread. */
    if (res) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
            TEXT("Unable to create a javaIO thread: %d, %s"), res, getLastErrorText());
        javaIOThreadSet = TRUE;
        return 1;
    } else {
        if (pthread_detach(javaIOThreadId)) {
            javaIOThreadSet = TRUE;
            return 1;
        }
        javaIOThreadSet = FALSE;
        return 0;
    }
}

void disposeJavaIO() {
    stopJavaIOThread = TRUE;
    /* Wait until the javaIO thread is actually stopped to avoid timing problems. */
    if (javaIOThreadStarted) {
        while (!javaIOThreadStopped) {
#ifdef _DEBUG
            wprintf(TEXT("Waiting for javaIO thread to stop.\n"));
#endif
            wrapperSleep(100);
        }
        pthread_kill(javaIOThreadId, SIGKILL);
    }
}

/**
 * The main entry point for the timer thread which is started by
 *  initializeTimer().  Once started, this thread will run for the
 *  life of the process.
 *
 * This thread will only be started if we are configured NOT to
 *  use the system time as a base for the tick counter.
 */
void *timerRunner(void *arg) {
    TICKS sysTicks;
    TICKS lastTickOffset = 0;
    TICKS tickOffset;
    TICKS nowTicks;
    int offsetDiff;
    int first = TRUE;
    sigset_t signal_mask;
#ifndef VALGRIND
    int rc;
#endif

    timerThreadStarted = TRUE;
    
    /* Immediately register this thread with the logger. */
    logRegisterThread(WRAPPER_THREAD_TIMER);

    /* mask signals so the timer doesn't get any of these. */
    sigemptyset(&signal_mask);
    sigaddset(&signal_mask, SIGTERM);
    sigaddset(&signal_mask, SIGINT);
    sigaddset(&signal_mask, SIGQUIT);
    sigaddset(&signal_mask, SIGALRM);
    sigaddset(&signal_mask, SIGHUP);
    sigaddset(&signal_mask, SIGUSR1);
#ifndef VALGRIND
    sigaddset(&signal_mask, SIGUSR2);
    rc = pthread_sigmask(SIG_BLOCK, &signal_mask, NULL);
    if (rc != 0) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Could not mask signals for timer thread."));
    }
#endif

    if (wrapperData->isTickOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Timer thread started."));
    }

    while (!stopTimerThread) {
        wrapperSleep(WRAPPER_TICK_MS);
        
        if (wrapperData->pauseThreadTimer) {
            wrapperPauseThread(wrapperData->pauseThreadTimer, TEXT("timer"));
            wrapperData->pauseThreadTimer = 0;
        }

        /* Get the tick count based on the system time. */
        sysTicks = wrapperGetSystemTicks();

        /* Lock the tick mutex whenever the "timerTicks" variable is accessed. */
        if (wrapperData->useTickMutex && wrapperLockTickMutex()) {
            timerThreadStopped = TRUE;
            return NULL;
        }
        
        /* Advance the timer tick count. */
        nowTicks = timerTicks++;
        
        if (wrapperData->useTickMutex && wrapperReleaseTickMutex()) {
            timerThreadStopped = TRUE;
            return NULL;
        }

        /* Calculate the offset between the two tick counts. This will always work due to overflow. */
        tickOffset = sysTicks - nowTicks;

        /* The number we really want is the difference between this tickOffset and the previous one. */
        offsetDiff = wrapperGetTickAgeTicks(lastTickOffset, tickOffset);

        if (first) {
            first = FALSE;
        } else {
            if (offsetDiff > wrapperData->timerSlowThreshold) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO,
                    TEXT("The timer fell behind the system clock by %ldms."), offsetDiff * WRAPPER_TICK_MS);
            } else if (offsetDiff < -1 * wrapperData->timerFastThreshold) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO,
                    TEXT("The system clock fell behind the timer by %ldms."), -1 * offsetDiff * WRAPPER_TICK_MS);
            }

            if (wrapperData->isTickOutputEnabled) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT(
                    "    Timer: ticks=0x%08x, system ticks=0x%08x, offset=0x%08x, offsetDiff=0x%08x"),
                    nowTicks, sysTicks, tickOffset, offsetDiff);
            }
        }

        /* Store this tick offset for the next time through the loop. */
        lastTickOffset = tickOffset;
    }

    timerThreadStopped = TRUE;
    if (wrapperData->isTickOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Timer thread stopped."));
    }
    return NULL;
}

/**
 * Creates a process whose job is to loop and simply increment a ticks
 *  counter.  The tick counter can then be used as a clock as an alternative
 *  to using the system clock.
 */
int initializeTimer() {
    int res;

    if (wrapperData->isTickOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Launching Timer thread."));
    }

    res = pthread_create(&timerThreadId,
        NULL, /* No attributes. */
        timerRunner,
        NULL); /* No parameters need to be passed to the thread. */
    if (res) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
            TEXT("Unable to create a timer thread: %d, %s"), res, getLastErrorText());
        timerThreadSet = TRUE;
        return 1;
    } else {
        if (pthread_detach(timerThreadId)) {
            timerThreadSet = TRUE;
            return 1;
        }
        timerThreadSet = FALSE;
        return 0;
    }
}

void disposeTimer() {
    stopTimerThread = TRUE;
    /* Wait until the timer thread is actually stopped to avoid timing problems. */
    if (timerThreadStarted) {
        while (!timerThreadStopped) {
#ifdef _DEBUG
            wprintf(TEXT("Waiting for timer thread to stop.\n"));
#endif
            wrapperSleep(100);
        }
        pthread_kill(timerThreadId, SIGKILL);
    }
}

/**
 * Execute initialization code to get the wrapper set up.
 */
int wrapperInitializeRun() {
    int retval = 0;
    int res;

    /* Register any signal actions we are concerned with. */
    if (registerSigAction(SIGALRM, sigActionAlarm) ||
        registerSigAction(SIGINT,  sigActionInterrupt) ||
        registerSigAction(SIGQUIT, sigActionQuit) ||
        registerSigAction(SIGCHLD, sigActionChildDeath) ||
        registerSigAction(SIGTERM, sigActionTermination) ||
        registerSigAction(SIGHUP,  sigActionHangup) ||
        registerSigAction(SIGUSR1, sigActionUSR1)
#ifndef VALGRIND
        ||
        registerSigAction(SIGUSR2, sigActionUSR2)
#endif
        ) {
        retval = -1;
    }

    /* Attempt to set the console title if it exists and is accessable.
     *  This works on all UNIX versions, but only Linux resets it
     *  correctly when the wrapper process terminates. */
#if defined(LINUX)
    if (wrapperData->consoleTitle) {
        if (wrapperData->isConsole) {
            /* The console should be visible. */
            _tprintf(TEXT("%c]0;%s%c"), TEXT('\033'), wrapperData->consoleTitle, TEXT('\007'));
        }
    }
#endif

    if (wrapperData->useSystemTime) {
        /* We are going to be using system time so there is no reason to start up a timer thread. */
        timerThreadSet = FALSE;
        /* Unable to set the timerThreadId to a null value on all platforms
         * timerThreadId = 0;*/
    } else {
        /* Create and initialize a timer thread. */
        if ((res = initializeTimer()) != 0) {
            return res;
        }
    }
    
    if (wrapperData->useJavaIOThread) {
        /* Create and initialize a javaIO thread. */
        if ((res = initializeJavaIO()) != 0) {
            return res;
        }
    } else {
        javaIOThreadSet = FALSE;
        /* Unable to set the javaIOThreadId to a null value on all platforms
         * javaIOThreadId = 0;*/
    }

    return retval;
}

/**
 * Cause the current thread to sleep for the specified number of milliseconds.
 *  Sleeps over one second are not allowed.
 *
 * @param ms Number of milliseconds to wait for.
 *
 * @return TRUE if the was interrupted, FALSE otherwise.  Neither is an error.
 */
int wrapperSleep(int ms) {
    /* We want to use nanosleep if it is available, but make it possible for the
       user to build a version that uses usleep if they want.
       usleep does not behave nicely with signals thrown while sleeping.  This
       was the believed cause of a hang experienced on one Solaris system. */
#ifdef USE_USLEEP
    if (wrapperData->isSleepOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
            TEXT("    Sleep: usleep %dms"), ms);
    }
    usleep(ms * 1000); /* microseconds */
#else
    struct timespec ts;

    if (ms >= 1000) {
        ts.tv_sec = (ms * 1000000) / 1000000000;
        ts.tv_nsec = (ms * 1000000) % 1000000000; /* nanoseconds */
    } else {
        ts.tv_sec = 0;
        ts.tv_nsec = ms * 1000000; /* nanoseconds */
    }

    if (wrapperData->isSleepOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Sleep: nanosleep %dms"), ms);
    }
    if (nanosleep(&ts, NULL)) {
        if (errno == EINTR) {
            if (wrapperData->isSleepOutputEnabled) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                    TEXT("    Sleep: nanosleep interrupted"));
            }
            return TRUE;
        } else if (errno == EAGAIN) {
            /* On 64-bit AIX this happens once on shutdown. */
            if (wrapperData->isSleepOutputEnabled) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                    TEXT("    Sleep: nanosleep unavailable"));
            }
            return TRUE;
        } else {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("nanosleep(%dms) failed. %s"), ms, getLastErrorText());
        }
    }
#endif

    if (wrapperData->isSleepOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Sleep: awake"));
    }
    
    return FALSE;
}

/**
 * Detaches the Java process so the Wrapper will if effect forget about it.
 */
void wrapperDetachJava() {
    int fd;
    
    wrapperSetJavaState(WRAPPER_JSTATE_DOWN_CLEAN, 0, -1);
    
    /* Redirect the pipes with the JVM to /dev/null so the JVM doesn't block when it writes to them. */
    fd = _topen(TEXT("/dev/null"), O_RDWR, 0);
    if (fd != -1) {
        close(pipedes[STDOUT_FILENO]);
        /*dup2(fd, pipedes[STDOUT_FILENO]); */
        close(pipedes[STDERR_FILENO]);
        /* dup2(fd, pipedes[STDERR_FILENO]); */
        /*if ((fd != pipedes[STDOUT_FILENO]) && (fd != pipedes[STDERR_FILENO])) { */
        close(fd);
        /*}*/
    }
}


/**
 * Build the java command line.
 *
 * @return TRUE if there were any problems.
 */
int wrapperBuildJavaCommand() {
    TCHAR **strings;
    int length, i;

    /* If this is not the first time through, then dispose the old command array */
    if (wrapperData->jvmCommand) {
        i = 0;
        while(wrapperData->jvmCommand[i] != NULL) {
            free(wrapperData->jvmCommand[i]);
            wrapperData->jvmCommand[i] = NULL;
            i++;
        }

        free(wrapperData->jvmCommand);
        wrapperData->jvmCommand = NULL;
    }

    /* First generate the classpath. */
    if (wrapperData->classpath) {
        free(wrapperData->classpath);
        wrapperData->classpath = NULL;
    }
    if (wrapperBuildJavaClasspath(&wrapperData->classpath) < 0) {
        return TRUE;
    }

    /* Build the Java Command Strings */
    strings = NULL;
    length = 0;
    if (wrapperBuildJavaCommandArray(&strings, &length, FALSE, wrapperData->classpath)) {
        return TRUE;
    }

    if (wrapperData->commandLogLevel != LEVEL_NONE) {
        for (i = 0; i < length; i++) {
            log_printf(WRAPPER_SOURCE_WRAPPER, wrapperData->commandLogLevel,
                TEXT("Command[%d] : %s"), i, strings[i]);
        }

        if (wrapperData->environmentClasspath) {
            log_printf(WRAPPER_SOURCE_WRAPPER, wrapperData->commandLogLevel, TEXT(
                "Classpath in Environment : %s"), wrapperData->classpath);
        }
    }

    if (wrapperData->environmentClasspath) {
        setEnv(TEXT("CLASSPATH"), wrapperData->classpath, ENV_SOURCE_WRAPPER);
    }

    /* Allocate memory to hold array of command strings.  The array is itself NULL terminated */
    wrapperData->jvmCommand = malloc(sizeof(TCHAR *) * (length + 1));
    if (!wrapperData->jvmCommand) {
        outOfMemory(TEXT("WBJC"), 1);
        return TRUE;
    }
    /* number of arguments + 1 for a NULL pointer at the end */
    for (i = 0; i <= length; i++) {
        if (i < length) {
            wrapperData->jvmCommand[i] = malloc(sizeof(TCHAR) * (_tcslen(strings[i]) + 1));
            if (!wrapperData->jvmCommand[i]) {
                outOfMemory(TEXT("WBJC"), 2);
                return TRUE;
            }
            _tcsncpy(wrapperData->jvmCommand[i], strings[i], _tcslen(strings[i]) + 1);
        } else {
            wrapperData->jvmCommand[i] = NULL;
        }
    }

    /* Free up the temporary command array */
    wrapperFreeJavaCommandArray(strings, length);

    return FALSE;
}

/**
 * Launches a JVM process and stores it internally.
 */
void wrapperExecute() {
    pid_t proc;

    /* Only allocate a pipe if we have not already done so. */
    if (!pipeInitialized) {
        /* Create the pipe. */
        if (pipe(pipedes) < 0) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                       TEXT("Could not init pipe: %s"), getLastErrorText());
            return;
        }
        pipeInitialized = TRUE;
    }

    /* Make sure the log file is closed before the Java process is created.  Failure to do
     *  so will give the Java process a copy of the open file.  This means that this process
     *  will not be able to rename the file even after closing it because it will still be
     *  open in the Java process.  Also set the auto close flag to make sure that other
     *  threads do not reopen the log file as the new process is being created. */
    setLogfileAutoClose(TRUE);
    closeLogfile();
    /* Fork off the child. */
    proc = fork();

    if (proc == -1) {
        /* Fork failed. */

        /* Restore the auto close flag. */
        setLogfileAutoClose(wrapperData->logfileInactivityTimeout <= 0);

        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                   TEXT("Could not spawn JVM process: %s"), getLastErrorText());

        /* The pipedes array is global so do not close the pipes. */

    } else {
        /* Reset the exit code when we launch a new JVM. */
        wrapperData->exitCode = 0;

        /* Reset the stopped flag. */
        wrapperData->jvmStopped = FALSE;

        if (proc == 0) {
            /* We are the child side. */

            /* Set the umask of the JVM */
            umask(wrapperData->javaUmask);

            /* The logging code causes some log corruption if logging is called from the
             *  child of a fork.  Not sure exactly why but most likely because the forked
             *  child receives a copy of the mutex and thus synchronization is not working.
             * It is ok to log errors in here, but avoid output otherwise.
             * TODO: Figure out a way to fix this.  Maybe using shared memory? */

            /* Send output to the pipe by dupicating the pipe fd and setting the copy as the stdout fd. */
            if (dup2(pipedes[STDOUT_FILENO], STDOUT_FILENO) < 0) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                    TEXT("%sUnable to set JVM's stdout: %s"), LOG_FORK_MARKER, getLastErrorText());
                return;
            }

            /* Send errors to the pipe by dupicating the pipe fd and setting the copy as the stderr fd. */
            if (dup2(pipedes[STDOUT_FILENO], STDERR_FILENO) < 0) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                    TEXT("%sUnable to set JVM's stderr: %s"), LOG_FORK_MARKER, getLastErrorText());
                return;
            }

            /* The pipedes array is global so do not close the pipes. */
            /* Child process: execute the JVM. */
            _texecvp(wrapperData->jvmCommand[0], wrapperData->jvmCommand);

            /* We reached this point...meaning we were unable to start. */
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("%sUnable to start JVM: %s (%d)"), LOG_FORK_MARKER, getLastErrorText(), errno);

            if (wrapperData->isAdviserEnabled) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE, TEXT("%s"), LOG_FORK_MARKER );
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                    TEXT("%s------------------------------------------------------------------------"), LOG_FORK_MARKER );
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                    TEXT("%sAdvice:"), LOG_FORK_MARKER );
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                    TEXT("%sUsually when the Wrapper fails to start the JVM process, it is because"), LOG_FORK_MARKER );
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                    TEXT("%sof a problem with the value of the configured Java command.  Currently:"), LOG_FORK_MARKER );
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                    TEXT("%swrapper.java.command=%s"), LOG_FORK_MARKER, getStringProperty(properties, TEXT("wrapper.java.command"), TEXT("java")));
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                    TEXT("%sPlease make sure that the PATH or any other referenced environment"), LOG_FORK_MARKER );
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                    TEXT("%svariables are correctly defined for the current environment."), LOG_FORK_MARKER );
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                    TEXT("%s------------------------------------------------------------------------"), LOG_FORK_MARKER );
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE, TEXT("%s"), LOG_FORK_MARKER );
            }

            /* This process needs to end. */
            exit(1);
        } else {
            /* We are the parent side. */
            wrapperData->javaPID = proc;
            jvmOut = pipedes[STDIN_FILENO];

            /* Restore the auto close flag. */
            setLogfileAutoClose(wrapperData->logfileInactivityTimeout <= 0);

            /* The pipedes array is global so do not close the pipes. */

            /* Mark our side of the pipe so that it won't block
             * and will close on exec, so new children won't see it. */
            if (fcntl(jvmOut, F_SETFL, O_NONBLOCK) < 0) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                    TEXT("Failed to set JVM output handle to non blocking mode: %s (%d)"),
                    getLastErrorText(), errno);
            }
            if (fcntl(jvmOut, F_SETFD, FD_CLOEXEC) < 0) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                    TEXT("Failed to set JVM output handle to close on JVM exit: %s (%d)"),
                    getLastErrorText(), errno);
            }

            /* If a java pid filename is specified then write the pid of the java process. */
            if (wrapperData->javaPidFilename) {
                if (writePidFile(wrapperData->javaPidFilename, wrapperData->javaPID, wrapperData->javaPidFileUmask)) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                        TEXT("Unable to write the Java PID file: %s"), wrapperData->javaPidFilename);
                }
            }

            /* If a java id filename is specified then write the pid of the java process. */
            if (wrapperData->javaIdFilename) {
                if (writePidFile(wrapperData->javaIdFilename, wrapperData->jvmRestarts, wrapperData->javaIdFileUmask)) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                        TEXT("Unable to write the Java Id file: %s"), wrapperData->javaIdFilename);
                }
            }
        }
    }
}

/**
 * Returns a tick count that can be used in combination with the
 *  wrapperGetTickAgeSeconds() function to perform time keeping.
 */
TICKS wrapperGetTicks() {
    TICKS ticks;
    
    if (wrapperData->useSystemTime) {
        /* We want to return a tick count that is based on the current system time. */
        ticks = wrapperGetSystemTicks();

    } else {
        /* Lock the tick mutex whenever the "timerTicks" variable is accessed. */
        if (wrapperData->useTickMutex && wrapperLockTickMutex()) {
            return 0;
        }
        
        /* Return a snapshot of the current tick count. */
        ticks = timerTicks;
        
        if (wrapperData->useTickMutex && wrapperReleaseTickMutex()) {
            return 0;
        }
    }
    
    return ticks;
}


/**
 * Outputs a a log entry describing what the memory dump columns are.
 */
void wrapperDumpMemoryBanner() {
    /* Not yet implemented on UNIX platforms. */
}

/**
 * Outputs a log entry at regular intervals to track the memory usage of the
 *  Wrapper and its JVM.
 */
void wrapperDumpMemory() {
    struct rusage wUsage;
    struct rusage jUsage;

    if (getrusage(RUSAGE_SELF, &wUsage)) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
            TEXT("Call to getrusage failed for Wrapper process: %s"), getLastErrorText());
        return;
    }
    /* The Children is only going to show the value for terminated children. */
    if (getrusage(RUSAGE_CHILDREN, &jUsage)) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
            TEXT("Call to getrusage failed for Java process: %s"), getLastErrorText());
        return;
    }

    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
        TEXT("Wrapper Memory: maxrss=%ld, ixrss=%ld, idrss=%ld, isrss=%ld, minflt=%ld, majflt=%ld, nswap=%ld, inblock=%ld, oublock=%ld, msgsnd=%ld, msgrcv=%ld, nsignals=%ld, nvcsw=%ld, nvcsw=%ld"),
        wUsage.ru_maxrss,
        wUsage.ru_ixrss,
        wUsage.ru_idrss,
        wUsage.ru_isrss,
        wUsage.ru_minflt,
        wUsage.ru_majflt,
        wUsage.ru_nswap,
        wUsage.ru_inblock,
        wUsage.ru_oublock,
        wUsage.ru_msgsnd,
        wUsage.ru_msgrcv,
        wUsage.ru_nsignals,
        wUsage.ru_nvcsw,
        wUsage.ru_nvcsw);
}

/**
 * Outputs a log entry at regular intervals to track the CPU usage over each
 *  interval for the Wrapper and its JVM.
 */
void wrapperDumpCPUUsage() {
    struct rusage wUsage;
    struct rusage jUsage;

    if (getrusage(RUSAGE_SELF, &wUsage)) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
            TEXT("Call to getrusage failed for Wrapper process: %s"), getLastErrorText());
        return;
    }
    /* The Children is only going to show the value for terminated children. */
    if (getrusage(RUSAGE_CHILDREN, &jUsage)) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
            TEXT("Call to getrusage failed for Java process: %s"), getLastErrorText());
        return;
    }

    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
        TEXT("Wrapper CPU: system %ld.%03ld, user %ld.%03ld  Java CPU: system %ld.%03ld, user %ld.%03ld"),
        wUsage.ru_stime.tv_sec, wUsage.ru_stime.tv_usec / 1000,
        wUsage.ru_utime.tv_sec, wUsage.ru_utime.tv_usec / 1000,
        jUsage.ru_stime.tv_sec, jUsage.ru_stime.tv_usec / 1000,
        jUsage.ru_utime.tv_sec, jUsage.ru_utime.tv_usec / 1000);
}

/**
 * Checks on the status of the JVM Process.
 * Returns WRAPPER_PROCESS_UP or WRAPPER_PROCESS_DOWN
 */
int wrapperGetProcessStatus(TICKS nowTicks, int sigChild) {
    int retval;
    int status;
    int exitCode;
    int res;
    
    if (wrapperData->javaPID <= 0) {
        /* We do not think that a JVM is currently running so return that it is down.
         * If we call waitpid with 0, it will wait for any child and cause problems with the event commands. */
        return WRAPPER_PROCESS_DOWN;
    }

    retval = waitpid(wrapperData->javaPID, &status, WNOHANG | WUNTRACED);
    if (retval == 0) {
        /* Up and running. */
        if (sigChild && wrapperData->jvmStopped) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("JVM process was continued."));
            wrapperData->jvmStopped = FALSE;
        }
        res = WRAPPER_PROCESS_UP;
    } else if (retval < 0) {
        if (errno == ECHILD) {
            if (wrapperData->jState == WRAPPER_JSTATE_DOWN_CHECK ||
                wrapperData->jState == WRAPPER_JSTATE_DOWN_CLEAN ||
                wrapperData->jState == WRAPPER_JSTATE_STOPPED) {
                res = WRAPPER_PROCESS_DOWN;
                wrapperJVMProcessExited(nowTicks, 0);
                return res;
            } else {
                /* Process is gone.  Happens after a SIGCHLD is handled. Normal. */
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("JVM process is gone."));
            }
        } else {
            /* Error requesting the status. */
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Unable to request JVM process status: %s"), getLastErrorText());
        }
        exitCode = 1;
        res = WRAPPER_PROCESS_DOWN;
        wrapperJVMProcessExited(nowTicks, exitCode);
    } else {
#ifdef _DEBUG
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  WIFEXITED=%d"), WIFEXITED(status));
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  WIFSTOPPED=%d"), WIFSTOPPED(status));
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  WIFSIGNALED=%d"), WIFSIGNALED(status));
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  WTERMSIG=%d"), WTERMSIG(status));
#endif

        /* Get the exit code of the process. */
        if (WIFEXITED(status)) {
            /* JVM has exited. */
            exitCode = WEXITSTATUS(status);
            res = WRAPPER_PROCESS_DOWN;

            wrapperJVMProcessExited(nowTicks, exitCode);
        } else if (WIFSIGNALED(status)) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                TEXT("JVM received a signal %s (%d)."), getSignalName(WTERMSIG(status)), WTERMSIG(status));
            res = WRAPPER_PROCESS_UP;
        } else if (WIFSTOPPED(status)) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                TEXT("JVM process was stopped.  It will be killed if the ping timeout expires."));
            wrapperData->jvmStopped = TRUE;
            res = WRAPPER_PROCESS_UP;
        } else {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                TEXT("JVM process signaled the Wrapper unexpectedly."));
            res = WRAPPER_PROCESS_UP;
        }
    }

    return res;
}

/**
 * This function does nothing on Unix machines.
 */
void wrapperReportStatus(int useLoggerQueue, int status, int errorCode, int waitHint) {
    return;
}

/**
 * Reads a single block of data from the child pipe.
 *
 * @param blockBuffer Pointer to the buffer where the block will be read.
 * @param blockSize Maximum number of bytes to read.
 * @param readCount Pointer to an int which will hold the number of bytes
 *                  actually read by the call.
 *
 * Returns TRUE if there were any problems, FALSE otherwise.
 */
int wrapperReadChildOutputBlock(char *blockBuffer, int blockSize, int *readCount) {
    if (jvmOut == -1) {
        /* The child is not up. */
        *readCount = 0;
        return FALSE;
    }

#if defined OPENBSD || defined FREEBSD
    /* Work around FreeBSD Bug #kern/64313
     *  http://www.freebsd.org/cgi/query-pr.cgi?pr=kern/64313
     *
     * When linked with the pthreads library the O_NONBLOCK flag is being reset
     *  on the jvmOut handle.  Not sure yet of the exact event that is causing
     *  this, but once it happens reads will start to block even though calls
     *  to fcntl(jvmOut, F_GETFL) say that the O_NONBLOCK flag is set.
     * Calling fcntl(jvmOut, F_SETFL, O_NONBLOCK) again will set the flag back
     *  again and cause it to start working correctly.  This may only need to
     *  be done once, however, because F_GETFL does not return the accurate
     *  state there is no reliable way to check.  Be safe and always set the
     *  flag. */
    if (fcntl(jvmOut, F_SETFL, O_NONBLOCK) < 0) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT(
            "Failed to set JVM output handle to non blocking mode to read child process output: %s (%d)"),
            getLastErrorText(), errno);
        return TRUE;
    }
#endif

    /* Fill read buffer. */
    *readCount = read(jvmOut, blockBuffer, blockSize);

    if (*readCount <= 0) {
        /* No more bytes available, return for now.  But make sure that this was not an error. */
        if (errno == EAGAIN) {
            /* Normal, the call would have blocked as there is no data available. */
        } else {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT(
                "Failed to read console output from the JVM: %s (%d)"),
                getLastErrorText(), errno);
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Transform a program into a daemon.
 * Inspired by code from GNU monit, which in turn, was
 * inspired by code from Stephen A. Rago's book,
 * Unix System V Network Programming.
 *
 * The idea is to first fork, then make the child a session leader,
 * and then fork again, so that it, (the session group leader), can
 * exit. This means that we, the grandchild, as a non-session group
 * leader, can never regain a controlling terminal.
 */
void daemonize(int argc, TCHAR** argv) {
    pid_t pid;
    int fd;

    /* Set the auto close flag and close the logfile before doing any forking to avoid
     *  duplicate open files. */
    setLogfileAutoClose(TRUE);
    closeLogfile();

    /* first fork */
    if (wrapperData->isDebugging) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Spawning intermediate process..."));
    }
    if ((pid = fork()) < 0) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Could not spawn daemon process: %s"),
            getLastErrorText());
        appExit(1, argc, argv);
    } else if (pid != 0) {
        /* Intermediate process is now running.  This is the original process, so exit. */

        /* If the main process was not launched in the background, then we want to make
         * the console output look nice by making sure that all output from the
         * intermediate and daemon threads are complete before this thread exits.
         * Sleep for 0.5 seconds. */
        wrapperSleep(500);

        /* Call exit rather than appExit as we are only exiting this process. */
        exit(0);
    }

    /* become session leader */
    if (setsid() == -1) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("setsid() failed: %s"),
           getLastErrorText());
        appExit(1, argc, argv);
    }

    signal(SIGHUP, SIG_IGN); /* don't let future opens allocate controlling terminals */

    /* Redirect stdin, stdout and stderr before closing to prevent the shell which launched
     *  the Wrapper from hanging when it exits. */
    fd = _topen(TEXT("/dev/null"), O_RDWR, 0);
    if (fd != -1) {
        close(STDIN_FILENO);
        dup2(fd, STDIN_FILENO);
        close(STDOUT_FILENO);
        dup2(fd, STDOUT_FILENO);
        close(STDERR_FILENO);
        dup2(fd, STDERR_FILENO);
        if (fd != STDIN_FILENO &&
            fd != STDOUT_FILENO &&
            fd != STDERR_FILENO) {
            close(fd);
        }
    }
    /* Console output was disabled above, so make sure the console log output is disabled
     *  so we don't waste any CPU formatting and sending output to '/dev/null'/ */
    setConsoleLogLevelInt(LEVEL_NONE);

    /* second fork */
    if (wrapperData->isDebugging) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Spawning daemon process..."));
    }
    if ((pid = fork()) < 0) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Could not spawn daemon process: %s"),
            getLastErrorText());
        appExit(1, argc, argv);
    } else if (pid != 0) {
        /* Daemon process is now running.  This is the intermediate process, so exit. */
        /* Call exit rather than appExit as we are only exiting this process. */
        exit(0);
    }

    /* Restore the auto close flag in the daemonized process. */
    setLogfileAutoClose(wrapperData->logfileInactivityTimeout <= 0);
}


/**
 * Sets the working directory to that of the current executable
 */
int setWorkingDir(TCHAR *app) {
    TCHAR szPath[PATH_MAX + 1];
    TCHAR* pos;

    /* Get the full path and filename of this program */
    if (_trealpath(app, szPath) == NULL) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to get the path for '%s'-%s"),
            app, getLastErrorText());
        return 1;
    }

    /* The wrapperData->isDebugging flag will never be set here, so we can't really use it. */
#ifdef _DEBUG
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Executable Name: %s"), szPath);
#endif

    /* To get the path, strip everything off after the last '\' */
    pos = _tcsrchr(szPath, TEXT('/'));
    if (pos == NULL) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to extract path from: %s"), szPath);
        return 1;
    } else {
        /* Clip the path at the position of the last backslash */
        pos[0] = (TCHAR)0;
    }

    /* Set a variable to the location of the binary. */
    setEnv(TEXT("WRAPPER_BIN_DIR"), szPath, ENV_SOURCE_WRAPPER);

    if (wrapperSetWorkingDir(szPath)) {
        return 1;
    }

    return 0;
}

/*******************************************************************************
 * Main function                                                               *
 *******************************************************************************/
#ifndef CUNIT
#ifdef UNICODE
int main(int argc, char **cargv) {
#else
int main(int argc, char **argv) {
#endif
#if defined(_DEBUG) || defined(UNICODE)
    int i;
#endif
    TCHAR *retLocale;
    int localeSet;
#ifdef UNICODE
    size_t req;
    TCHAR **argv;
    
    /* Create UNICODE versions of the argv array for internal use. */
    argv = malloc(argc * sizeof *argv );
    if(!argv) {
        _tprintf(TEXT("Out of Memory in Main\n"));
        appExit(1, 0, NULL);
        return 1;
    }
    for (i = 0; i < argc; i++) {
        req = mbstowcs(NULL, cargv[i], 0);
        argv[i] = malloc((int)(req + 1) * sizeof(TCHAR));
        if (!argv[i]) {
            _tprintf(TEXT("Out of Memory in Main\n"));
            while(--i > 0) {
                free(argv[i]);
            }
            free(argv);
            appExit(1, 0, argv);
            return 1;
        }
        mbstowcs(argv[i], cargv[i], (req + 1) * sizeof(TCHAR));
    }

#endif
    retLocale = _tsetlocale(LC_ALL, TEXT(""));
    if (retLocale) {
#if defined(UNICODE)
        free(retLocale);
#endif
        localeSet = TRUE;
    } else {
        /* TODO - We need to be careful about LANG here as it is not set on all systems. */
        /*
        envLang = _tgetenv(TEXT("LANG"));
        _tprintf(TEXT("Can't set the locale(%s); make sure $LC_* and $LANG are correct.\n"), envLang);
#if defined(UNICODE)
        free(envLang);
#endif
        */
        localeSet = FALSE;
    }
    if (wrapperInitialize()) {
        appExit(1, argc, argv);
        return 1; /* For compiler. */
    }

    /* Main thread initialized in wrapperInitialize. */

#ifdef _DEBUG
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Wrapper DEBUG build!"));
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Logging initialized."));
#endif
    /* Get the current process. */
    wrapperData->wrapperPID = getpid();

    if (setWorkingDir(argv[0])) {
        appExit(1, argc, argv);
        return 1; /* For compiler. */
    }
#ifdef _DEBUG
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Working directory set."));
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Arguments:"));
    for (i = 0; i < argc; i++) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  argv[%d]=%s"), i, argv[i]);
    }
#endif
    /* Parse the command and configuration file from the command line. */
    if (!wrapperParseArguments(argc, argv)) {
        appExit(1, argc, argv);
        return 1; /* For compiler. */
    }
    wrapperLoadHostName();
    if (!strcmpIgnoreCase(wrapperData->argCommand, TEXT("-translate"))) {
        /* We want to disable all log output when a translation request is made. */
        setSilentLogLevels();
    }
    /* At this point, we have a command, confFile, and possibly additional arguments. */
    if (!strcmpIgnoreCase(wrapperData->argCommand, TEXT("?")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-help"))) {
        /* User asked for the usage. */
        setSimpleLogLevels();
        wrapperUsage(argv[0]);
        appExit(0, argc, argv);
        return 0; /* For compiler. */
    } else if (!strcmpIgnoreCase(wrapperData->argCommand, TEXT("v")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-version"))) {
        /* User asked for version. */
        setSimpleLogLevels();
        wrapperVersionBanner();
        appExit(0, argc, argv);
        return 0; /* For compiler. */
    } else if (!strcmpIgnoreCase(wrapperData->argCommand, TEXT("h")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-hostid"))) {
        /* User asked for version. */
        setSimpleLogLevels();
        wrapperVersionBanner();
        showHostIds(LEVEL_STATUS);
        appExit(0, argc, argv);
        return 0; /* For compiler. */
    }

    /* Load the properties. */
    if (wrapperLoadConfigurationProperties(FALSE)) {
        /* Unable to load the configuration.  Any errors will have already
         *  been reported. */
        if (wrapperData->argConfFileDefault && !wrapperData->argConfFileFound) {
            /* The config file that was being looked for was default and
             *  it did not exist.  Show the usage. */
            wrapperUsage(argv[0]);
        }
        appExit(1, argc, argv);
        return 1; /* For compiler. */
    }

    /* Set the default umask of the Wrapper process. */
    umask(wrapperData->umask);
    if (!strcmpIgnoreCase(wrapperData->argCommand, TEXT("-translate"))) {
        setSimpleLogLevels();
        /* Print out the string so the caller sees it as its translated output. */
        _tprintf(TEXT("%s"), argv[2]);
        appExit(0, argc, argv);
        return 0; /* For compiler. */
    } else if (!strcmpIgnoreCase(wrapperData->argCommand, TEXT("c")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-console"))) {
        /* Run as a console application */

        /* fork to a Daemonized process if configured to do so. */
        if (wrapperData->daemonize) {
            daemonize(argc, argv);

            /* When we daemonize the Wrapper, its PID changes. Because of the
             *  WRAPPER_PID environment variable, we need to set it again here
             *  and then reload the configuration in case the PID is referenced
             *  in the configuration. */

            /* Get the current process. */
            wrapperData->wrapperPID = getpid();

            if (wrapperData->isDebugging) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Reloading configuration."));
            }
    
            /* If the working dir has been changed then we need to restore it before
             *  the configuration can be reloaded.  This is needed to support relative
             *  references to include files. */
            if (wrapperData->workingDir && wrapperData->originalWorkingDir) {
                if (wrapperSetWorkingDir(wrapperData->originalWorkingDir)) {
                    /* Failed to restore the working dir.  Shutdown the Wrapper */
                    appExit(1, argc, argv);
                    return 1; /* For compiler. */
                }
            }
    
            /* Load the properties. */
            if (wrapperLoadConfigurationProperties(FALSE)) {
                /* Unable to load the configuration.  Any errors will have already
                 *  been reported. */
                if (wrapperData->argConfFileDefault && !wrapperData->argConfFileFound) {
                    /* The config file that was being looked for was default and
                     *  it did not exist.  Show the usage. */
                    wrapperUsage(argv[0]);
                }
                appExit(1, argc, argv);
                return 1; /* For compiler. */
            }
        }


        /* See if the logs should be rolled on Wrapper startup. */
        if ((getLogfileRollMode() & ROLL_MODE_WRAPPER) ||
            (getLogfileRollMode() & ROLL_MODE_JVM)) {
            rollLogs();
        }

        /* Write pid and anchor files as requested.  If they are the same file the file is
         *  simply overwritten. */
        if (wrapperData->anchorFilename) {
            if (writePidFile(wrapperData->anchorFilename, wrapperData->wrapperPID, wrapperData->anchorFileUmask)) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                     TEXT("ERROR: Could not write anchor file %s: %s"),
                     wrapperData->anchorFilename, getLastErrorText());
                appExit(1, argc, argv);
                return 1; /* For compiler. */
            }
        }
        if (wrapperData->pidFilename) {
            if (writePidFile(wrapperData->pidFilename, wrapperData->wrapperPID, wrapperData->pidFileUmask)) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                     TEXT("ERROR: Could not write pid file %s: %s"),
                     wrapperData->pidFilename, getLastErrorText());
                appExit(1, argc, argv);
                return 1; /* For compiler. */
            }
        }
        if (wrapperData->lockFilename) {
            if (writePidFile(wrapperData->lockFilename, wrapperData->wrapperPID, wrapperData->lockFileUmask)) {
                /* This will fail if the user is running as a user without full privileges.
                 *  To make things easier for user configuration, this is ignored if sufficient
                 *  privileges do not exist. */
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                     TEXT("WARNING: Could not write lock file %s: %s"),
                     wrapperData->lockFilename, getLastErrorText());
                wrapperData->lockFilename = NULL;
            }
        }

        appExit(wrapperRunConsole(), argc, argv);
        return 0; /* For compiler. */
    } else {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT(""));
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Unrecognized option: -%s"), wrapperData->argCommand);
        wrapperUsage(argv[0]);
        appExit(1, argc, argv);
        return 1; /* For compiler. */
    }
}
#endif

#endif /* ifndef WIN32 */
