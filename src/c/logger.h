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
 *   Johan Sorlin   <Johan.Sorlin@Paregos.se>
 *   Leif Mortenson <leif@tanukisoftware.com>
 */

#ifndef _LOGGER_H
#define _LOGGER_H

#ifdef MACOSX
 #ifndef wcscasecmp
extern inline int wcscasecmp(const wchar_t* s1, const wchar_t* s2);
  #define MACOSX_ECSCASECMP
 #endif
#endif

#ifdef _DEBUG
 #define _DEBUG_QUEUE
#endif

#ifdef WIN32
 #include <windows.h>
 #define LOG_USER    (1<<3)
#endif
#include "wrapper_i18n.h"
#ifndef DWORD
 #define DWORD unsigned long
#endif

/* * * Log source constants * * */

#define WRAPPER_SOURCE_WRAPPER -1
#define WRAPPER_SOURCE_PROTOCOL -2

/* * * Log thread constants * * */
/* These are indexes in an array so they must be sequential, start
 *  with zero and be one less than the final WRAPPER_THREAD_COUNT */
#define WRAPPER_THREAD_CURRENT  -1
#define WRAPPER_THREAD_SIGNAL   0
#define WRAPPER_THREAD_MAIN     1
#define WRAPPER_THREAD_SRVMAIN  2
#define WRAPPER_THREAD_TIMER    3
#define WRAPPER_THREAD_JAVAIO   4
#define WRAPPER_THREAD_COUNT    5

#define MAX_LOG_SIZE 4096

#ifdef WIN32
#else
/* A special prefix on log messages that can be bassed through from a forked process
   so the parent will handle the log message correctly. */
#define LOG_FORK_MARKER TEXT("#!#WrApPeR#!#")
#define LOG_SPECIAL_MARKER TEXT("#!#WrApPeRsPeCiAl#!#")
#endif

/* * * Log level constants * * */

/* No logging at all. */
#define LEVEL_NONE   9

/* Notice messages which should always be displayed.  These never go to the syslog. */
#define LEVEL_NOTICE 8

/* Advisor messages which should always be displayed.  These never go to the syslog. */
#define LEVEL_ADVICE 7

/* Too many restarts, unable to start etc. Case when the Wrapper is forced to exit. */
#define LEVEL_FATAL  6

/* JVM died, hung messages */
#define LEVEL_ERROR  5

/* Warning messages. */
#define LEVEL_WARN   4

/* Started, Stopped, Restarted messages. */
#define LEVEL_STATUS 3

/* Copyright message. and logged console output. */
#define LEVEL_INFO   2

/* Current debug messages */
#define LEVEL_DEBUG  1

/* Unknown level */
#define LEVEL_UNKNOWN  0

/* * * Log file roll mode constants * * */
#define ROLL_MODE_UNKNOWN         0
#define ROLL_MODE_NONE            1
#define ROLL_MODE_SIZE            2
#define ROLL_MODE_WRAPPER         4
#define ROLL_MODE_JVM             8
#define ROLL_MODE_SIZE_OR_WRAPPER ROLL_MODE_SIZE + ROLL_MODE_WRAPPER
#define ROLL_MODE_SIZE_OR_JVM     ROLL_MODE_SIZE + ROLL_MODE_JVM
#define ROLL_MODE_DATE            16

#define ROLL_MODE_DATE_TOKEN      TEXT("YYYYMMDD")


#ifdef WIN32
extern void setConsoleStdoutHandle(HANDLE stdoutHandle);
#endif

/* * * Function predeclaration * * */
#define strcmpIgnoreCase(str1, str2) _tcsicmp(str1, str2)


/* This can be called from within logging code that would otherwise get stuck in recursion.
 *  Log to the console exactly when it happens and then also try to get it into the log
 *  file at the next oportunity. */
extern void outOfMemoryQueued(const TCHAR *context, int id);

extern void outOfMemory(const TCHAR *context, int id);

/**
 * Sets the number of milliseconds to allow logging to take before a warning is logged.
 *  Defaults to 0 for no limit.  Possible values 0 to 3600000.
 *
 * @param threshold Warning threashold.
 */
extern void setLogWarningThreshold(int threshold);

/**
 * Sets the log levels to a silence so we never output anything.
 */
extern void setSilentLogLevels();

/**
 * Sets the console log levels to a simple format for help and usage messages.
 */
extern void setSimpleLogLevels();

/* * Logfile functions * */
extern int isLogfileAccessed();

