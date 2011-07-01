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
 * This file contains the main event loop and state engine for
 *  the Java Service Wrapper.
 *
 * Author:
 *   Leif Mortenson <leif@tanukisoftware.com>
 *   Ryan Shaw
 */

#include <stdio.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>

#ifdef WIN32
#include <io.h>

/* MS Visual Studio 8 went and deprecated the POXIX names for functions.
 *  Fixing them all would be a big headache for UNIX versions. */
#pragma warning(disable : 4996)

#else /* UNIX */
#include <unistd.h>
#include <stdlib.h>
#endif
#include "wrapper_i18n.h"
#include "wrapper.h"
#include "logger.h"

#ifndef MAX
#define MAX(a, b)  (((a) > (b)) ? (a) : (b))
#endif

/**
 * Returns a constant text representation of the specified Wrapper State.
 *
 * @param wState The Wrapper State whose name is being requested.
 *
 * @return Thre requested Wrapper State.
 */
const TCHAR *wrapperGetWState(int wState) {
    const TCHAR *name;
    switch(wState) {
    case WRAPPER_WSTATE_STARTING:
        name = TEXT("STARTING");
        break;
    case WRAPPER_WSTATE_STARTED:
        name = TEXT("STARTED");
        break;
    case WRAPPER_WSTATE_PAUSING:
        name = TEXT("PAUSING");
        break;
    case WRAPPER_WSTATE_PAUSED:
        name = TEXT("PAUSED");
        break;
    case WRAPPER_WSTATE_RESUMING:
        name = TEXT("RESUMING");
        break;
    case WRAPPER_WSTATE_STOPPING:
        name = TEXT("STOPPING");
        break;
    case WRAPPER_WSTATE_STOPPED:
        name = TEXT("STOPPED");
        break;
    default:
        name = TEXT("UNKNOWN");
        break;
    }
    return name;
}

/**
 * Returns a constant text representation of the specified Java State.
 *
 * @param jState The Java State whose name is being requested.
 *
 * @return Thre requested Java State.
 */
const TCHAR *wrapperGetJState(int jState) {
    const TCHAR *name;
    switch(jState) {
    case WRAPPER_JSTATE_DOWN_CLEAN:
        name = TEXT("DOWN_CLEAN");
        break;
    case WRAPPER_JSTATE_LAUNCH_DELAY:
        name = TEXT("LAUNCH(DELAY)");
        break;
    case WRAPPER_JSTATE_RESTART:
        name = TEXT("RESTART");
        break;
    case WRAPPER_JSTATE_LAUNCH:
        name = TEXT("LAUNCH");
        break;
    case WRAPPER_JSTATE_LAUNCHING:
        name = TEXT("LAUNCHING");
        break;
    case WRAPPER_JSTATE_LAUNCHED:
        name = TEXT("LAUNCHED");
        break;
    case WRAPPER_JSTATE_STARTING:
        name = TEXT("STARTING");
        break;
    case WRAPPER_JSTATE_STARTED:
        name = TEXT("STARTED");
        break;
    case WRAPPER_JSTATE_STOP:
        name = TEXT("STOP");
        break;
    case WRAPPER_JSTATE_STOPPING:
        name = TEXT("STOPPING");
        break;
    case WRAPPER_JSTATE_STOPPED:
        name = TEXT("STOPPED");
        break;
    case WRAPPER_JSTATE_KILLING:
        name = TEXT("KILLING");
        break;
    case WRAPPER_JSTATE_KILL:
        name = TEXT("KILL");
        break;
    case WRAPPER_JSTATE_DOWN_CHECK:
        name = TEXT("DOWN_CHECK");
        break;
    default:
        name = TEXT("UNKNOWN");
        break;
    }
    return name;
}


void writeStateFile(const TCHAR *filename, const TCHAR *state, int newUmask) {
    FILE *fp = NULL;
    int old_umask;
    int cnt = 0;

    /* If other processes are reading the state file it may be locked for a moment.
     *  Avoid problems by trying a few times before giving up. */
    while (cnt < 10) {
#ifdef WIN32
        old_umask = umask(newUmask);
        fp = _tfopen(filename, TEXT("w"));
        umask(old_umask);
#else
        old_umask = umask(newUmask);
        fp = _tfopen(filename, TEXT("w"));
        umask(old_umask);
#endif

        if (fp != NULL) {
            _ftprintf(fp, TEXT("%s\n"), state);
            fclose(fp);

            return;
        }

        /* Sleep for a tenth of a second. */
        wrapperSleep(100);

        cnt++;
    }

    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Unable to write to the status file: %s"), filename);
}

/**
 * Changes the current Wrapper state.
 *
 * wState - The new Wrapper state.
 */
void wrapperSetWrapperState(int wState) {
    if (wrapperData->isStateOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
            TEXT("      Set Wrapper State %s -> %s"),
            wrapperGetWState(wrapperData->wState),
            wrapperGetWState(wState));
    }

    wrapperData->wState = wState;

    if (wrapperData->statusFilename != NULL) {
        writeStateFile(wrapperData->statusFilename, wrapperGetWState(wrapperData->wState), wrapperData->statusFileUmask);
    }
}

/**
 * Updates the current state time out.
 *
 * nowTicks - The current tick count at the time of the call, ignored if
 *            delay is negative.
 * delay - The delay in seconds, added to the nowTicks after which the state
 *         will time out, if negative will never time out.
 */
void wrapperUpdateJavaStateTimeout(TICKS nowTicks, int delay) {
    TICKS newTicks;
    int ignore;
    int tickAge;

    if (delay >= 0) {
        newTicks = wrapperAddToTicks(nowTicks, delay);
        ignore = FALSE;
        if (wrapperData->jStateTimeoutTicksSet) {
            /* We need to make sure that the new delay is longer than the existing one.
             *  This is complicated slightly because the tick counter can be wrapped. */
            tickAge = wrapperGetTickAgeTicks(wrapperData->jStateTimeoutTicks, newTicks);
            if (tickAge <= 0) {
                ignore = TRUE;
            }
        }

        if (ignore) {
            /* The new value is meaningless. */
            if (wrapperData->isStateOutputEnabled) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                    TEXT("      Set Java State %s (%d) Ignored Timeout %08x"),
                    wrapperGetJState(wrapperData->jState),
                    delay,
                    wrapperData->jStateTimeoutTicks);
            }
        } else {
            if (wrapperData->isStateOutputEnabled) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                    TEXT("      Set Java State %s (%d) Timeout %08x -> %08x"),
                    wrapperGetJState(wrapperData->jState),
                    delay,
                    nowTicks,
                    delay,
                    newTicks);
            }

            wrapperData->jStateTimeoutTicks = newTicks;
            wrapperData->jStateTimeoutTicksSet = 1;
        }
    } else {
        wrapperData->jStateTimeoutTicks = 0;
        wrapperData->jStateTimeoutTicksSet = 0;
    }
}

/**
 * Changes the current Java state.
 *
 * jState - The new Java state.
 * nowTicks - The current tick count at the time of the call, ignored if
 *            delay is negative.
 * delay - The delay in seconds, added to the nowTicks after which the state
 *         will time out, if negative will never time out.
 */
void wrapperSetJavaState(int jState, TICKS nowTicks, int delay) {
    if (wrapperData->isStateOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("      Set Java State %s -> %s"),
            wrapperGetJState(wrapperData->jState),
            wrapperGetJState(jState));
    }

    if (wrapperData->jState != jState) {
        /* If the state has changed, then the old timeout will never be used.
         *  Clear it here so any new timeout will be used. */
        wrapperData->jStateTimeoutTicks = 0;
        wrapperData->jStateTimeoutTicksSet = 0;
    }
    wrapperData->jState = jState;
    wrapperUpdateJavaStateTimeout(nowTicks, delay);

    if (wrapperData->javaStatusFilename != NULL) {
        writeStateFile(wrapperData->javaStatusFilename, wrapperGetJState(wrapperData->jState), wrapperData->javaStatusFileUmask);
    }
}

void displayLaunchingTimeoutMessage() {
    const TCHAR *mainClass;

    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
        TEXT("Startup failed: Timed out waiting for a signal from the JVM."));

    mainClass = getStringProperty(properties, TEXT("wrapper.java.mainclass"), TEXT("Main"));

    if ((_tcsstr(mainClass, TEXT("org.tanukisoftware.wrapper.WrapperSimpleApp")) != NULL)
        || (_tcsstr(mainClass, TEXT("org.tanukisoftware.wrapper.WrapperStartStopApp")) != NULL)) {

        /* The user appears to be using a valid main class, so no advice available. */
    } else {
        if (wrapperData->isAdviserEnabled) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE, TEXT("") );
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                TEXT("------------------------------------------------------------------------") );
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                TEXT("Advice:") );
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                TEXT("The Wrapper consists of a native component as well as a set of classes\nwhich run within the JVM that it launches.  The Java component of the\nWrapper must be initialized promptly after the JVM is launched or the\nWrapper will timeout, as just happened.  Most likely the main class\nspecified in the Wrapper configuration file is not correctly initializing\nthe Wrapper classes:"));
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                TEXT("    %s"), mainClass);
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                TEXT("While it is possible to do so manually, the Wrapper ships with helper\nclasses to make this initialization processes automatic.\nPlease review the integration section of the Wrapper's documentation\nfor the various methods which can be employed to launch an application\nwithin the Wrapper:"));
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                TEXT("    http://wrapper.tanukisoftware.com/doc/english/integrate.html"));
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                TEXT("------------------------------------------------------------------------") );
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE, TEXT("") );
        }
    }
}

/**
 * Handles a timeout for a DebugJVM by showing an appropriate message and
 *  resetting internal timeouts.
 */
void handleDebugJVMTimeout(TICKS nowTicks, const TCHAR *message, const TCHAR *timer) {
    if (!wrapperData->debugJVMTimeoutNotified) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
            TEXT("------------------------------------------------------------------------") );
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("%s"), message);
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
            TEXT("The JVM was launched with debug options so this may be because the JVM\nis currently suspended by a debugger.  Any future timeouts during this\nJVM invocation will be silently ignored."));
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
            TEXT("------------------------------------------------------------------------") );
    }
    wrapperData->debugJVMTimeoutNotified = TRUE;

    /* Make this individual state never timeout then continue. */
    if (wrapperData->isStateOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
            TEXT("      DebugJVM timeout.  Disable current %s timeout."), timer);
    }
    wrapperUpdateJavaStateTimeout(nowTicks, -1);
}

/**
 * Tests for the existence of the anchor file.  If it does not exist then
 *  the Wrapper will begin its shutdown process.
 *
 * nowTicks: The tick counter value this time through the event loop.
 */
void anchorPoll(TICKS nowTicks) {
#if defined(WIN32) && !defined(WIN64)
    struct _stat64i32 fileStat;
#else
    struct stat fileStat;
#endif
    int result;

#ifdef _DEBUG
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO,
        TEXT("Anchor timeout=%d, now=%d"), wrapperData->anchorTimeoutTicks, nowTicks);
#endif

    if (wrapperData->anchorFilename) {
        if (wrapperTickExpired(nowTicks, wrapperData->anchorTimeoutTicks)) {
            if (wrapperData->isLoopOutputEnabled) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Loop: check anchor file"));
            }

            result = _tstat(wrapperData->anchorFilename, &fileStat);
            if (result == 0) {
                /* Anchor file exists.  Do nothing. */
#ifdef _DEBUG
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO,
                    TEXT("The anchor file %s exists."), wrapperData->anchorFilename);
#endif
            } else {
                /* Anchor file is gone. */
#ifdef _DEBUG
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO,
                    TEXT("The anchor file %s was deleted."), wrapperData->anchorFilename);
#endif

                /* Unless we are already doing so, start the shudown process. */
                if (wrapperData->exitRequested || wrapperData->restartRequested ||
                    (wrapperData->jState == WRAPPER_JSTATE_STOP) ||
                    (wrapperData->jState == WRAPPER_JSTATE_STOPPING) ||
                    (wrapperData->jState == WRAPPER_JSTATE_STOPPED) ||
                    (wrapperData->jState == WRAPPER_JSTATE_KILLING) ||
                    (wrapperData->jState == WRAPPER_JSTATE_KILL) ||
                    (wrapperData->jState == WRAPPER_JSTATE_DOWN_CHECK) ||
                    (wrapperData->jState == WRAPPER_JSTATE_DOWN_CLEAN)) {
                    /* Already shutting down, so nothing more to do. */
                } else {
                    /* Always force the shutdown as this was an external event. */
                    wrapperStopProcess(0, TRUE);
                }

                /* To make sure that the JVM will not be restarted for any reason,
                 *  start the Wrapper shutdown process as well. */
                if ((wrapperData->wState == WRAPPER_WSTATE_STOPPING) ||
                    (wrapperData->wState == WRAPPER_WSTATE_STOPPED)) {
                    /* Already stopping. */
                } else {
                    /* Start the shutdown process. */
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Anchor file deleted.  Shutting down."));
                    
                    wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
                }
            }

            wrapperData->anchorTimeoutTicks = wrapperAddToTicks(nowTicks, wrapperData->anchorPollInterval);
        }
    }
}

/**
 * Tests for the existence of the command file.  If it exists then it will be
 *  opened and any included commands will be processed.  On completion, the
 *  file will be deleted.
 *
 * nowTicks: The tick counter value this time through the event loop.
 */
#define MAX_COMMAND_LENGTH 80
void commandPoll(TICKS nowTicks) {
#if defined(WIN32) && !defined(WIN64)
    struct _stat64i32 fileStat;
#else
    struct stat fileStat;
#endif
    int result;
    FILE *stream;
    int cnt;
    TCHAR buffer[MAX_COMMAND_LENGTH];
    TCHAR *c;
    TCHAR *d;
    TCHAR *command;
    TCHAR *param1;
    TCHAR *param2;
    int exitCode;
    int pauseTime;
    int logLevel;
    int oldLowLogLevel;
    int newLowLogLevel;
    int flag;
    int accessViolation = FALSE;

#ifdef _DEBUG
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO,
        TEXT("Command timeout=%08x, now=%08x"), wrapperData->commandTimeoutTicks, nowTicks);
#endif

    if (wrapperData->commandFilename) {
        if (wrapperTickExpired(nowTicks, wrapperData->commandTimeoutTicks)) {
            if (wrapperData->isLoopOutputEnabled) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Loop: check command file"));
            }

            result = _tstat(wrapperData->commandFilename, &fileStat);
            if (result == 0) {
                /* Command file exists. */
#ifdef _DEBUG
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO,
                    TEXT("The command file %s exists."), wrapperData->commandFilename);
#endif
                /* We need to be able to lock and then read the command file.  Other
                 *  applications will be creating this file so we need to handle the
                 *  case where it is locked for a few moments. */
                cnt = 0;
                do {
                    stream = _tfopen(wrapperData->commandFilename, TEXT("r+t"));
                    if (stream == NULL) {
                        /* Sleep for a tenth of a second. */
                        wrapperSleep(100);
                    }

                    cnt++;
                } while ((cnt < 10) && (stream == NULL));

                if (stream == NULL) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                        TEXT("Unable to read the command file: %s"), wrapperData->commandFilename);
                } else {
                    /* Read in each of the commands line by line. */
                    do {
                        c = _fgetts(buffer, MAX_COMMAND_LENGTH, stream);
                        if (c != NULL) {
                            /* Always strip both ^M and ^J off the end of the line, this is done rather
                             *  than simply checking for \n so that files will work on all platforms
                             *  even if their line feeds are incorrect. */
                            if ((d = _tcschr(buffer, 13 /* ^M */)) != NULL) {
                                d[0] = TEXT('\0');
                            }
                            if ((d = _tcschr(buffer, 10 /* ^J */)) != NULL) {
                                d[0] = TEXT('\0');
                            }

                            command = buffer;

                            /** Look for the first space, everything after it will be the parameter(s). */
                            /* Look for parameter 1. */
                            if ((param1 = _tcschr(buffer, ' ')) != NULL ) {
                                param1[0] = TEXT('\0'); /* Terminate the command. */

                                /* Find the first non-space character. */
                                do {
                                    param1++;
                                } while (param1[0] == TEXT(' '));
                            }
                            if (param1 != NULL) {
                                /* Look for parameter 2. */
                                if ((param2 = _tcschr(param1, ' ')) != NULL ) {
                                    param2[0] = TEXT('\0'); /* Terminate param1. */
    
                                    /* Find the first non-space character. */
                                    do {
                                        param2++;
                                    } while (param2[0] == TEXT(' '));
                                }
                                if (param2 != NULL) {
                                    /* Make sure parameter 2 is terminated. */
                                    if ((d = _tcschr(param2, ' ')) != NULL ) {
                                        d[0] = TEXT('\0'); /* Terminate param2. */
                                    }
                                }
                            } else {
                                param2 = NULL;
                            }

                            /* Process the command. */
                            if (strcmpIgnoreCase(command, TEXT("RESTART")) == 0) {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Command '%s'. Restarting JVM."), command);
                                wrapperRestartProcess();
                            } else if (strcmpIgnoreCase(command, TEXT("STOP")) == 0) {
                                if (param1 == NULL) {
                                    exitCode = 0;
                                } else {
                                    exitCode = _ttoi(param1);
                                }
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Command '%s'. Shutting down with exit code %d."), command, exitCode);

                                /* Always force the shutdown as this is an external event. */
                                wrapperStopProcess(exitCode, TRUE);
                                
                                /* To make sure that the JVM will not be restarted for any reason,
                                 *  start the Wrapper shutdown process as well. */
                                if ((wrapperData->wState == WRAPPER_WSTATE_STOPPING) ||
                                    (wrapperData->wState == WRAPPER_WSTATE_STOPPED)) {
                                    /* Already stopping. */
                                } else {
                                    wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
                                }
                            } else if (strcmpIgnoreCase(command, TEXT("PAUSE")) == 0) {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Command '%s'. Pausing JVM."), command);
                                wrapperPauseProcess(WRAPPER_ACTION_SOURCE_CODE_COMMANDFILE);
                            } else if (strcmpIgnoreCase(command, TEXT("RESUME")) == 0) {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Command '%s'. Resuming JVM."), command);
                                wrapperResumeProcess(WRAPPER_ACTION_SOURCE_CODE_COMMANDFILE);
                            } else if (strcmpIgnoreCase(command, TEXT("DUMP")) == 0) {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Command '%s'. Requesting a Thread Dump."), command);
                                wrapperRequestDumpJVMState();
                            } else if (strcmpIgnoreCase(command, TEXT("GC")) == 0) {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Command '%s'. Requesting a GC."), command);
                                wrapperRequestJVMGC();
                            } else if ((strcmpIgnoreCase(command, TEXT("CONSOLE_LOGLEVEL")) == 0) ||
                                    (strcmpIgnoreCase(command, TEXT("LOGFILE_LOGLEVEL")) == 0) ||
                                    (strcmpIgnoreCase(command, TEXT("SYSLOG_LOGLEVEL")) == 0)) {
                                if (param1 == NULL) {
                                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Command '%s' is missing its log level."), command);
                                } else {
                                    logLevel = getLogLevelForName(param1);
                                    if (logLevel == LEVEL_UNKNOWN) {
                                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Command '%s' specified an unknown log level: '%'"), command, param1);
                                    } else {
                                        oldLowLogLevel = getLowLogLevel();

                                        if (strcmpIgnoreCase(command, TEXT("CONSOLE_LOGLEVEL")) == 0) {
                                            setConsoleLogLevelInt(logLevel);
                                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Command '%s'. Set console log level to '%s'."), command, param1);
                                        } else if (strcmpIgnoreCase(command, TEXT("LOGFILE_LOGLEVEL")) == 0) {
                                            setLogfileLevelInt(logLevel);
                                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Command '%s'. Set log file log level to '%s'."), command, param1);
                                        } else if (strcmpIgnoreCase(command, TEXT("SYSLOG_LOGLEVEL")) == 0) {
                                            setSyslogLevelInt(logLevel);
                                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Command '%s'. Set syslog log level to '%s'."), command, param1);
                                        } else {
                                            /* Shouldn't get here. */
                                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Command '%s' lead to an unexpected state."), command);
                                        }

                                        newLowLogLevel = getLowLogLevel();
                                        if (oldLowLogLevel != newLowLogLevel) {
                                            wrapperData->isDebugging = (newLowLogLevel <= LEVEL_DEBUG);

                                            _sntprintf(buffer, MAX_COMMAND_LENGTH, TEXT("%d"), getLowLogLevel());
                                            wrapperProtocolFunction(WRAPPER_MSG_LOW_LOG_LEVEL, buffer);
                                        }
                                    }
                                }
                            } else if ((strcmpIgnoreCase(command, TEXT("LOOP_OUTPUT")) == 0) ||
                                    (strcmpIgnoreCase(command, TEXT("STATE_OUTPUT")) == 0) ||
                                    (strcmpIgnoreCase(command, TEXT("MEMORY_OUTPUT")) == 0) ||
                                    (strcmpIgnoreCase(command, TEXT("CPU_OUTPUT")) == 0) ||
                                    (strcmpIgnoreCase(command, TEXT("TIMER_OUTPUT")) == 0) ||
                                    (strcmpIgnoreCase(command, TEXT("SLEEP_OUTPUT")) == 0)) {
                                flag = ((param1 != NULL) && (strcmpIgnoreCase(param1, TEXT("TRUE")) == 0));
                                if (strcmpIgnoreCase(command, TEXT("LOOP_OUTPUT")) == 0) {
                                    wrapperData->isLoopOutputEnabled = flag;
                                } else if (strcmpIgnoreCase(command, TEXT("STATE_OUTPUT")) == 0) {
                                    wrapperData->isStateOutputEnabled = flag;
                                } else if (strcmpIgnoreCase(command, TEXT("MEMORY_OUTPUT")) == 0) {
                                    wrapperData->isMemoryOutputEnabled = flag;
                                } else if (strcmpIgnoreCase(command, TEXT("CPU_OUTPUT")) == 0) {
                                    wrapperData->isCPUOutputEnabled = flag;
                                } else if (strcmpIgnoreCase(command, TEXT("TIMER_OUTPUT")) == 0) {
                                    wrapperData->isTickOutputEnabled = flag;
                                } else if (strcmpIgnoreCase(command, TEXT("SLEEP_OUTPUT")) == 0) {
                                    wrapperData->isSleepOutputEnabled = flag;
                                }
                                if (flag) {
                                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Command '%s'. Enable %s."), command, command);
                                } else {
                                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Command '%s'. Disable %s."), command, command);
                                }
                            } else if (strcmpIgnoreCase(command, TEXT("CLOSE_SOCKET")) == 0) {
                                if (wrapperData->commandFileTests) {
                                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Command '%s'.  Closing backend socket to JVM..."), command);
                                    wrapperProtocolClose();
                                } else {
                                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Command '%s'.  Tests disabled."), command);
                                }
                            } else if (strcmpIgnoreCase(command, TEXT("PAUSE_THREAD")) == 0) {
                                if (wrapperData->commandFileTests) {
                                    if (param2 == NULL) {
                                        pauseTime = -1;
                                    } else {
                                        pauseTime = __max(1, __min(3600, _ttoi(param2)));
                                    }
                                    if (strcmpIgnoreCase(param1, TEXT("MAIN")) == 0) {
                                        wrapperData->pauseThreadMain = pauseTime;
                                    } else if (strcmpIgnoreCase(param1, TEXT("TIMER")) == 0) {
                                        wrapperData->pauseThreadTimer = pauseTime;
                                    } else if (strcmpIgnoreCase(param1, TEXT("JAVAIO")) == 0) {
                                        wrapperData->pauseThreadJavaIO = pauseTime;
                                    } else {
                                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Command '%s'.  Enqueue request to pause unknown thread."), command);
                                        pauseTime = 0;
                                    }
                                    if (pauseTime > 0) {
                                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Command '%s'.  Enqueue request to pause %s thread for %d seconds..."), command, param1, pauseTime);
                                    } else if (pauseTime < 0) {
                                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Command '%s'.  Enqueue request to pause %s thread indefinitely..."), command, param1);
                                    }
                                } else {
                                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Command '%s'.  Tests disabled."), command);
                                }
                            } else if (strcmpIgnoreCase(command, TEXT("ACCESS_VIOLATION")) == 0) {
                                if (wrapperData->commandFileTests) {
                                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Command '%s'.  Intentionally causing an Access Violation in Wrapper..."), command);
                                    /* We can't do the access violation here because we want to make sure the
                                     *  file is deleted first, otherwise it be executed again when the Wrapper is restarted. */
                                    accessViolation = TRUE;
                                } else {
                                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Command '%s'.  Tests disabled."), command);
                                }
                            } else {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Command '%s' is unknown, ignoring."), command);
                            }
                        }
                    } while (c != NULL);

                    /* Close the file. */
                    fclose(stream);

                    /* Delete the file. */
                    if (_tremove(wrapperData->commandFilename) == -1) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                            TEXT("Unable to delete the command file, %s: %s"),
                            wrapperData->commandFilename, getLastErrorText());
                    }
                    
                    if (accessViolation) {
                        c = NULL;
                        c[0] = TEXT('\0');
                        /* Should never get here. */
                    }
                }
            } else {
                /* Command file does not exist. */
#ifdef _DEBUG
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO,
                    TEXT("The command file %s does not exist."), wrapperData->commandFilename);