/**
 * Sets the log file to be used.  If the specified file is not absolute then
 *  it will be resolved into an absolute path.  If there are any problems with
 *  the path, like a directory not existing then the call will fail and the
 *  cause will be written to the existing log.
 *
 * @param log_file_path Log file to start using.
 * @param workingDir The current working directory, used for relative paths.
 *                   This will be NULL if this is part of the bootstrap process,
 *                   in which case we should not attempt to resolve the absolute
 *                   path.
 * @param preload TRUE if called as part of the preload process.  We use this to
 *                suppress double warnings.
 *
 * @return TRUE if there were any problems.
 */
extern int setLogfilePath( const TCHAR *log_file_path, const TCHAR *workingDir, int preload);

extern const TCHAR *getLogfilePath();

/**
 * Returns a snapshot of the current log file path.  This call safely gets the current path
 *  and returns a copy.  It is the responsibility of the caller to free up the memory on
 *  return.  Could return null if there was an error.
 */
extern TCHAR *getCurrentLogfilePath();

/**
 * Check the directory of the current logfile path to make sure it is writable.
 *  If there are any problems, log a warning.
 *
 * @return TRUE if there were any problems.
 */
extern int checkLogfileDir();

extern int getLogfileRollModeForName( const TCHAR *logfileRollName );
extern void setLogfileRollMode(int log_file_roll_mode);
extern int getLogfileRollMode();
extern void setLogfileUmask(int log_file_umask);
extern void setLogfileFormat( const TCHAR *log_file_format );
extern void setLogfileLevelInt(int log_file_level);
extern int getLogfileLevelInt();
extern void setLogfileLevel( const TCHAR *log_file_level );
extern void setLogfileMaxFileSize( const TCHAR *max_file_size );
extern void setLogfileMaxFileSizeInt(int max_file_size);
extern void setLogfileMaxLogFiles(int max_log_files);
extern void setLogfilePurgePattern(const TCHAR *pattern);
extern void setLogfilePurgeSortMode(int sortMode);
extern DWORD getLogfileActivity();
extern void closeLogfile();
extern void setLogfileAutoClose(int autoClose);
extern void flushLogfile();

/* * Console functions * */
extern void setConsoleLogFormat( const TCHAR *console_log_format );
extern void setConsoleLogLevelInt(int console_log_level);
extern int getConsoleLogLevelInt();
extern void setConsoleLogLevel( const TCHAR *console_log_level );
extern void setConsoleFlush(int flush);
extern void setConsoleFatalToStdErr(int toStdErr);
extern void setConsoleErrorToStdErr(int toStdErr);
extern void setConsoleWarnToStdErr(int toStdErr);

/* * Syslog/eventlog functions * */
extern void setSyslogLevelInt(int loginfo_level);
extern int getSyslogLevelInt();
extern void setSyslogLevel( const TCHAR *loginfo_level );
#ifndef WIN32
extern void setSyslogFacility( const TCHAR *loginfo_level );
#endif
extern void setSyslogEventSourceName( const TCHAR *event_source_name );
extern int registerSyslogMessageFile();
extern int unregisterSyslogMessageFile();


extern int getLowLogLevel();

/* * General log functions * */
extern int initLogging();
extern int disposeLogging();
extern void setUptime(int uptime, int flipped);
extern void rollLogs();
extern int getLogLevelForName( const TCHAR *logLevelName );
#ifndef WIN32
extern int getLogFacilityForName( const TCHAR *logFacilityName );
#endif
extern void logRegisterThread(int thread_id);

/**
 * The log_printf function logs a message to the configured log targets.
 *
 * This method can be used safely in most cases.  See the log_printf_queue
 *  funtion for the exceptions.
 */
extern void log_printf( int source_id, int level, const TCHAR *lpszFmt, ... );

/**
 * The log_printf_queue function is less efficient than the log_printf
 *  function and will cause logged messages to be logged out of order from
 *  those logged with log_printf because the messages are queued and then
 *  logged from another thread.
 *
 * Use of this function is required in cases where the thread may possibly
 *  be a signal callback.  In these cases, it is possible for the original
 *  thread to have been suspended within a log_printf call.  If the signal
 *  thread then attempted to call log_printf, it would result in a deadlock.
 */
extern void log_printf_queue( int useQueue, int source_id, int level, const TCHAR *lpszFmt, ... );

extern TCHAR* getLastErrorText();
extern int getLastError();
extern void maintainLogger();
extern void invalidMultiByteSequence(const TCHAR *context, int id);
#endif