#endif
            }

            wrapperData->commandTimeoutTicks = wrapperAddToTicks(nowTicks, wrapperData->commandPollInterval);
        }
    }
}

/********************************************************************
 * Wrapper States
 *******************************************************************/
/**
 * WRAPPER_WSTATE_STARTING
 * The Wrapper process is being started.  It will remain in this state
 *  until a JVM and its application has been successfully started.
 *
 * nowTicks: The tick counter value this time through the event loop.
 */
void wStateStarting(TICKS nowTicks) {
    int timeout;

    /* While the wrapper is starting up, we need to ping the service  */
    /*  manager to reasure it that we are still alive. */

    /* Tell the service manager that we are starting */
    if (wrapperData->startupTimeout > 0) {
        timeout = (wrapperData->startupTimeout) * 1000;
    } else {
        timeout = 86400000; /* Set infinity at 1 day. */
    }
    wrapperReportStatus(FALSE, WRAPPER_WSTATE_STARTING, 0, timeout);

    /* If we are supposed to pause on startup, we need to jump to that state now, and report that we are started. */
    if (wrapperData->initiallyPaused && wrapperData->pausable) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Initially Paused."));
        
        wrapperSetWrapperState(WRAPPER_WSTATE_PAUSED);

        /* Tell the service manager that we started */
        wrapperReportStatus(FALSE, WRAPPER_WSTATE_PAUSED, 0, 0);
    } else {
        /* If the JVM state is now STARTED, then change the wrapper state */
        /*  to be STARTED as well. */
        if (wrapperData->jState == WRAPPER_JSTATE_STARTED) {
            wrapperSetWrapperState(WRAPPER_WSTATE_STARTED);
    
            /* Tell the service manager that we started */
            wrapperReportStatus(FALSE, WRAPPER_WSTATE_STARTED, 0, 0);
        }
    }
}

/**
 * WRAPPER_WSTATE_STARTED
 * The Wrapper process is started.  It will remain in this state until
 *  the Wrapper is ready to start shutting down.  The JVM process may
 *  be restarted one or more times while the Wrapper is in this state.
 *
 * nowTicks: The tick counter value this time through the event loop.
 */
void wStateStarted(TICKS nowTicks) {
    /* Just keep running.  Nothing to do here. */
}

/**
 * WRAPPER_WSTATE_PAUSING
 * The Wrapper process is being paused.  If stopping the JVM is enabled
 *  then it will remain in this state until the JVM has been stopped.
 *  Otherwise it will immediately go to the WRAPPER_WSTATE_PAUSED state.
 *
 * nowTicks: The tick counter value this time through the event loop.
 */
void wStatePausing(TICKS nowTicks) {
    int timeout;

    /* While the wrapper is pausing, we need to ping the service  */
    /*  manager to reasure it that we are still alive. */

    /* If we are configured to do so, stop the JVM */
    if (wrapperData->pausableStopJVM) {
        /* If it has not already been set, set the exit request flag. */
        if (wrapperData->jState == WRAPPER_JSTATE_DOWN_CLEAN) {
            /* JVM is now down.  We are now paused. */
            wrapperSetWrapperState(WRAPPER_WSTATE_PAUSED);

            /* Tell the service manager that we are paused */
            wrapperReportStatus(FALSE, WRAPPER_WSTATE_PAUSED, 0, 0);
        } else {
            /* Tell the service manager that we are pausing */
            if ((wrapperData->shutdownTimeout <= 0) || (wrapperData->jvmExitTimeout <= 0)) {
                timeout = 86400000; /* Set infinity at 1 day. */
            } else {
                timeout = MAX(wrapperData->shutdownTimeout, wrapperData->jvmExitTimeout) * 1000;
            }
            wrapperReportStatus(FALSE, WRAPPER_WSTATE_PAUSING, 0, timeout);

            if (wrapperData->exitRequested ||
                (wrapperData->jState == WRAPPER_JSTATE_STOP) ||
                (wrapperData->jState == WRAPPER_JSTATE_STOPPING) ||
                (wrapperData->jState == WRAPPER_JSTATE_STOPPED) ||
                (wrapperData->jState == WRAPPER_JSTATE_KILLING) ||
                (wrapperData->jState == WRAPPER_JSTATE_KILL) ||
                (wrapperData->jState == WRAPPER_JSTATE_DOWN_CHECK)) {
                /* In the process of stopping the JVM. */
            } else {
                /* The JVM needs to be stopped, start that process. */
                wrapperData->exitRequested = TRUE;

                /* Make sure the JVM will be restarted. */
                wrapperData->restartRequested = WRAPPER_RESTART_REQUESTED_CONFIGURED;
            }
        }
    } else {
        /* We want to leave the JVM process as is.  We are now paused. */
        wrapperSetWrapperState(WRAPPER_WSTATE_PAUSED);

        /* Tell the service manager that we are paused */
        wrapperReportStatus(FALSE, WRAPPER_WSTATE_PAUSED, 0, 0);
    }
}

/**
 * WRAPPER_WSTATE_PAUSED
 * The Wrapper process is paused.  It will remain in this state until
 *  the Wrapper is resumed or is ready to start shutting down.  The
 *  JVM may be stopped or will remain stopped while the Wrapper is in
 *  this state.
 *
 * nowTicks: The tick counter value this time through the event loop.
 */
void wStatePaused(TICKS nowTicks) {
    /* Just keep running.  Nothing to do here. */
}

/**
 * WRAPPER_WSTATE_RESUMING
 * The Wrapper process is being resumed.  We will remain in this state
 *  until the JVM enters the running state.  It may or may not be initially
 *  started.
 *
 * nowTicks: The tick counter value this time through the event loop.
 */
void wStateResuming(TICKS nowTicks) {
    int timeout;

    /* While the wrapper is resuming, we need to ping the service  */
    /*  manager to reasure it that we are still alive. */

    /* If the JVM state is now STARTED, then change the wrapper state */
    /*  to be STARTED as well. */
    if (wrapperData->jState == WRAPPER_JSTATE_STARTED) {
        wrapperSetWrapperState(WRAPPER_WSTATE_STARTED);

        /* Tell the service manager that we started */
        wrapperReportStatus(FALSE, WRAPPER_WSTATE_STARTED, 0, 0);
    } else {
        /* JVM is down and so it needs to be started. */
        /* Tell the service manager that we are resuming */
        if (wrapperData->startupTimeout > 0) {
            timeout = wrapperData->startupTimeout * 1000;
        } else {
            timeout = 86400000; /* Set infinity at 1 day. */
        }
        wrapperReportStatus(FALSE, WRAPPER_WSTATE_RESUMING, 0, timeout);
    }
}

/**
 * WRAPPER_WSTATE_STOPPING
 * The Wrapper process has started its shutdown process.  It will
 *  remain in this state until it is confirmed that the JVM has been
 *  stopped.
 *
 * nowTicks: The tick counter value this time through the event loop.
 */
void wStateStopping(TICKS nowTicks) {
    int timeout;

    /* The wrapper is stopping, we need to ping the service manager */
    /*  to reasure it that we are still alive. */

    /* Tell the service manager that we are stopping */
    if ((wrapperData->shutdownTimeout <= 0) || (wrapperData->jvmExitTimeout <= 0)) {
        timeout = 86400000; /* Set infinity at 1 day. */
    } else {
        timeout = MAX(wrapperData->shutdownTimeout, wrapperData->jvmExitTimeout) * 1000;
    }
    wrapperReportStatus(FALSE, WRAPPER_WSTATE_STOPPING, wrapperData->exitCode, timeout);

    /* If the JVM state is now DOWN_CLEAN, then change the wrapper state */
    /*  to be STOPPED as well. */
    if (wrapperData->jState == WRAPPER_JSTATE_DOWN_CLEAN) {
        wrapperSetWrapperState(WRAPPER_WSTATE_STOPPED);

        /* Don't tell the service manager that we stopped here.  That */
        /*    will be done when the application actually quits. */
    }
}

/**
 * WRAPPER_WSTATE_STOPPED
 * The Wrapper process is now ready to exit.  The event loop will complete
 *  and the Wrapper process will exit.
 *
 * nowTicks: The tick counter value this time through the event loop.
 */
void wStateStopped(TICKS nowTicks) {
    /* The wrapper is ready to stop.  Nothing to be done here.  This */
    /*  state will exit the event loop below. */
}

/********************************************************************
 * JVM States
 *******************************************************************/

/**
 * WRAPPER_JSTATE_DOWN_CLEAN
 * The JVM process currently does not exist and we are clean.  Depending
 *  on the Wrapper state and other factors, we will either stay in this
 *  state or switch to the LAUNCH state causing a JVM to be launched
 *  after a delay set in this function.
 *
 * nowTicks: The tick counter value this time through the event loop.
 * nextSleep: Flag which is used to determine whether or not the state engine
 *            will be sleeping before then next time through the loop.  It
 *            may make sense to avoid certain actions if it is known that the
 *            function will be called again immediately.
 */
void jStateDownClean(TICKS nowTicks, int nextSleep) {
    TCHAR onExitParamBuffer[16 + 10 + 1];
    int startupDelay;
    int restartMode;

    /* The JVM can be down for one of 4 reasons.  The first is that the
     *  wrapper is just starting.  The second is that the JVM is being
     *  restarted for some reason, the 3rd is that the wrapper is paused,
     *  and the 4th is that the wrapper is trying to shut down. */
    if ((wrapperData->wState == WRAPPER_WSTATE_STARTING) ||
        (wrapperData->wState == WRAPPER_WSTATE_STARTED) ||
        (wrapperData->wState == WRAPPER_WSTATE_RESUMING)) {

        if (wrapperData->restartRequested) {
            /* A JVM needs to be launched. */
            restartMode = wrapperData->restartRequested;
            wrapperData->restartRequested = WRAPPER_RESTART_REQUESTED_NO;

            /* Depending on the number of restarts to date, decide how to handle the (re)start. */
            if (wrapperData->jvmRestarts > 0) {
                /* This is not the first JVM, so make sure that we still want to launch. */
                if ((wrapperData->wState == WRAPPER_WSTATE_RESUMING) && wrapperData->pausableStopJVM) {
                    /* We are resuming and the JVM was expected to be stopped.  Always launch
                     *  immediately and reset the failed invocation count.
                     * This mode of restarts works even if restarts have been disabled. */
                    wrapperData->failedInvocationCount = 0;
                    wrapperSetJavaState(WRAPPER_JSTATE_LAUNCH_DELAY, nowTicks, 0);

                } else if ((restartMode == WRAPPER_RESTART_REQUESTED_AUTOMATIC) && wrapperData->isAutoRestartDisabled) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Automatic JVM Restarts disabled.  Shutting down."));
                    wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);

                } else if (wrapperData->isRestartDisabled) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("JVM Restarts disabled.  Shutting down."));
                    wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);

                } else if (wrapperGetTickAgeSeconds(wrapperData->jvmLaunchTicks, nowTicks) >= wrapperData->successfulInvocationTime) {
                    /* The previous JVM invocation was running long enough that its invocation */
                    /*   should be considered a success.  Reset the failedInvocationStart to   */
                    /*   start the count fresh.                                                */
                    wrapperData->failedInvocationCount = 0;

                    /* Set the state to launch after the restart delay. */
                    wrapperSetJavaState(WRAPPER_JSTATE_LAUNCH_DELAY, nowTicks, wrapperData->restartDelay);

                    if (wrapperData->restartDelay > 0) {
                        if (wrapperData->isDebugging) {
                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                                TEXT("Waiting %d seconds before launching another JVM."), wrapperData->restartDelay);
                        }
                    }
                } else {
                    /* The last JVM invocation died quickly and was considered to have */
                    /*  been a faulty launch.  Increase the failed count.              */
                    wrapperData->failedInvocationCount++;

                    if (wrapperData->isDebugging) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                            TEXT("JVM was only running for %d seconds leading to a failed restart count of %d."),
                            wrapperGetTickAgeSeconds(wrapperData->jvmLaunchTicks, nowTicks), wrapperData->failedInvocationCount);
                    }

                    /* See if we are allowed to try restarting the JVM again. */
                    if (wrapperData->failedInvocationCount < wrapperData->maxFailedInvocations) {
                        /* Try reslaunching the JVM */

                        /* Set the state to launch after the restart delay. */
                        wrapperSetJavaState(WRAPPER_JSTATE_LAUNCH_DELAY, nowTicks, wrapperData->restartDelay);

                        if (wrapperData->restartDelay > 0) {
                            if (wrapperData->isDebugging) {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                                    TEXT("Waiting %d seconds before launching another JVM."), wrapperData->restartDelay);
                            }
                        }
                    } else {
                        /* Unable to launch another JVM. */
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                                   TEXT("There were %d failed launches in a row, each lasting less than %d seconds.  Giving up."),
                                   wrapperData->failedInvocationCount, wrapperData->successfulInvocationTime);
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                                   TEXT("  There may be a configuration problem: please check the logs."));
                        wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
                    }
                }
            } else {
                /* This will be the first invocation. */
                wrapperData->failedInvocationCount = 0;

                /* Set the state to launch after the startup delay. */
                if (wrapperData->isConsole) {
                    startupDelay = wrapperData->startupDelayConsole;
                } else {
                    startupDelay = wrapperData->startupDelayService;
                }
                wrapperSetJavaState(WRAPPER_JSTATE_LAUNCH_DELAY, nowTicks, startupDelay);

                if (startupDelay > 0) {
                    if (wrapperData->isDebugging) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                            TEXT("Waiting %d seconds before launching the first JVM."), startupDelay);
                    }
                }
            }
        } else {
            /* The JVM is down, but a restart has not yet been requested.
             *   See if the user has registered any events for the exit code. */
            _sntprintf(onExitParamBuffer, 16 + 10 + 1, TEXT("wrapper.on_exit.%d"), wrapperData->exitCode);
            if (checkPropertyEqual(properties, onExitParamBuffer, getStringProperty(properties, TEXT("wrapper.on_exit.default"), TEXT("shutdown")), TEXT("restart"))) {
                /* We want to restart the JVM. */
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                    TEXT("on_exit trigger matched.  Restarting the JVM.  (Exit code: %d)"), wrapperData->exitCode);

                wrapperData->restartRequested = WRAPPER_RESTART_REQUESTED_CONFIGURED;

                /* Fall through, the restart will take place on the next loop. */
            } else if (checkPropertyEqual(properties, onExitParamBuffer, getStringProperty(properties, TEXT("wrapper.on_exit.default"), TEXT("shutdown")), TEXT("pause"))) {
                /* We want to pause the JVM. */
                if (wrapperData->pausable) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("on_exit trigger matched.  Pausing the Wrapper.  (Exit code: %d)"), wrapperData->exitCode);
                    wrapperPauseProcess(WRAPPER_ACTION_SOURCE_CODE_ON_EXIT);
                } else {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("on_exit trigger matched.  Pausing not enabled.  Restarting the JVM.  (Exit code: %d)"), wrapperData->exitCode);
                    wrapperData->restartRequested = WRAPPER_RESTART_REQUESTED_CONFIGURED;
                }
            } else {
                /* We want to stop the Wrapper. */
                wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
            }
        }
    } else if (wrapperData->wState == WRAPPER_WSTATE_PAUSED) {
        /* The wrapper is paused. */

        if (wrapperData->pausableStopJVM) {
            /* The stop state is expected. */

            /* Make sure we are setup to restart when the Wrapper is resumed later. */
            wrapperData->restartRequested = WRAPPER_RESTART_REQUESTED_CONFIGURED;
        } else {
            /* The JVM should still be running, but it is not.  Try to figure out why. */
            if (wrapperData->restartRequested) {
                /* The JVM must have crashed.  The restart will be honored when the service
                 *  is resumed. Do nothing for now. */
            } else {
                /* No restart was requested.  So the JVM must have requested a stop.
                 *  Normally, this would result in the service stopping from the paused
                 *  state, but it is possible that an exit code is registered. Check them. */
                _sntprintf(onExitParamBuffer, 16 + 10 + 1, TEXT("wrapper.on_exit.%d"), wrapperData->exitCode);
                if (checkPropertyEqual(properties, onExitParamBuffer, getStringProperty(properties, TEXT("wrapper.on_exit.default"), TEXT("shutdown")), TEXT("restart"))) {
                    /* We want to restart the JVM.   But not now.  Let the user know. */
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("on_exit trigger matched.  Service is paused, will restart the JVM when resumed.  (Exit code: %d)"), wrapperData->exitCode);

                    /* Make sure we are setup to restart when the Wrapper is resumed later. */
                    wrapperData->restartRequested = WRAPPER_RESTART_REQUESTED_CONFIGURED;

                    /* Fall through, the restart will take place once the service is resumed. */
                } else if (checkPropertyEqual(properties, onExitParamBuffer, getStringProperty(properties, TEXT("wrapper.on_exit.default"), TEXT("shutdown")), TEXT("pause"))) {
                    /* We are paused as expected. */

                    /* Make sure we are setup to restart when the Wrapper is resumed later. */
                    wrapperData->restartRequested = WRAPPER_RESTART_REQUESTED_CONFIGURED;
                } else {
                    /* We want to stop the Wrapper. */
                    wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
                }
            }
        }
    } else {
        /* The wrapper is shutting down or pausing.  Do nothing. */
    }

    /* Reset the last ping time */
    wrapperData->lastPingTicks = nowTicks;
    wrapperData->lastLoggedPingTicks = nowTicks;
}

/**
 * WRAPPER_JSTATE_LAUNCH_DELAY
 * Waiting to launch a JVM.  When the state timeout has expired, a JVM
 *  will be launched.
 *
 * nowTicks: The tick counter value this time through the event loop.
 * nextSleep: Flag which is used to determine whether or not the state engine
 *            will be sleeping before then next time through the loop.  It
 *            may make sense to avoid certain actions if it is known that the
 *            function will be called again immediately.
 */
void jStateLaunchDelay(TICKS nowTicks, int nextSleep) {
    const TCHAR *mainClass;

    /* The Waiting state is set from the DOWN_CLEAN state if a JVM had
     *  previously been launched the Wrapper will wait in this state
     *  until the restart delay has expired.  If this was the first
     *  invocation, then the state timeout will be set to the current
     *  time causing the new JVM to be launced immediately. */
    if ((wrapperData->wState == WRAPPER_WSTATE_STARTING) ||
        (wrapperData->wState == WRAPPER_WSTATE_STARTED) ||
        (wrapperData->wState == WRAPPER_WSTATE_RESUMING)) {

        /* Is it time to proceed? */
        if (wrapperData->jStateTimeoutTicksSet && (wrapperGetTickAgeSeconds(wrapperData->jStateTimeoutTicks, nowTicks) >= 0)) {
            /* Launch the new JVM */

            if (wrapperData->jvmRestarts > 0) {
                /* See if the logs should be rolled on Wrapper startup. */
                if (getLogfileRollMode() & ROLL_MODE_JVM) {
                    rollLogs();
                }

                /* Unless this is the first JVM invocation, make it possible to reload the
                 *  Wrapper configuration file. */
                if (wrapperData->restartReloadConf) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("Reloading Wrapper configuration..."));

                    /* If the working dir has been changed then we need to restore it before
                     *  the configuration can be reloaded.  This is needed to support relative
                     *  references to include files. */
                    if (wrapperData->workingDir && wrapperData->originalWorkingDir) {
                        if (wrapperSetWorkingDir(wrapperData->originalWorkingDir)) {
                            /* Failed to restore the working dir.  Shutdown the Wrapper */
                            wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
                            wrapperData->exitCode = 1;
                            return;
                        }
                    }

                    if (wrapperLoadConfigurationProperties()) {
                        /* Failed to reload the configuration.  This is bad.
                         *  The JVM is already down.  Shutdown the Wrapper. */
                        wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
                        wrapperData->exitCode = 1;
                        return;
                    }

                    /* Change the working directory if configured to do so. */
                    if (wrapperData->workingDir && wrapperSetWorkingDir(wrapperData->workingDir)) {
                        /* Failed to set the working dir.  Shutdown the Wrapper */
                        wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
                        wrapperData->exitCode = 1;
                        return;
                    }
                }
            }

            /* Make sure user is not trying to use the old removed SilverEgg package names. */
            mainClass = getStringProperty(properties, TEXT("wrapper.java.mainclass"), TEXT("Main"));
            if (_tcsstr(mainClass, TEXT("com.silveregg.wrapper.WrapperSimpleApp")) != NULL) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                    TEXT("The com.silveregg.wrapper.WrapperSimpleApp class is no longer supported." ));
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                    TEXT("Please use the org.tanukisoftware.wrapper.WrapperSimpleApp class instead." ));
                wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
                wrapperData->exitCode = 1;
                return;
            } else if (_tcsstr(mainClass, TEXT("com.silveregg.wrapper.WrapperStartStopApp")) != NULL) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                    TEXT("The com.silveregg.wrapper.WrapperStartStopApp class is no longer supported." ));
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                    TEXT("Please use the org.tanukisoftware.wrapper.WrapperStartStopApp class instead." ));
                wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
                wrapperData->exitCode = 1;
                return;
            }

            /* Set the launch time to the curent time */
            wrapperData->jvmLaunchTicks = nowTicks;

            /* Generate a unique key to use when communicating with the JVM */
            wrapperBuildKey();

            /* Check the backend server to make sure it has been initialized.
             *  This is needed so we can pass its port as part of the java command. */
            if (!wrapperCheckServerBackend(TRUE)) {
                /* The backend is not up.  An error should have been reported.  But this means we
                 *  are unable to continue. */
                wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
                wrapperData->exitCode = 1;
                return;
            }

            /* Generate the command used to launch the Java process */
            if (wrapperBuildJavaCommand()) {
                /* Failed. Wrapper shutdown. */
                wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
                wrapperData->exitCode = 1;
                return;
            }

            /* Log a few comments that will explain the JVM behavior. */
            if (wrapperData->isDebugging) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                    TEXT("Ping settings: wrapper.ping.interval=%d, wrapper.ping.interval.logged=%d, wrapper.ping.timeout=%d"),
                    wrapperData->pingInterval, wrapperData->pingIntervalLogged, wrapperData->pingTimeout);
            }

            if (wrapperData->jvmRestarts > 0) {
                wrapperSetJavaState(WRAPPER_JSTATE_RESTART, nowTicks, -1);
            } else {
                /* Increment the JVM restart Id to keep track of how many JVMs we have launched. */
                wrapperData->jvmRestarts++;

                wrapperSetJavaState(WRAPPER_JSTATE_LAUNCH, nowTicks, -1);
            }
        }
    } else {
        /* The wrapper is shutting down, pausing or paused.  Switch to the
         *  down clean state because the JVM was never launched. */
        wrapperSetJavaState(WRAPPER_JSTATE_DOWN_CLEAN, nowTicks, -1);
    }
}

/**
 * WRAPPER_JSTATE_RESTART
 * The Wrapper is ready to restart a JVM.
 *
 * nowTicks: The tick counter value this time through the event loop.
 * nextSleep: Flag which is used to determine whether or not the state engine
 *            will be sleeping before then next time through the loop.  It
 *            may make sense to avoid certain actions if it is known that the
 *            function will be called again immediately.
 */
void jStateRestart(TICKS nowTicks, int nextSleep) {

    if ((wrapperData->wState == WRAPPER_WSTATE_STARTING) ||
        (wrapperData->wState == WRAPPER_WSTATE_STARTED) ||
        (wrapperData->wState == WRAPPER_WSTATE_RESUMING)) {
        /* Increment the JVM restart Id to keep track of how many JVMs we have launched. */
        wrapperData->jvmRestarts++;

        wrapperSetJavaState(WRAPPER_JSTATE_LAUNCH, nowTicks, -1);
    } else {
        /* The wrapper is shutting down, pausing or paused.  Switch to the
         *  down clean state because the JVM was never launched. */
        wrapperSetJavaState(WRAPPER_JSTATE_DOWN_CLEAN, nowTicks, -1);
    }
}

/**
 * WRAPPER_JSTATE_LAUNCH
 * The Wrapper is ready to launch a JVM.
 *
 * nowTicks: The tick counter value this time through the event loop.
 * nextSleep: Flag which is used to determine whether or not the state engine
 *            will be sleeping before then next time through the loop.  It
 *            may make sense to avoid certain actions if it is known that the
 *            function will be called again immediately.
 */
void jStateLaunch(TICKS nowTicks, int nextSleep) {

    if ((wrapperData->wState == WRAPPER_WSTATE_STARTING) ||
        (wrapperData->wState == WRAPPER_WSTATE_STARTED) ||
        (wrapperData->wState == WRAPPER_WSTATE_RESUMING)) {

        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Launching a JVM..."));

        wrapperExecute();

        /* The JVM was launched.  We still do not know whether the
         *  launch will be successful.  Allow <startupTimeout> seconds before giving up.
         *  This can take quite a while if the system is heavily loaded.
         *  (At startup for example) */
        if (wrapperData->startupTimeout > 0) {
            wrapperSetJavaState(WRAPPER_JSTATE_LAUNCHING, nowTicks, wrapperData->startupTimeout);
        } else {
            wrapperSetJavaState(WRAPPER_JSTATE_LAUNCHING, nowTicks, -1);
        }
    } else {
        /* The wrapper is shutting down, pausing or paused.  Switch to the down clean state because the JVM was never launched. */
        wrapperSetJavaState(WRAPPER_JSTATE_DOWN_CLEAN, nowTicks, -1);
    }
}

/**
 * WRAPPER_JSTATE_LAUNCHING
 * The JVM process has been launched, but there has been no confirmation that
 *  the JVM and its application have started.  We remain in this state until
 *  the state times out or the WrapperManager class in the JVM has sent a
 *  message that it is initialized.
 *
 * nowTicks: The tick counter value this time through the event loop.
 * nextSleep: Flag which is used to determine whether or not the state engine
 *            will be sleeping before then next time through the loop.  It
 *            may make sense to avoid certain actions if it is known that the
 *            function will be called again immediately.
 */
void jStateLaunching(TICKS nowTicks, int nextSleep) {
    /* Make sure that the JVM process is still up and running */
    if (nextSleep && (wrapperGetProcessStatus(nowTicks, FALSE) == WRAPPER_PROCESS_DOWN)) {
        /* The process is gone.  Restart it. (Handled and logged) */
    } else {
        /* The process is up and running.
         * We are waiting in this state until we receive a KEY packet
         *  from the JVM attempting to register.
         * Have we waited too long already */
        if (wrapperData->jStateTimeoutTicksSet && (wrapperGetTickAgeSeconds(wrapperData->jStateTimeoutTicks, nowTicks) >= 0)) {
            if (wrapperData->debugJVM) {
                handleDebugJVMTimeout(nowTicks,
                    TEXT("Startup: Timed out waiting for a signal from the JVM."), TEXT("startup"));
            } else {
                displayLaunchingTimeoutMessage();

                /* Give up on the JVM and start trying to kill it. */
                wrapperKillProcess();

                /* Restart the JVM. */
                wrapperData->restartRequested = WRAPPER_RESTART_REQUESTED_AUTOMATIC;
            }
        }
    }
}

/**
 * WRAPPER_JSTATE_LAUNCHED
 * The WrapperManager class in the JVM has been initialized.  We are now
 *  ready to request that the application in the JVM be started.
 *
 * nowTicks: The tick counter value this time through the event loop.
 * nextSleep: Flag which is used to determine whether or not the state engine
 *            will be sleeping before then next time through the loop.  It
 *            may make sense to avoid certain actions if it is known that the
 *            function will be called again immediately.
 */
void jStateLaunched(TICKS nowTicks, int nextSleep) {
    int ret;

    /* The Java side of the wrapper code has responded to a ping.
     *  Tell the Java wrapper to start the Java application. */
    if (wrapperData->isDebugging) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Start Application."));
    }
    ret = wrapperProtocolFunction(WRAPPER_MSG_START, TEXT("start"));
    if (ret) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to send the start command to the JVM."));

        /* Give up on the JVM and start trying to kill it. */
        wrapperKillProcess();

        /* Restart the JVM. */
        wrapperData->restartRequested = WRAPPER_RESTART_REQUESTED_AUTOMATIC;
    } else {
        /* Start command send.  Start waiting for the app to signal
         *  that it has started.  Allow <startupTimeout> seconds before
         *  giving up.  A good application will send starting signals back
         *  much sooner than this as a way to extend this time if necessary. */
        if (wrapperData->startupTimeout > 0) {
            wrapperSetJavaState(WRAPPER_JSTATE_STARTING, nowTicks, wrapperData->startupTimeout);
        } else {
            wrapperSetJavaState(WRAPPER_JSTATE_STARTING, nowTicks, -1);
        }
    }
}

/**
 * WRAPPER_JSTATE_STARTING
 * The JVM is up and the application has been asked to start.  We
 *  stay in this state until we receive confirmation that the
 *  application has been started or the state times out.
 *
 * nowTicks: The tick counter value this time through the event loop.
 * nextSleep: Flag which is used to determine whether or not the state engine
 *            will be sleeping before then next time through the loop.  It
 *            may make sense to avoid certain actions if it is known that the
 *            function will be called again immediately.
 */
void jStateStarting(TICKS nowTicks, int nextSleep) {
    /* Make sure that the JVM process is still up and running */
    if (nextSleep && (wrapperGetProcessStatus(nowTicks, FALSE) == WRAPPER_PROCESS_DOWN)) {
        /* The process is gone.  Restart it. (Handled and logged) */
    } else {
        /* Have we waited too long already */
        if (wrapperData->jStateTimeoutTicksSet && (wrapperGetTickAgeSeconds(wrapperData->jStateTimeoutTicks, nowTicks) >= 0)) {
            if (wrapperData->debugJVM) {
                handleDebugJVMTimeout(nowTicks,
                    TEXT("Startup: Timed out waiting for a signal from the JVM."), TEXT("startup"));
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                           TEXT("Startup failed: Timed out waiting for signal from JVM."));

                /* Give up on the JVM and start trying to kill it. */
                wrapperKillProcess();

                /* Restart the JVM. */
                wrapperData->restartRequested = WRAPPER_RESTART_REQUESTED_AUTOMATIC;
            }
        } else {
            /* Keep waiting. */
        }
    }
}

/**
 * WRAPPER_JSTATE_STARTED
 * The application in the JVM has confirmed that it is started.  We will
 *  stay in this state, sending pings to the JVM at regular intervals,
 *  until the JVM fails to respond to a ping, or the JVM is ready to be
 *  shutdown.
 * The pings are sent to make sure that the JVM does not die or hang.
 *
 * nowTicks: The tick counter value this time through the event loop.
 * nextSleep: Flag which is used to determine whether or not the state engine
 *            will be sleeping before then next time through the loop.  It
 *            may make sense to avoid certain actions if it is known that the
 *            function will be called again immediately.
 */
void jStateStarted(TICKS nowTicks, int nextSleep) {
    int ret;

    /* Make sure that the JVM process is still up and running */
    if (nextSleep && (wrapperGetProcessStatus(nowTicks, FALSE) == WRAPPER_PROCESS_DOWN)) {
        /* The process is gone.  Restart it. (Handled and logged) */
    } else {
        if (wrapperData->jStateTimeoutTicksSet && (wrapperGetTickAgeSeconds(wrapperData->jStateTimeoutTicks, nowTicks) >= 0)) {
            /* Have we waited too long already.  The jStateTimeoutTicks is reset each time a ping
             *  response is received from the JVM. */
            if (wrapperData->debugJVM) {
                handleDebugJVMTimeout(nowTicks,
                    TEXT("Ping: Timed out waiting for signal from JVM."), TEXT("ping"));
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                           TEXT("JVM appears hung: Timed out waiting for signal from JVM."));

                /* Give up on the JVM and start trying to kill it. */
                wrapperKillProcess();

                /* Restart the JVM. */
                wrapperData->restartRequested = WRAPPER_RESTART_REQUESTED_AUTOMATIC;
            }
        } else if (wrapperGetTickAgeSeconds(wrapperAddToTicks(wrapperData->lastPingTicks, wrapperData->pingInterval), nowTicks) >= 0) {
            /* It is time to send another ping to the JVM */
            if (wrapperGetTickAgeSeconds(wrapperAddToTicks(wrapperData->lastLoggedPingTicks, wrapperData->pingIntervalLogged), nowTicks) >= 0) {
                if (wrapperData->isLoopOutputEnabled) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Loop: Sending a ping packet."));
                }
                ret = wrapperProtocolFunction(WRAPPER_MSG_PING, TEXT("ping"));
                wrapperData->lastLoggedPingTicks = nowTicks;
            } else {
                if (wrapperData->isLoopOutputEnabled) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Loop: Sending a silent ping packet."));
                }
                ret = wrapperProtocolFunction(WRAPPER_MSG_PING, TEXT("silent"));
            }
            if (ret) {
                /* Failed to send the ping. */
                if (wrapperData->isDebugging) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("JVM Ping Failed."));
                }
            }
            if (wrapperData->isLoopOutputEnabled) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Loop: Sent a ping packet."));
            }
            wrapperData->lastPingTicks = nowTicks;
        } else {
            /* Do nothing.  Keep waiting. */
        }
    }
}

/**
 * WRAPPER_JSTATE_STOP
 * The application in the JVM should be asked to stop but is still running.
 *
 * nowTicks: The tick counter value this time through the event loop.
 * nextSleep: Flag which is used to determine whether or not the state engine
 *            will be sleeping before then next time through the loop.  It
 *            may make sense to avoid certain actions if it is known that the
 *            function will be called again immediately.
 */
void jStateStop(TICKS nowTicks, int nextSleep) {

    /* Make sure that the JVM process is still up and running */
    if (nextSleep && (wrapperGetProcessStatus(nowTicks, FALSE) == WRAPPER_PROCESS_DOWN)) {
        /* The process is gone. (Handled and logged)*/
    } else {
        /* Ask the JVM to shutdown. */
        wrapperProtocolFunction(WRAPPER_MSG_STOP, NULL);

        /* Allow up to 5 + <shutdownTimeout> seconds for the application to stop itself. */
        /* Already in this state. */
        if (wrapperData->shutdownTimeout > 0) {
            wrapperSetJavaState(WRAPPER_JSTATE_STOPPING, nowTicks, 5 + wrapperData->shutdownTimeout);
        } else {
            wrapperSetJavaState(WRAPPER_JSTATE_STOPPING, nowTicks, -1);
        }
    }
}

/**
 * WRAPPER_JSTATE_STOPPING
 * The application in the JVM has been asked to stop but we are still
 *  waiting for a signal that it is stopped.
 *
 * nowTicks: The tick counter value this time through the event loop.
 * nextSleep: Flag which is used to determine whether or not the state engine
 *            will be sleeping before then next time through the loop.  It
 *            may make sense to avoid certain actions if it is known that the
 *            function will be called again immediately.
 */
void jStateStopping(TICKS nowTicks, int nextSleep) {
    /* Make sure that the JVM process is still up and running */
    if (nextSleep && (wrapperGetProcessStatus(nowTicks, FALSE) == WRAPPER_PROCESS_DOWN)) {
        /* The process is gone. (Handled and logged)*/
    } else {
        /* Have we waited too long already */
        if (wrapperData->jStateTimeoutTicksSet && (wrapperGetTickAgeSeconds(wrapperData->jStateTimeoutTicks, nowTicks) >= 0)) {
            if (wrapperData->debugJVM) {
                handleDebugJVMTimeout(nowTicks,
                    TEXT("Shutdown: Timed out waiting for a signal from the JVM."), TEXT("shutdown"));
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                           TEXT("Shutdown failed: Timed out waiting for signal from JVM."));

                /* Give up on the JVM and start trying to kill it. */
                wrapperKillProcess();
            }
        } else {
            /* Keep waiting. */
        }
    }
}

/**
 * WRAPPER_JSTATE_STOPPED
 * The application in the JVM has signalled that it has stopped.  We are now
 *  waiting for the JVM process to exit.  A good application will do this on
 *  its own, but if it fails to exit in a timely manner then the JVM will be
 *  killed.
 * Once the JVM process is gone we go back to the DOWN state.
 *
 * nowTicks: The tick counter value this time through the event loop.
 * nextSleep: Flag which is used to determine whether or not the state engine
 *            will be sleeping before then next time through the loop.  It
 *            may make sense to avoid certain actions if it is known that the
 *            function will be called again immediately.
 */
void jStateStopped(TICKS nowTicks, int nextSleep) {
    if (nextSleep && (wrapperGetProcessStatus(nowTicks, FALSE) == WRAPPER_PROCESS_DOWN)) {
        /* The process is gone. This is what we were waiting for. */
    } else {
        /* Have we waited too long already */
        if (wrapperData->jStateTimeoutTicksSet && (wrapperGetTickAgeSeconds(wrapperData->jStateTimeoutTicks, nowTicks) >= 0)) {
            if (wrapperData->debugJVM) {
                handleDebugJVMTimeout(nowTicks,
                    TEXT("Shutdown: Timed out waiting for the JVM to terminate."), TEXT("JVM exit"));
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                           TEXT("Shutdown failed: Timed out waiting for the JVM to terminate."));

                /* Give up on the JVM and start trying to kill it. */
                wrapperKillProcess();
            }
        } else {
            /* Keep waiting. */
        }
    }
}

/**
 * WRAPPER_JSTATE_KILLING
 * The Wrapper is about to kill the JVM.  If thread dumps on exit is enabled
 *  then the Wrapper must wait a few moments between telling the JVM to do
 *  a thread dump and actually killing it.  The Wrapper will sit in this state
 *  while it is waiting.
 *
 * nowTicks: The tick counter value this time through the event loop.
 * nextSleep: Flag which is used to determine whether or not the state engine
 *            will be sleeping before then next time through the loop.  It
 *            may make sense to avoid certain actions if it is known that the
 *            function will be called again immediately.
 */
void jStateKilling(TICKS nowTicks, int nextSleep) {
    /* Make sure that the JVM process is still up and running */
    if (nextSleep && (wrapperGetProcessStatus(nowTicks, FALSE) == WRAPPER_PROCESS_DOWN)) {
        /* The process is gone. (Handled and logged) */
    } else {
        /* Have we waited long enough */
        if (wrapperData->jStateTimeoutTicksSet && (wrapperGetTickAgeSeconds(wrapperData->jStateTimeoutTicks, nowTicks) >= 0)) {
            /* It is time to actually kill the JVM. */
            wrapperSetJavaState(WRAPPER_JSTATE_KILL, nowTicks, 0);
        } else {
            /* Keep waiting. */
        }
    }
}

/**
 * WRAPPER_JSTATE_KILL
 * The Wrapper is ready to kill the JVM.
 *
 * nowTicks: The tick counter value this time through the event loop.
 * nextSleep: Flag which is used to determine whether or not the state engine
 *            will be sleeping before then next time through the loop.  It
 *            may make sense to avoid certain actions if it is known that the
 *            function will be called again immediately.
 */
void jStateKill(TICKS nowTicks, int nextSleep) {

    if (nextSleep && (wrapperGetProcessStatus(nowTicks, FALSE) == WRAPPER_PROCESS_DOWN)) {
        /* The process is gone. (Handled and logged) */
    } else {
        /* Have we waited long enough */
        if (wrapperData->jStateTimeoutTicksSet && (wrapperGetTickAgeSeconds(wrapperData->jStateTimeoutTicks, nowTicks) >= 0)) {
            /* It is time to actually kill the JVM. */
            wrapperKillProcessNow();
        } else {
            /* Keep waiting. */
        }
    }
}

/**
 * WRAPPER_JSTATE_DOWN_CHECK
 * The JVM process currently does not exist but we still need to clean up.
 *  Once we have cleaned up, we will switch to the DOWN_CLEAN state.
 *
 * nowTicks: The tick counter value this time through the event loop.
 * nextSleep: Flag which is used to determine whether or not the state engine
 *            will be sleeping before then next time through the loop.  It
 *            may make sense to avoid certain actions if it is known that the
 *            function will be called again immediately.
 */
void jStateDownCheck(TICKS nowTicks, int nextSleep) {
    wrapperSetJavaState(WRAPPER_JSTATE_DOWN_CLEAN, nowTicks, -1);
}

/********************************************************************
 * Event Loop / State Engine
 *******************************************************************/

void logTickTimerStats() {
    struct tm when;
    time_t now, overflowTime;

    TICKS sysTicks;
    TICKS ticks;

    time(&now);

    sysTicks = wrapperGetSystemTicks();

    overflowTime = (time_t)(now - (sysTicks / (1000 / WRAPPER_TICK_MS)));
    when = *localtime(&overflowTime);
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
        TEXT("    Last system time tick overflow at: %04d/%02d/%02d %02d:%02d:%02d"),
        when.tm_year + 1900, when.tm_mon + 1, when.tm_mday,
        when.tm_hour, when.tm_min, when.tm_sec);

    overflowTime = (time_t)(now + ((0xffffffffUL - sysTicks) / (1000 / WRAPPER_TICK_MS)));
    when = *localtime(&overflowTime);
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
        TEXT("    Next system time tick overflow at: %04d/%02d/%02d %02d:%02d:%02d"),
        when.tm_year + 1900, when.tm_mon + 1, when.tm_mday,
        when.tm_hour, when.tm_min, when.tm_sec);

    if (!wrapperData->useSystemTime) {
        ticks = wrapperGetTicks();

        overflowTime = (time_t)(now - (ticks / (1000 / WRAPPER_TICK_MS)));
        when = *localtime(&overflowTime);
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
            TEXT("    Last tick overflow at: %04d/%02d/%02d %02d:%02d:%02d"),
            when.tm_year + 1900, when.tm_mon + 1, when.tm_mday,
            when.tm_hour, when.tm_min, when.tm_sec);

        overflowTime = (time_t)(now + ((0xffffffffUL - ticks) / (1000 / WRAPPER_TICK_MS)));
        when = *localtime(&overflowTime);
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
            TEXT("    Next tick overflow at: %04d/%02d/%02d %02d:%02d:%02d"),
            when.tm_year + 1900, when.tm_mon + 1, when.tm_mday,
            when.tm_hour, when.tm_min, when.tm_sec);
    }
}

/**
 * The main event loop for the wrapper.  Handles all state changes and events.
 */
DWORD lastLogfileActivity = 0;
void wrapperEventLoop() {
    TICKS nowTicks;
    int uptimeSeconds;
    TICKS lastCycleTicks = wrapperGetTicks();
    int nextSleep;
    DWORD activity;
    /* Initialize the tick timeouts. */
    wrapperData->anchorTimeoutTicks = lastCycleTicks;
    wrapperData->commandTimeoutTicks = lastCycleTicks;
    wrapperData->memoryOutputTimeoutTicks = lastCycleTicks;
    wrapperData->cpuOutputTimeoutTicks = lastCycleTicks;
    wrapperData->pageFaultOutputTimeoutTicks = lastCycleTicks;
    wrapperData->logfileInactivityTimeoutTicks = lastCycleTicks;

    if (wrapperData->isDebugging) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Use tick timer mutex=%s"), wrapperData->useTickMutex ? TEXT("TRUE") : TEXT("FALSE"));
    }
    
    if (wrapperData->isTickOutputEnabled) {
        logTickTimerStats();
    }

    if (wrapperData->isLoopOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Event loop started."));
    }

    if (wrapperData->isMemoryOutputEnabled) {
        wrapperDumpMemoryBanner();
    }

    
    nextSleep = TRUE;
    do {
        if (wrapperData->isLoopOutputEnabled) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Loop: %ssleep"), (nextSleep ? TEXT("") : TEXT("no ")));
        }
        if (nextSleep) {
            /* Sleep for a tenth of a second. */
            wrapperSleep(100);
        }
        nextSleep = TRUE;
        
        /* Before doing anything else, always maintain the logger to make sure
         *  that any queued messages are logged before doing anything else.
         *  Called a second time after socket and child output to make sure
         *  that all messages appropriate for the state changes have been
         *  logged.  Failure to do so can result in a confusing sequence of
         *  output. */
        if (wrapperData->isLoopOutputEnabled) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Loop: maintain logger"));
        }
        maintainLogger();
        
        if (wrapperData->pauseThreadMain) {
            wrapperPauseThread(wrapperData->pauseThreadMain, TEXT("main"));
            wrapperData->pauseThreadMain = 0;
        }
        
        /* After we maintain the logger, see if there were any signals trapped. */
#ifdef WIN32
        wrapperMaintainControlCodes();
#else
        wrapperMaintainSignals();
#endif

#ifdef WIN32
        wrapperCheckConsoleWindows();
#endif

        if (!wrapperData->useJavaIOThread) {
            /* Check the stout pipe of the child process. */
            if (wrapperData->isLoopOutputEnabled) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Loop: process JVM output"));
            }
            if (wrapperReadChildOutput()) {
                if (wrapperData->isDebugging) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                        TEXT("Pause reading child process output to share cycles."));
                }
                nextSleep = FALSE;
            }
        }

        /* Check for incoming data packets. */
        if (wrapperData->isLoopOutputEnabled) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Loop: process socket"));
        }
        /* Don't bother processing the socket if we are shutting down and the JVM is down. */
        if (((wrapperData->jState == WRAPPER_JSTATE_DOWN_CHECK) || (wrapperData->jState == WRAPPER_JSTATE_DOWN_CLEAN)) &&
            ((wrapperData->wState == WRAPPER_WSTATE_STOPPING) || (wrapperData->wState == WRAPPER_WSTATE_STOPPED))) {
            /* Skip socket processing. */
        } else {
            if (wrapperProtocolRead()) {
                /* There was more data waiting to be read, but we broke out. */
                if (wrapperData->isDebugging) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                        TEXT("Pause reading socket data to share cycles."));
                }
                nextSleep = FALSE;
            }
        }

        /* See comment for first call above. */
        if (wrapperData->isLoopOutputEnabled) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Loop: maintain logger(2)"));
        }
        maintainLogger();

        /* Get the current time for use in this cycle. */
        nowTicks = wrapperGetTicks();

        /* Tell the logging code what to use for the uptime. */
        if (!wrapperData->uptimeFlipped) {
            uptimeSeconds = wrapperGetTickAgeSeconds(WRAPPER_TICK_INITIAL, nowTicks);
            if (uptimeSeconds > WRAPPER_MAX_UPTIME_SECONDS) {
                wrapperData->uptimeFlipped = TRUE;
                setUptime(0, TRUE);
            } else {
                setUptime(uptimeSeconds, FALSE);
            }
        }

        /* Log memory usage. */
        if (wrapperData->isMemoryOutputEnabled) {
            if (wrapperTickExpired(nowTicks, wrapperData->memoryOutputTimeoutTicks)) {
                wrapperDumpMemory();
                wrapperData->memoryOutputTimeoutTicks = wrapperAddToTicks(nowTicks, wrapperData->memoryOutputInterval);
            }
        }

        /* Log CPU usage. */
        if (wrapperData->isCPUOutputEnabled) {
            if (wrapperTickExpired(nowTicks, wrapperData->cpuOutputTimeoutTicks)) {
                wrapperDumpCPUUsage();
                wrapperData->cpuOutputTimeoutTicks = wrapperAddToTicks(nowTicks, wrapperData->cpuOutputInterval);
            }
        }

#ifdef WIN32
        /* Log PageFault info. */
        if (wrapperData->isPageFaultOutputEnabled) {
            if (wrapperTickExpired(nowTicks, wrapperData->pageFaultOutputTimeoutTicks)) {
                wrapperDumpPageFaultUsage();
                wrapperData->pageFaultOutputTimeoutTicks = wrapperAddToTicks(nowTicks, wrapperData->pageFaultOutputInterval);
            }
        }
#endif

        /* Test the activity of the logfile. */
        activity = getLogfileActivity();
        if (activity != lastLogfileActivity) {
            /* There has been recent output.  update the timeout. */
            wrapperData->logfileInactivityTimeoutTicks = wrapperAddToTicks(nowTicks, wrapperData->logfileInactivityTimeout);
        }
        if (wrapperTickExpired(nowTicks, wrapperData->logfileInactivityTimeoutTicks)) {
            closeLogfile();
        } else {
            flushLogfile();
        }

        /* Has the process been getting CPU? This check will only detect a lag
         * if the useSystemTime flag is set. */
        if (wrapperGetTickAgeSeconds(lastCycleTicks, nowTicks) > wrapperData->cpuTimeout) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO,
                TEXT("Wrapper Process has not received any CPU time for %d seconds.  Extending timeouts."),
                wrapperGetTickAgeSeconds(lastCycleTicks, nowTicks));

            if (wrapperData->jStateTimeoutTicksSet) {
                wrapperData->jStateTimeoutTicks =
                    wrapperAddToTicks(wrapperData->jStateTimeoutTicks, wrapperGetTickAgeSeconds(lastCycleTicks, nowTicks));
            }
        }
        lastCycleTicks = nowTicks;

        /* Useful for development debugging, but not runtime debugging */
        if (wrapperData->isStateOutputEnabled) {
            if (wrapperData->jStateTimeoutTicksSet) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                           TEXT("    Ticks=%08x, WrapperState=%s, JVMState=%s JVMStateTimeoutTicks=%08x (%ds), Exit=%s, RestartMode=%d"),
                           nowTicks,
                           wrapperGetWState(wrapperData->wState),
                           wrapperGetJState(wrapperData->jState),
                           wrapperData->jStateTimeoutTicks,
                           wrapperGetTickAgeSeconds(nowTicks, wrapperData->jStateTimeoutTicks),
                           (wrapperData->exitRequested ? TEXT("true") : TEXT("false")),
                           wrapperData->restartRequested);
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                           TEXT("    Ticks=%08x, WrapperState=%s, JVMState=%s JVMStateTimeoutTicks=%08x (N/A), Exit=%s, RestartMode=%d"),
                           nowTicks,
                           wrapperGetWState(wrapperData->wState),
                           wrapperGetJState(wrapperData->jState),
                           wrapperData->jStateTimeoutTicks,
                           (wrapperData->exitRequested ? TEXT("true") : TEXT("false")),
                           wrapperData->restartRequested);
            }
        }

        /* If we are configured to do so, confirm that the anchor file still exists. */
        anchorPoll(nowTicks);

        /* If we are configured to do so, look for a command file and perform any
         *  requested operations. */
        commandPoll(nowTicks);

        if (wrapperData->exitRequested) {
            /* A new request for the JVM to be stopped has been made. */

            if (wrapperData->isLoopOutputEnabled) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Loop: exit requested"));
            }
            /* Acknowledge that we have seen the exit request so we don't get here again. */
            wrapperData->exitRequested = FALSE;

            if (wrapperData->jState == WRAPPER_JSTATE_DOWN_CLEAN) {
                /* A JVM is not currently running. Nothing to do.*/
            } else if ((wrapperData->jState == WRAPPER_JSTATE_LAUNCH_DELAY) ||
                (wrapperData->jState == WRAPPER_JSTATE_RESTART) ||
                (wrapperData->jState == WRAPPER_JSTATE_LAUNCH)) {
                /* A JVM is not yet running go back to the DOWN_CLEAN state. */
                wrapperSetJavaState(WRAPPER_JSTATE_DOWN_CLEAN, nowTicks, -1);
            } else if ((wrapperData->jState == WRAPPER_JSTATE_STOP) ||
                (wrapperData->jState == WRAPPER_JSTATE_STOPPING) ||
                (wrapperData->jState == WRAPPER_JSTATE_STOPPED) ||
                (wrapperData->jState == WRAPPER_JSTATE_KILLING) ||
                (wrapperData->jState == WRAPPER_JSTATE_KILL) ||
                (wrapperData->jState == WRAPPER_JSTATE_DOWN_CHECK)) {
                /* The JVM is already being stopped, so nothing else needs to be done. */
            } else {
                /* The JVM should be running or is in the process of launching, so it needs to be stopped. */
                if (wrapperGetProcessStatus(nowTicks, FALSE) == WRAPPER_PROCESS_DOWN) {
                    /* The process is gone.  (Handled and logged) */

                    /* We never want to restart here. */
                    wrapperData->restartRequested = WRAPPER_RESTART_REQUESTED_NO;
                } else {
                    /* JVM is still up.  Try asking it to shutdown nicely. */
                    if (wrapperData->isDebugging) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                            TEXT("Sending stop signal to JVM"));
                    }

                    wrapperSetJavaState(WRAPPER_JSTATE_STOP, nowTicks, -1);
                }
            }
        }

        /* Do something depending on the wrapper state */
        if (wrapperData->isLoopOutputEnabled) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Loop: handle wrapper state: %s"),
                wrapperGetWState(wrapperData->wState));
        }
        switch(wrapperData->wState) {
        case WRAPPER_WSTATE_STARTING:
            wStateStarting(nowTicks);
            break;

        case WRAPPER_WSTATE_STARTED:
            wStateStarted(nowTicks);
            break;

        case WRAPPER_WSTATE_PAUSING:
            wStatePausing(nowTicks);
            break;

        case WRAPPER_WSTATE_PAUSED:
            wStatePaused(nowTicks);
            break;

        case WRAPPER_WSTATE_RESUMING:
            wStateResuming(nowTicks);
            break;

        case WRAPPER_WSTATE_STOPPING:
            wStateStopping(nowTicks);
            break;

        case WRAPPER_WSTATE_STOPPED:
            wStateStopped(nowTicks);
            break;

        default:
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unknown wState=%d"), wrapperData->wState);
            break;
        }

        /* Do something depending on the JVM state */
        if (wrapperData->isLoopOutputEnabled) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Loop: handle JVM state: %s"),
                wrapperGetJState(wrapperData->jState));
        }
        switch(wrapperData->jState) {
        case WRAPPER_JSTATE_DOWN_CLEAN:
            jStateDownClean(nowTicks, nextSleep);
            break;

        case WRAPPER_JSTATE_LAUNCH_DELAY:
            jStateLaunchDelay(nowTicks, nextSleep);
            break;

        case WRAPPER_JSTATE_RESTART:
            jStateRestart(nowTicks, nextSleep);
            break;

        case WRAPPER_JSTATE_LAUNCH:
            jStateLaunch(nowTicks, nextSleep);
            break;

        case WRAPPER_JSTATE_LAUNCHING:
            jStateLaunching(nowTicks, nextSleep);
            break;

        case WRAPPER_JSTATE_LAUNCHED:
            jStateLaunched(nowTicks, nextSleep);
            break;

        case WRAPPER_JSTATE_STARTING:
            jStateStarting(nowTicks, nextSleep);
            break;

        case WRAPPER_JSTATE_STARTED:
            jStateStarted(nowTicks, nextSleep);
            break;

        case WRAPPER_JSTATE_STOP:
            jStateStop(nowTicks, nextSleep);
            break;

        case WRAPPER_JSTATE_STOPPING:
            jStateStopping(nowTicks, nextSleep);
            break;

        case WRAPPER_JSTATE_STOPPED:
            jStateStopped(nowTicks, nextSleep);
            break;

        case WRAPPER_JSTATE_KILLING:
            jStateKilling(nowTicks, nextSleep);
            break;

        case WRAPPER_JSTATE_KILL:
            jStateKill(nowTicks, nextSleep);
            break;

        case WRAPPER_JSTATE_DOWN_CHECK:
            jStateDownCheck(nowTicks, nextSleep);
            break;

        default:
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unknown jState=%d"), wrapperData->jState);
            break;
        }
    } while (wrapperData->wState != WRAPPER_WSTATE_STOPPED);

    if (wrapperData->isLoopOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Event loop stopped."));
    }
}
