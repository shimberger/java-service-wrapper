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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include <sys/stat.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include "wrapper_file.h"

#ifdef WIN32
#include <io.h>
#include <Fcntl.h>
#include <windows.h>
#include <tchar.h>
#include <conio.h>
#include <sys/timeb.h>
#include "messages.h"

/* MS Visual Studio 8 went and deprecated the POXIX names for functions.
 *  Fixing them all would be a big headache for UNIX versions. */
#pragma warning(disable : 4996)

/* Defines for MS Visual Studio 6 */
#ifndef _INTPTR_T_DEFINED
typedef long intptr_t;
#define _INTPTR_T_DEFINED
#endif

#else
#include <syslog.h>
#include <strings.h>
#include <pthread.h>
#include <sys/time.h>
#include <limits.h>


 #if defined(SOLARIS)
  #include <sys/errno.h>
  #include <sys/fcntl.h>
 #elif defined(AIX) || defined(HPUX) || defined(MACOSX) || defined(OSF1)
 #elif defined(IRIX)
  #define PATH_MAX FILENAME_MAX
 #elif defined(FREEBSD)
  #include <sys/param.h>
  #include <errno.h>
 #else /* LINUX */
  #include <asm/errno.h>
 #endif

#endif

#include "wrapper_i18n.h"
#include "logger.h"

#ifndef TRUE
#define TRUE -1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/* Global data for logger */

/* Maximum number of milliseconds that a log write can take before we show a warning. */
int logPrintfWarnThreshold = 0;

/* Number of millisecoonds which the previous log message took to process. */
time_t previousLogLag;


/* Initialize all log levels to unknown until they are set */
int currentConsoleLevel = LEVEL_UNKNOWN;
int currentLogfileLevel = LEVEL_UNKNOWN;
int currentLoginfoLevel = LEVEL_UNKNOWN;

/* Default syslog facility is LOG_USER */
int currentLogfacilityLevel = LOG_USER;

/* Callback notified whenever the active logfile changes. */
void (*logFileChangedCallback)(const TCHAR *logFile);

/* Stores a carefully malloced filename of the most recent log file change.   This value is only set in log_printf(), and only cleared in maintainLogger(). */
TCHAR *pendingLogFileChange = NULL;

TCHAR *logFilePath;
TCHAR *currentLogFileName;
TCHAR *workLogFileName;
size_t logFileNameSize;
int logFileRollMode = ROLL_MODE_SIZE;
int logFileUmask = 0022;
TCHAR *logLevelNames[] = { TEXT("NONE  "), TEXT("DEBUG "), TEXT("INFO  "), TEXT("STATUS"), TEXT("WARN  "), TEXT("ERROR "), TEXT("FATAL "), TEXT("ADVICE"), TEXT("NOTICE") };
TCHAR *defaultLoginfoSourceName = TEXT("wrapper");
TCHAR *loginfoSourceName = NULL;
int  logFileMaxSize = -1;
int  logFileMaxLogFiles = -1;
TCHAR *logFilePurgePattern = NULL;
int  logFilePurgeSortMode = WRAPPER_FILE_SORT_MODE_TIMES;

TCHAR logFileLastNowDate[9];
/* Defualt formats (Must be 4 chars) */
TCHAR consoleFormat[32];
TCHAR logfileFormat[32];
/* Flag to keep track of whether the console output should be flushed or not. */
int consoleFlush = FALSE;

/* Flags to contol where error log level output goes to the console. */
int consoleFatalToStdErr = TRUE;
int consoleErrorToStdErr = TRUE;
int consoleWarnToStdErr = FALSE;

/* Number of seconds since the Wrapper was launched. */
int uptimeSeconds = 0;
/* TRUE once the uptime is so large that it is meaningless. */
int uptimeFlipped = FALSE;

/* Internal function declaration */
#ifdef WIN32
void sendEventlogMessage( int source_id, int level, const TCHAR *szBuff );
#else
void sendLoginfoMessage( int source_id, int level, const TCHAR *szBuff );
#endif
#ifdef WIN32
void writeToConsole( HANDLE hdl, TCHAR *lpszFmt, ...);
#endif
void checkAndRollLogs(const TCHAR *nowDate);
int lockLoggingMutex();
int releaseLoggingMutex();

/* Any log messages generated within signal handlers must be stored until we
 *  have left the signal handler to avoid deadlocks in the logging code.
 *  Messages are stored in a round robin buffer of log messages until
 *  maintainLogger is next called.
 * When we are inside of a signal, and thus when calling log_printf_queue,
 *  we know that it is safe to modify the queue as needed.  But it is possible
 *  that a signal could be fired while we are in maintainLogger, so case is
 *  taken to make sure that volatile changes are only made in log_printf_queue.
 */
#define QUEUE_SIZE 20
#define QUEUED_BUFFER_SIZE_USABLE (512 + 1)
#define QUEUED_BUFFER_SIZE (QUEUED_BUFFER_SIZE_USABLE + 4)
int queueWrapped[WRAPPER_THREAD_COUNT];
int queueWriteIndex[WRAPPER_THREAD_COUNT];
int queueReadIndex[WRAPPER_THREAD_COUNT];
TCHAR queueMessages[WRAPPER_THREAD_COUNT][QUEUE_SIZE][QUEUED_BUFFER_SIZE];
int queueSourceIds[WRAPPER_THREAD_COUNT][QUEUE_SIZE];
int queueLevels[WRAPPER_THREAD_COUNT][QUEUE_SIZE];

/* Thread specific work buffers. */
int threadSets[WRAPPER_THREAD_COUNT];
#ifdef WIN32
DWORD threadIds[WRAPPER_THREAD_COUNT];
#else
pthread_t threadIds[WRAPPER_THREAD_COUNT];
#endif
TCHAR *threadMessageBuffer = NULL;
size_t threadMessageBufferSize = 0;
TCHAR *threadPrintBuffer = NULL;
size_t threadPrintBufferSize = 0;

/* Flag which gets set when a log entry is written to the log file. */
int logFileAccessed = FALSE;

/* Logger file pointer.  It is kept open under high log loads but closed whenever it has been idle. */
FILE *logfileFP = NULL;

/** Flag which controls whether or not the logfile is auto closed after each line. */
int autoCloseLogfile = 0;

/* The number of lines sent to the log file since the getLogfileActivity method was last called. */
DWORD logfileActivityCount;


/* Mutex for syncronization of the log_printf function. */
#ifdef WIN32
HANDLE log_printfMutexHandle = NULL;
#else
pthread_mutex_t log_printfMutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#ifdef WIN32
HANDLE consoleStdoutHandle = NULL;
void setConsoleStdoutHandle( HANDLE stdoutHandle ) {
    consoleStdoutHandle = stdoutHandle;
}
#endif

void outOfMemory(const TCHAR *context, int id) {
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Out of memory (%s%02d). %s"),
        context, id, getLastErrorText());
}

/* This can be called from within logging code that would otherwise get stuck in recursion.
 *  Log to the console exactly when it happens and then also try to get it into the log
 *  file at the next oportunity. */
void outOfMemoryQueued(const TCHAR *context, int id) {
    _tprintf(TEXT("Out of memory (%s%02d). %s\n"), context, id, getLastErrorText());
    log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Out of memory (%s%02d). %s"),
        context, id, getLastErrorText());
}


void invalidMultiByteSequence(const TCHAR *context, int id) {
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Invalid multibyte Sequence found in (%s%02d). %s"),
        context, id, getLastErrorText());
}

/**
 * Replaces one token with another.  The length of the new token must be equal
 *  to or less than that of the old token.
 *
 * newToken may be null, implying "".
 */
TCHAR *replaceStringLongWithShort(TCHAR *string, const TCHAR *oldToken, const TCHAR *newToken) {
    size_t oldLen = _tcslen(oldToken);
    size_t newLen;
    TCHAR *in = string;
    TCHAR *out = string;

    if (newToken) {
        newLen = _tcslen(newToken);
    } else {
        newLen = 0;
    }

    /* Assertion check. */
    if (newLen > oldLen) {
        return string;
    }

    while (in[0] != L'\0') {
        if (_tcsncmp(in, oldToken, oldLen) == 0) {
            /* Found the oldToken.  Replace it with the new. */
            if (newLen > 0) {
                _tcsncpy(out, newToken, newLen);
            }
            in += oldLen;
            out += newLen;
        }
        else
        {
            out[0] = in[0];
            in++;
            out++;
        }
    }
    out[0] = L'\0';

    return string;
}

/**
 * Initializes the logger.  Returns 0 if the operation was successful.
 */
int initLogging(void (*logFileChanged)(const TCHAR *logFile)) {
    int threadId, i;

    logFileChangedCallback = logFileChanged;

#ifdef WIN32
    if (!(log_printfMutexHandle = CreateMutex(NULL, FALSE, NULL))) {
        _tprintf(TEXT("Failed to create logging mutex. %s\n"), getLastErrorText());
        return 1;
    }
#endif

    loginfoSourceName = defaultLoginfoSourceName;

    logFileAccessed = FALSE;
    logFileLastNowDate[0] = L'\0';

    for ( threadId = 0; threadId < WRAPPER_THREAD_COUNT; threadId++ ) {
        threadSets[threadId] = FALSE;
        /* threadIds[threadId] = 0; */

        for ( i = 0; i < QUEUE_SIZE; i++ )
        {
            queueWrapped[threadId] = 0;
            queueWriteIndex[threadId] = 0;
            queueReadIndex[threadId] = 0;
            queueMessages[threadId][i][0] = TEXT('\0');
            queueSourceIds[threadId][i] = 0;
            queueLevels[threadId][i] = 0;
        }
    }
    return 0;
}

/**
 * Disposes of any logging resouces prior to shutdown.
 */
int disposeLogging() {
#ifdef WIN32
 #ifdef WRAPPERW
    int i;
 #endif
    
    if (log_printfMutexHandle) {
        if (!CloseHandle(log_printfMutexHandle))
        {
            _tprintf(TEXT("Unable to close Logging Mutex handle. %s\n"), getLastErrorText());
            return 1;
        }
    }
 #ifdef WRAPPERW
    for (i = 0; i < dialogLogEntries; i++) {
        free(dialogLogs[i]);
    }
    free(dialogLogs);
 #endif
#endif
    if (threadPrintBuffer && threadPrintBufferSize > 0) {
        free(threadPrintBuffer);
    }
    if (threadMessageBuffer && threadMessageBufferSize > 0) {
        free(threadMessageBuffer);
    }


    if (logFilePath) {
        free(logFilePath);
        logFilePath = NULL;
    }
    if (currentLogFileName) {
        free(currentLogFileName);
        currentLogFileName = NULL;
    }
    if (workLogFileName) {
        free(workLogFileName);
        workLogFileName = NULL;
    }
    if (loginfoSourceName != defaultLoginfoSourceName) {
        free(loginfoSourceName);
        loginfoSourceName = NULL;
    }
    if (logfileFP) {
        fclose(logfileFP);
        logfileFP = NULL;
    }
    return 0;
}

/** Registers the calling thread so it can be recognized when it calls
 *  again later. */
void logRegisterThread( int thread_id ) {
#ifdef WIN32
    DWORD threadId;
    threadId = GetCurrentThreadId();
#else
    pthread_t threadId;
    threadId = pthread_self();
#endif

#ifdef _DEBUG
    _tprintf(TEXT("logRegisterThread(%d)\n"), thread_id);
#endif
    if ( thread_id >= 0 && thread_id < WRAPPER_THREAD_COUNT )
    {
        threadSets[thread_id] = TRUE;
        threadIds[thread_id] = threadId;
#ifdef _DEBUG
        _tprintf(TEXT("logRegisterThread(%d) found\n"), thread_id);
#endif
    }
}

int getThreadId() {
    int i;
#ifdef WIN32
    DWORD threadId;
    threadId = GetCurrentThreadId();
#else
    pthread_t threadId;
    threadId = pthread_self();
#endif
    /*_tprintf(TEXT("threadId=%lu\n"), threadId );*/

    for ( i = 0; i < WRAPPER_THREAD_COUNT; i++ ) {
#ifdef WIN32
        if (threadSets[i] && (threadIds[i] == threadId)) {
#else
        if (threadSets[i] && pthread_equal(threadIds[i], threadId)) {
#endif
            return i;
        }
    }

    _tprintf( TEXT("WARNING - Encountered an unknown thread %ld in getThreadId().\n"),
        (long int)threadId
        );
    return 0; /* WRAPPER_THREAD_SIGNAL */
}

int getLogfileRollModeForName( const TCHAR *logfileRollName ) {
    if (strcmpIgnoreCase(logfileRollName, TEXT("NONE")) == 0) {
        return ROLL_MODE_NONE;
    } else if (strcmpIgnoreCase(logfileRollName, TEXT("SIZE")) == 0) {
        return ROLL_MODE_SIZE;
    } else if (strcmpIgnoreCase(logfileRollName, TEXT("WRAPPER")) == 0) {
        return ROLL_MODE_WRAPPER;
    } else if (strcmpIgnoreCase(logfileRollName, TEXT("JVM")) == 0) {
        return ROLL_MODE_JVM;
    } else if (strcmpIgnoreCase(logfileRollName, TEXT("SIZE_OR_WRAPPER")) == 0) {
        return ROLL_MODE_SIZE_OR_WRAPPER;
    } else if (strcmpIgnoreCase(logfileRollName, TEXT("SIZE_OR_JVM")) == 0) {
        return ROLL_MODE_SIZE_OR_JVM;
    } else if (strcmpIgnoreCase(logfileRollName, TEXT("DATE")) == 0) {
        return ROLL_MODE_DATE;
    } else {
        return ROLL_MODE_UNKNOWN;
    }
}

int getLogLevelForName( const TCHAR *logLevelName ) {
    if (strcmpIgnoreCase(logLevelName, TEXT("NONE")) == 0) {
        return LEVEL_NONE;
    } else if (strcmpIgnoreCase(logLevelName, TEXT("NOTICE")) == 0) {
        return LEVEL_NOTICE;
    } else if (strcmpIgnoreCase(logLevelName, TEXT("ADVICE")) == 0) {
        return LEVEL_ADVICE;
    } else if (strcmpIgnoreCase(logLevelName, TEXT("FATAL")) == 0) {
        return LEVEL_FATAL;
    } else if (strcmpIgnoreCase(logLevelName, TEXT("ERROR")) == 0) {
        return LEVEL_ERROR;
    } else if (strcmpIgnoreCase(logLevelName, TEXT("WARN")) == 0) {
        return LEVEL_WARN;
    } else if (strcmpIgnoreCase(logLevelName, TEXT("STATUS")) == 0) {
        return LEVEL_STATUS;
    } else if (strcmpIgnoreCase(logLevelName, TEXT("INFO")) == 0) {
        return LEVEL_INFO;
    } else if (strcmpIgnoreCase(logLevelName, TEXT("DEBUG")) == 0) {
        return LEVEL_DEBUG;
    } else {
        return LEVEL_UNKNOWN;
    }
}

#ifndef WIN32
int getLogFacilityForName( const TCHAR *logFacilityName ) {
    if (strcmpIgnoreCase(logFacilityName, TEXT("USER")) == 0) {
      return LOG_USER;
    } else if (strcmpIgnoreCase(logFacilityName, TEXT("LOCAL0")) == 0) {
      return LOG_LOCAL0;
    } else if (strcmpIgnoreCase(logFacilityName, TEXT("LOCAL1")) == 0) {
      return LOG_LOCAL1;
    } else if (strcmpIgnoreCase(logFacilityName, TEXT("LOCAL2")) == 0) {
      return LOG_LOCAL2;
    } else if (strcmpIgnoreCase(logFacilityName, TEXT("LOCAL3")) == 0) {
      return LOG_LOCAL3;
    } else if (strcmpIgnoreCase(logFacilityName, TEXT("LOCAL4")) == 0) {
      return LOG_LOCAL4;
    } else if (strcmpIgnoreCase(logFacilityName, TEXT("LOCAL5")) == 0) {
      return LOG_LOCAL5;
    } else if (strcmpIgnoreCase(logFacilityName, TEXT("LOCAL6")) == 0) {
      return LOG_LOCAL6;
    } else if (strcmpIgnoreCase(logFacilityName, TEXT("LOCAL7")) == 0) {
      return LOG_LOCAL7;
    } else {
      return LOG_USER;
    }
}
#endif

/**
 * Sets the number of milliseconds to allow logging to take before a warning is logged.
 *  Defaults to 0 for no limit.  Possible values 0 to 3600000.
 *
 * @param threshold Warning threashold.
 */
void setLogWarningThreshold(int threshold) {
    logPrintfWarnThreshold = __max(__min(threshold, 3600000), 0);
}

/**
 * Sets the log levels to a silence so we never output anything.
 */
void setSilentLogLevels() {
    setConsoleLogLevelInt(LEVEL_NONE);
#ifdef WRAPPERW
    setDialogLogLevelInt(LEVEL_NONE);
#endif
    setLogfileLevelInt(LEVEL_NONE);
    setSyslogLevelInt(LEVEL_NONE);
}

/**
 * Sets the console log levels to a simple format for help and usage messages.
 */
void setSimpleLogLevels() {
    /* Force the log levels to control output. */
    setConsoleLogFormat(TEXT("M"));
    setConsoleLogLevelInt(LEVEL_INFO);

    setLogfileLevelInt(LEVEL_NONE);
    setSyslogLevelInt(LEVEL_NONE);
}

/* Logfile functions */
int isLogfileAccessed() {
    return logFileAccessed;
}

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
extern int setLogfilePath(const TCHAR *log_file_path, const TCHAR *workingDir, int preload) {
    size_t len = _tcslen(log_file_path);
#ifdef WIN32
    TCHAR *c;
#endif

    logFileNameSize = len + 10 + 1;
    if (logFilePath) {
        free(logFilePath);
        free(currentLogFileName);
        free(workLogFileName);
    }
    logFilePath = NULL;
    currentLogFileName = NULL;
    workLogFileName = NULL;

    logFilePath = malloc(sizeof(TCHAR) * (len + 1));
    if (!logFilePath) {
        outOfMemoryQueued(TEXT("SLP"), 1);
        return TRUE;
    }
    _tcsncpy(logFilePath, log_file_path, len + 1);

    currentLogFileName = malloc(sizeof(TCHAR) * (len + 10 + 1));
    if (!currentLogFileName) {
        outOfMemoryQueued(TEXT("SLP"), 2);
        free(logFilePath);
        logFilePath = NULL;
        return TRUE;
    }
    currentLogFileName[0] = TEXT('\0');
    workLogFileName = malloc(sizeof(TCHAR) * (len + 10 + 1));
    if (!workLogFileName) {
        outOfMemoryQueued(TEXT("SLP"), 3);
        free(logFilePath);
        logFilePath = NULL;
        free(currentLogFileName);
        logFileNameSize = 0;
        currentLogFileName = NULL;
        return TRUE;
    }
    workLogFileName[0] = TEXT('\0');

#ifdef WIN32
    /* To avoid problems on some windows systems, the '/' characters must
     *  be replaced by '\' characters in the specified path. */
    c = (TCHAR *)logFilePath;
    while((c = _tcschr(c, TEXT('/'))) != NULL) {
        c[0] = TEXT('\\');
    }
#endif
    
    return FALSE;
}

const TCHAR *getLogfilePath()
{
    return logFilePath;
}

/**
 * Returns a snapshot of the current log file path.  This call safely gets the current path
 *  and returns a copy.  It is the responsibility of the caller to free up the memory on
 *  return.  Could return null if there was an error.
 */
TCHAR *getCurrentLogfilePath() {
    TCHAR *logFileCopy;
    
    /* Lock the logging mutex. */
    if (lockLoggingMutex()) {
        return NULL;
    }
    
    /* We should always have a current log file name here because there will be at least one line of log output before this is called.
     *  If that is false then we will return an empty length, but valid, string. */
    logFileCopy = malloc(sizeof(TCHAR) * (_tcslen(currentLogFileName) + 1));
    if (!logFileCopy) {
        _tprintf(TEXT("Out of memory in logging code (%s)\n"), TEXT("P3"));
    } else {
        _tcsncpy(logFileCopy, currentLogFileName, _tcslen(currentLogFileName) + 1);
    }

    /* Release the lock we have on the logging mutex so that other threads can get in. */
    if (releaseLoggingMutex()) {
        if (logFileCopy) {
            free(logFileCopy);
        }
        return NULL;
    }
    
    return logFileCopy;
}


/**
 * Check the directory of the current logfile path to make sure it is writable.
 *  If there are any problems, log a warning.
 *
 * @return TRUE if there were any problems.
 */
int checkLogfileDir() {
    size_t len;
    TCHAR *c;
    TCHAR *logFileDir;
    TCHAR *testfile;
    int fd;
    
    len = _tcslen(logFilePath) + 1;
    logFileDir = malloc(len * sizeof(TCHAR));
    if (!logFileDir) {
        outOfMemory(TEXT("CLD"), 1);
        return TRUE;
    }
    _tcsncpy(logFileDir, logFilePath, len);
    
#ifdef WIN32
    c = _tcsrchr(logFileDir, TEXT('\\'));
#else
    c = _tcsrchr(logFileDir, TEXT('/'));
#endif
    if (c) {
        c[0] = TEXT('\0');
        
        /* We want to try writing a test file to the configured log directory to make sure it is writable. */
        len = _tcslen(logFileDir) + 23 + 1 + 1000;
        testfile = malloc(len * sizeof(TCHAR));
        if (!testfile) {
            outOfMemory(TEXT("CLD"), 1);
            free(logFileDir);
            return TRUE;
        }
        
        _sntprintf(testfile, len, TEXT("%s%c.wrapper_test-%.4d%.4d"),
            logFileDir,
#ifdef WIN32
            TEXT('\\'),
#else
            TEXT('/'),
#endif
            rand() % 9999, rand() % 9999);
        
        if ((fd = _topen(testfile, O_WRONLY | O_CREAT | O_EXCL
#ifdef WIN32
                , _S_IWRITE
#else
                , S_IRUSR | S_IWUSR
#endif 
                )) == -1) {
            if (errno == EACCES) {
                log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                    TEXT("Unable to write to the configured log directory: %s (%s)\n  The Wrapper may also have problems writing or rolling the log file.\n  Please make sure that the current user has read/write access."),
                    logFileDir, getLastErrorText());
            } else if (errno == ENOENT) {
                log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                    TEXT("Unable to write to the configured log directory: %s (%s)\n  The directory does not exist."),
                    logFileDir, getLastErrorText());
            }
        } else {
            /* Successfully wrote the temp file. */
#ifdef WIN32
            _close(fd);
#else
            close(fd);
#endif
            if (_tremove(testfile)) {
                log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                    TEXT("Unable to remove temporary file: %s (%s)\n  The Wrapper may also have problems writing or rolling the log file.\n  Please make sure that the current user has read/write access."),
                    testfile, getLastErrorText());
            }
        }
        
        free(testfile);
    }
    
    free(logFileDir);
    
    return FALSE;
}
    

void setLogfileRollMode( int log_file_roll_mode ) {
    logFileRollMode = log_file_roll_mode;
}

int getLogfileRollMode() {
    return logFileRollMode;
}

void setLogfileUmask( int log_file_umask ) {
    logFileUmask = log_file_umask;
}

void setLogfileFormat( const TCHAR *log_file_format ) {
    if ( log_file_format != NULL ) {
        _tcsncpy( logfileFormat, log_file_format, 32 );
        
        /* We only want to time logging if it is needed. */
        if ((logPrintfWarnThreshold <= 0) && (_tcschr(log_file_format, TEXT('G')))) {
            logPrintfWarnThreshold = 99999999;
        }
    }
}

void setLogfileLevelInt( int log_file_level ) {
    currentLogfileLevel = log_file_level;
}

int getLogfileLevelInt() {
    return currentLogfileLevel;
}

void setLogfileLevel( const TCHAR *log_file_level ) {
    setLogfileLevelInt(getLogLevelForName(log_file_level));
}

void setLogfileMaxFileSize( const TCHAR *max_file_size ) {
    int multiple, i, newLength;
    TCHAR *tmpFileSizeBuff;
    TCHAR chr;

    if ( max_file_size != NULL ) {
        /* Allocate buffer */
        tmpFileSizeBuff = malloc(sizeof(TCHAR) * (_tcslen( max_file_size ) + 1));
        if (!tmpFileSizeBuff) {
            outOfMemoryQueued(TEXT("SLMFS"), 1);
            return;
        }

        /* Generate multiple and remove unwanted chars */
        multiple = 1;
        newLength = 0;
        for( i = 0; i < (int)_tcslen(max_file_size); i++ ) {
            chr = max_file_size[i];

            switch( chr ) {
                case TEXT('k'): /* Kilobytes */
                case TEXT('K'):
                    multiple = 1024;
                break;

                case TEXT('M'): /* Megabytes */
                case TEXT('m'):
                    multiple = 1048576;
                break;
            }

            if( (chr >= TEXT('0') && chr <= TEXT('9')) || (chr == TEXT('-')) )
                tmpFileSizeBuff[newLength++] = max_file_size[i];
        }
        tmpFileSizeBuff[newLength] = TEXT('\0');/* Crop string */

        logFileMaxSize = _ttoi( tmpFileSizeBuff );
        if( logFileMaxSize > 0 )
            logFileMaxSize *= multiple;

        /* Free memory */
        free( tmpFileSizeBuff );
        tmpFileSizeBuff = NULL;

        if ((logFileMaxSize > 0) && (logFileMaxSize < 1024)) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT(
                "wrapper.logfile.maxsize must be 0 or at least 1024.  Changing to %d."), logFileMaxSize);
            logFileMaxSize = 1024;
        }
    }
}

void setLogfileMaxFileSizeInt( int max_file_size ) {
    logFileMaxSize = max_file_size;
}

void setLogfileMaxLogFiles( int max_log_files ) {
    logFileMaxLogFiles = max_log_files;
}

void setLogfilePurgePattern(const TCHAR *pattern) {
    size_t len;

    if (logFilePurgePattern) {
        free(logFilePurgePattern);
        logFilePurgePattern = NULL;
    }

    len = _tcslen(pattern);
    if (len > 0) {
        logFilePurgePattern = malloc(sizeof(TCHAR) * (len + 1));
        if (!logFilePurgePattern) {
            outOfMemoryQueued(TEXT("SLPP"), 1);
            return;
        }
        _tcsncpy(logFilePurgePattern, pattern, len + 1);
    }
}

void setLogfilePurgeSortMode(int sortMode) {
    logFilePurgeSortMode = sortMode;
}

/** Returns the number of lines of log file activity since the last call. */
DWORD getLogfileActivity() {
    DWORD logfileLines;

    /* Don't worry about synchronization here.  Any errors are not critical the way this is used. */
    logfileLines = logfileActivityCount;
    logfileActivityCount = 0;

    return logfileLines;
}

/** Obtains a lock on the logging mutex. */
int lockLoggingMutex() {
#ifdef WIN32
    switch (WaitForSingleObject(log_printfMutexHandle, INFINITE)) {
    case WAIT_ABANDONED:
        _tprintf(TEXT("Logging mutex was abandoned.\n"));
        return -1;
    case WAIT_FAILED:
        _tprintf(TEXT("Logging mutex wait failed.\n"));
        return -1;
    case WAIT_TIMEOUT:
        _tprintf(TEXT("Logging mutex wait timed out.\n"));
        return -1;
    default:
        /* Ok */
        break;
    }
#else
    if (pthread_mutex_lock(&log_printfMutex)) {
        _tprintf(TEXT("Failed to lock the Logging mutex. %s\n"), getLastErrorText());
        return -1;
    }
#endif

    return 0;
}

/** Releases a lock on the logging mutex. */
int releaseLoggingMutex() {
#ifdef WIN32
    if (!ReleaseMutex(log_printfMutexHandle)) {
        _tprintf( TEXT("Failed to release logging mutex. %s\n"), getLastErrorText());
        return -1;
    }
#else
    if (pthread_mutex_unlock(&log_printfMutex)) {
        _tprintf(TEXT("Failed to unlock the Logging mutex. %s\n"), getLastErrorText());
        return -1;
    }
#endif
    return 0;
}

/** Closes the logfile if it is open. */
void closeLogfile() {
    /* We need to be very careful that only one thread is allowed in here
     *  at a time.  On Windows this is done using a Mutex object that is
     *  initialized in the initLogging function. */
    if (lockLoggingMutex()) {
        return;
    }

    if (logfileFP != NULL) {
#ifdef _DEBUG
        _tprintf(TEXT("Closing logfile by request...\n"));
#endif

        fclose(logfileFP);
        logfileFP = NULL;
        /* Do not clean the currentLogFileName here as the name is not actually changing. */
    }

    /* Release the lock we have on this function so that other threads can get in. */
    if (releaseLoggingMutex()) {
        return;
    }
}

/** Sets the auto close log file flag. */
void setLogfileAutoClose(int autoClose) {
    autoCloseLogfile = autoClose;
}

/** Flushes any buffered logfile output to the disk. */
void flushLogfile() {
    /* We need to be very careful that only one thread is allowed in here
     *  at a time.  On Windows this is done using a Mutex object that is
     *  initialized in the initLogging function. */
    if (lockLoggingMutex()) {
        return;
    }

    if (logfileFP != NULL) {
#ifdef _DEBUG
        _tprintf(TEXT("Flushing logfile by request...\n"));
#endif

        fflush(logfileFP);
    }

    /* Release the lock we have on this function so that other threads can get in. */
    if (releaseLoggingMutex()) {
        return;
    }
}

/* Console functions */
void setConsoleLogFormat( const TCHAR *console_log_format ) {
    if ( console_log_format != NULL ) {
        _tcsncpy( consoleFormat, console_log_format, 32 );
        
        /* We only want to time logging if it is needed. */
        if ((logPrintfWarnThreshold <= 0) && (_tcschr(console_log_format, TEXT('G')))) {
            logPrintfWarnThreshold = 99999999;
        }
    }
}

void setConsoleLogLevelInt( int console_log_level ) {
    currentConsoleLevel = console_log_level;
}

int getConsoleLogLevelInt() {
    return currentConsoleLevel;
}

void setConsoleLogLevel( const TCHAR *console_log_level ) {
    setConsoleLogLevelInt(getLogLevelForName(console_log_level));
}

void setConsoleFlush( int flush ) {
    consoleFlush = flush;
}

void setConsoleFatalToStdErr(int toStdErr) {
    consoleFatalToStdErr = toStdErr;
}

void setConsoleErrorToStdErr(int toStdErr) {
    consoleErrorToStdErr = toStdErr;
}

void setConsoleWarnToStdErr(int toStdErr) {
    consoleWarnToStdErr = toStdErr;
}

/* Syslog/eventlog functions */
void setSyslogLevelInt( int loginfo_level ) {
    currentLoginfoLevel = loginfo_level;
}

int getSyslogLevelInt() {
    return currentLoginfoLevel;
}

void setSyslogLevel( const TCHAR *loginfo_level ) {
    setSyslogLevelInt(getLogLevelForName(loginfo_level));
}

#ifndef WIN32
void setSyslogFacilityInt( int logfacility_level ) {
    currentLogfacilityLevel = logfacility_level;
}

void setSyslogFacility( const TCHAR *loginfo_level ) {
    setSyslogFacilityInt(getLogFacilityForName(loginfo_level));
}
#endif

void setSyslogEventSourceName( const TCHAR *event_source_name ) {
    if (event_source_name != NULL) {
        if (loginfoSourceName != defaultLoginfoSourceName) {
            free(loginfoSourceName);
        }
        loginfoSourceName = malloc(sizeof(TCHAR) * (_tcslen(event_source_name) + 1));
        if (!loginfoSourceName) {
            _tprintf(TEXT("Out of memory in logging code (%s)\n"), TEXT("SSESN"));
            loginfoSourceName = defaultLoginfoSourceName;
            return;
        }

        _tcsncpy(loginfoSourceName, event_source_name, _tcslen(event_source_name) + 1);
        if (_tcslen(loginfoSourceName) > 32) {
            loginfoSourceName[32] = TEXT('\0');
        }
    }
}

int getLowLogLevel() {
    int lowLogLevel = (currentLogfileLevel < currentConsoleLevel ? currentLogfileLevel : currentConsoleLevel);
    lowLogLevel =  (currentLoginfoLevel < lowLogLevel ? currentLoginfoLevel : lowLogLevel);
    return lowLogLevel;
}

TCHAR* preparePrintBuffer(size_t reqSize) {
    if (threadPrintBuffer == NULL) {
        threadPrintBuffer = malloc(sizeof(TCHAR) * reqSize);
        if (!threadPrintBuffer) {
            _tprintf(TEXT("Out of memory in logging code (%s)\n"), TEXT("PPB1"));
            threadPrintBufferSize = 0;
            return NULL;
        }
        threadPrintBufferSize = reqSize;
    } else if (threadPrintBufferSize < reqSize) {
        free(threadPrintBuffer);
        threadPrintBuffer = malloc(sizeof(TCHAR) * reqSize);
        if (!threadPrintBuffer) {
            _tprintf(TEXT("Out of memory in logging code (%s)\n"), TEXT("PPB2"));
            threadPrintBufferSize = 0;
            return NULL;
        }
        threadPrintBufferSize = reqSize;
    }

    return threadPrintBuffer;
}

/* Writes to and then returns a buffer that is reused by the current thread.
 *  It should not be released. */
TCHAR* buildPrintBuffer( int source_id, int level, int threadId, int queued, struct tm *nowTM, int nowMillis, const TCHAR *format, const TCHAR *message ) {
    int       i;
    size_t    reqSize;
    int       numColumns;
    TCHAR      *pos;
    int       currentColumn;
    int       handledFormat;

    /* Decide the number of columns and come up with a required length for the printBuffer. */
    reqSize = 0;
    for( i = 0, numColumns = 0; i < (int)_tcslen( format ); i++ ) {
        switch( format[i] ) {
        case TEXT('P'):
            reqSize += 8 + 3;
            numColumns++;
            break;

        case TEXT('L'):
            reqSize += 6 + 3;
            numColumns++;
            break;

        case TEXT('D'):
            reqSize += 7 + 3;
            numColumns++;
            break;

        case TEXT('Q'):
            reqSize += 1 + 3;
            numColumns++;
            break;

        case TEXT('T'):
            reqSize += 19 + 3;
            numColumns++;
            break;

        case TEXT('Z'):
            reqSize += 23 + 3;
            numColumns++;
            break;

        case TEXT('U'):
            reqSize += 8 + 3;
            numColumns++;
            break;

        case TEXT('G'):
            reqSize += 10 + 3;
            numColumns++;
            break;

        case TEXT('M'):
            reqSize += _tcslen( message ) + 3;
            numColumns++;
            break;
        }
    }

    /* Always add room for the null. */
    reqSize += 1;

    if ( !preparePrintBuffer(reqSize)) {
        return NULL;
    }

    /* Always start with a null terminated string in case there are no formats specified. */
    threadPrintBuffer[0] = TEXT('\0');

    /* Create a pointer to the beginning of the print buffer, it will be advanced
     *  as the formatted message is build up. */
    pos = threadPrintBuffer;

    /* We now have a buffer large enough to store the entire formatted message. */
    for( i = 0, currentColumn = 0; i < (int)_tcslen( format ); i++ ) {
        handledFormat = 1;

        switch( format[i] ) {
        case TEXT('P'):
            switch ( source_id ) {
            case WRAPPER_SOURCE_WRAPPER:
                pos += _sntprintf( pos, reqSize, TEXT("wrapper ") );
                break;

            case WRAPPER_SOURCE_PROTOCOL:
                pos += _sntprintf( pos, reqSize, TEXT("wrapperp") );
                break;

            default:
                pos += _sntprintf( pos, reqSize, TEXT("jvm %-4d"), source_id );
                break;
            }
            currentColumn++;
            break;

        case TEXT('L'):
            pos += _sntprintf( pos, reqSize, TEXT("%s"), logLevelNames[ level ] );
            currentColumn++;
            break;

        case TEXT('D'):
            switch ( threadId )
            {
            case WRAPPER_THREAD_SIGNAL:
                pos += _sntprintf( pos, reqSize, TEXT("signal ") );
                break;

            case WRAPPER_THREAD_MAIN:
                pos += _sntprintf( pos, reqSize, TEXT("main   ") );
                break;

            case WRAPPER_THREAD_SRVMAIN:
                pos += _sntprintf( pos, reqSize, TEXT("srvmain") );
                break;

            case WRAPPER_THREAD_TIMER:
                pos += _sntprintf( pos, reqSize, TEXT("timer  ") );
                break;

            case WRAPPER_THREAD_JAVAIO:
                pos += _sntprintf( pos, reqSize, TEXT("javaio ") );
                break;

            default:
                pos += _sntprintf( pos, reqSize, TEXT("unknown") );
                break;
            }
            currentColumn++;
            break;

        case TEXT('Q'):
            pos += _sntprintf( pos, reqSize, TEXT("%c"), ( queued ? TEXT('Q') : TEXT(' ')));
            currentColumn++;
            break;

        case TEXT('T'):
            pos += _sntprintf( pos, reqSize, TEXT("%04d/%02d/%02d %02d:%02d:%02d"),
                nowTM->tm_year + 1900, nowTM->tm_mon + 1, nowTM->tm_mday,
                nowTM->tm_hour, nowTM->tm_min, nowTM->tm_sec );
            currentColumn++;
            break;

        case TEXT('Z'):
            pos += _sntprintf( pos, reqSize, TEXT("%04d/%02d/%02d %02d:%02d:%02d.%03d"),
                nowTM->tm_year + 1900, nowTM->tm_mon + 1, nowTM->tm_mday,
                nowTM->tm_hour, nowTM->tm_min, nowTM->tm_sec, nowMillis );
            currentColumn++;
            break;
            
        case TEXT('U'):
            if (uptimeFlipped) {
                pos += _sntprintf( pos, reqSize, TEXT("--------") );
            } else {
                pos += _sntprintf( pos, reqSize, TEXT("%8d"), uptimeSeconds);
            }
            currentColumn++;
            break;
            
        case TEXT('G'):
            pos += _sntprintf( pos, reqSize, TEXT("%8d"), __min(previousLogLag, 99999999));
            currentColumn++;
            break;

        case TEXT('M'):
            pos += _sntprintf( pos, reqSize, TEXT("%s"), message );
            currentColumn++;
            break;

        default:
            handledFormat = 0;
        }

        /* Add separator chars */
        if ( handledFormat && ( currentColumn != numColumns ) ) {
            pos += _sntprintf( pos, reqSize, TEXT(" | ") );
        }
    }

    /* Return the print buffer to the caller. */
    return threadPrintBuffer;
}

void forceFlush(FILE *fp) {
    int lastError;

    fflush(fp);
    lastError = getLastError();
}

/**
 * Generates a log file name given.
 *
 * buffer - Buffer into which to _sntprintf the generated name.
 * template - Template from which the name is generated.
 * nowDate - Optional date used to replace any YYYYMMDD tokens.
 * rollNum - Optional roll number used to replace any ROLLNUM tokens.
 */
void generateLogFileName(TCHAR *buffer, const TCHAR *template, const TCHAR *nowDate, const TCHAR *rollNum ) {
    /* Copy the template to the buffer to get started. */
    _tcsncpy(buffer, template, _tcslen(logFilePath) + 11);

    /* Handle the date token. */
    if (_tcsstr(buffer, TEXT("YYYYMMDD"))) {
        if (nowDate == NULL) {
            /* The token needs to be removed. */
            replaceStringLongWithShort(buffer, TEXT("-YYYYMMDD"), NULL);
            replaceStringLongWithShort(buffer, TEXT("_YYYYMMDD"), NULL);
            replaceStringLongWithShort(buffer, TEXT(".YYYYMMDD"), NULL);
            replaceStringLongWithShort(buffer, TEXT("YYYYMMDD"), NULL);
        } else {
            /* The token needs to be replaced. */
            replaceStringLongWithShort(buffer, TEXT("YYYYMMDD"), nowDate);
        }
    }

    /* Handle the roll number token. */
    if (_tcsstr(buffer, TEXT("ROLLNUM"))) {
        if (rollNum == NULL ) {
            /* The token needs to be removed. */
            replaceStringLongWithShort(buffer, TEXT("-ROLLNUM"), NULL);
            replaceStringLongWithShort(buffer, TEXT("_ROLLNUM"), NULL);
            replaceStringLongWithShort(buffer, TEXT(".ROLLNUM"), NULL);
            replaceStringLongWithShort(buffer, TEXT("ROLLNUM"), NULL);
        } else {
            /* The token needs to be replaced. */
            replaceStringLongWithShort(buffer, TEXT("ROLLNUM"), rollNum);
        }
    } else {
        /* The name did not contain a ROLLNUM token. */
        if (rollNum != NULL ) {
            /* Generate the name as if ".ROLLNUM" was appended to the template. */
            _sntprintf(buffer + _tcslen(buffer), logFileNameSize, TEXT(".%s"), rollNum);
        }
    }
}

/**
 * Prints the contents of a buffer to all configured targets.
 *
 * Must be called while locked.
 *
 * @return True if the logfile name changed.
 */
int log_printf_message( int source_id, int level, int threadId, int queued, TCHAR *message ) {
    int         logFileChanged = FALSE;
    TCHAR       *subMessage;
    TCHAR       *nextLF;
    TCHAR       *printBuffer;
    int         old_umask;
#ifndef WRAPPERW
    FILE        *target;
#endif
    TCHAR       nowDate[9];
#ifdef WIN32
    struct _timeb timebNow;
#else
    size_t      reqSize;
    struct timeval timevalNow;
    TCHAR       intBuffer[3];
    TCHAR*      pos;
#endif
    time_t      now;
    int         nowMillis;
    struct tm   *nowTM;

    /* If the message contains line feeds then break up the line into substrings and recurse. */
    subMessage = message;
    nextLF = _tcschr(subMessage, TEXT('\n'));
    if (nextLF) {
        /* This string contains more than one line.   Loop over the strings.  It is Ok to corrupt this string because it is only used once. */
        while (nextLF) {
            nextLF[0] = TEXT('\0');
            logFileChanged |= log_printf_message(source_id, level, threadId, queued, subMessage);
            
            /* Locate the next one. */
            subMessage = &(nextLF[1]);
            nextLF = _tcschr(subMessage, TEXT('\n'));
        }
        
        /* The rest of the buffer will be the final line. */
        logFileChanged |= log_printf_message(source_id, level, threadId, queued, subMessage);
        
        return logFileChanged;
    }
    
#ifdef WIN32
#else
    /* See if this is a special case log entry from the forked child. */
    if (_tcsstr(message, LOG_FORK_MARKER) == message) {
        /* Found the marker.  We only want to log the message as is to the console with a special prefix.
         *  This is used to pass the log output through the pipe to the parent Wrapper process where it
         *  will be decoded below and displayed appropriately. */
        reqSize = _tcslen(LOG_SPECIAL_MARKER) + 1 + 2 + 1 + 2 + 1 + 2 + 1 + _tcslen(message) - _tcslen(LOG_FORK_MARKER) + 1;
        if (!(printBuffer = preparePrintBuffer(reqSize))) {
            return FALSE;
        }
        _sntprintf(printBuffer, reqSize, TEXT("%s|%02d|%02d|%02d|%s"), LOG_SPECIAL_MARKER, source_id, level, threadId, message + _tcslen(LOG_FORK_MARKER));
        
        /* Decide where to send the output. */
        switch (level) {
        case LEVEL_FATAL:
            if (consoleFatalToStdErr) {
                target = stderr;
            } else {
                target = stdout;
            }
            break;
            
        case LEVEL_ERROR:
            if (consoleErrorToStdErr) {
                target = stderr;
            } else {
                target = stdout;
            }
            break;
            
        case LEVEL_WARN:
            if (consoleWarnToStdErr) {
                target = stderr;
            } else {
                target = stdout;
            }
            break;
            
        default:
            target = stdout;
            break;
        }
        
        _ftprintf(target, TEXT("%s\n"), printBuffer);
        if (consoleFlush) {
            fflush(target);
        }
        return FALSE;
    } else if ((_tcsstr(message, LOG_SPECIAL_MARKER) == message) && (_tcslen(message) >= _tcslen(LOG_SPECIAL_MARKER) + 10)) {
        /* Got a special encoded log message from the child process.   Parse it and continue as if the log
         *  message came from this process. */
        pos = (TCHAR *)(message + _tcslen(LOG_SPECIAL_MARKER) + 1);

        /* source_id */
        _tcsncpy(intBuffer, pos, 2);
        intBuffer[2] = TEXT('\0');
        source_id = _ttoi(intBuffer);
        pos += 3;

        /* level */
        _tcsncpy(intBuffer, pos, 2);
        intBuffer[2] = TEXT('\0');
        level = _ttoi(intBuffer);
        pos += 3;

        /* threadId */
        _tcsncpy(intBuffer, pos, 2);
        intBuffer[2] = TEXT('\0');
        threadId = _ttoi(intBuffer);
        pos += 3;

        /* message */
        message = pos;
    }
#endif

    /* Build a timestamp */
#ifdef WIN32
    _ftime( &timebNow );
    now = (time_t)timebNow.time;
    nowMillis = timebNow.millitm;
#else
    gettimeofday( &timevalNow, NULL );
    now = (time_t)timevalNow.tv_sec;
    nowMillis = timevalNow.tv_usec / 1000;
#endif
    nowTM = localtime( &now );
    if ( threadId < 0 )
    {
        /* The current thread was specified.  Resolve what thread this actually is. */
        threadId = getThreadId();
    }

    /* Console output by format */
    if( level >= currentConsoleLevel ) {
        /* Build up the printBuffer. */
        printBuffer = buildPrintBuffer( source_id, level, threadId, queued, nowTM, nowMillis, consoleFormat, message );
        if (printBuffer) {
            /* Decide where to send the output. */
            switch (level) {
                case LEVEL_FATAL:
                    if (consoleFatalToStdErr) {
                        target = stderr;
                    } else {
                        target = stdout;
                    }
                    break;
                    
                case LEVEL_ERROR:
                    if (consoleErrorToStdErr) {
                        target = stderr;
                    } else {
                        target = stdout;
                    }
                    break;
                    
                case LEVEL_WARN:
                    if (consoleWarnToStdErr) {
                        target = stderr;
                    } else {
                        target = stdout;
                    }
                    break;
                    
                default:
                    target = stdout;
                    break;
            }

            /* Write the print buffer to the console. */
#ifdef WIN32
            /* Using the WinAPI function WriteConsole would make it impossible to pipe the console output */		    
            /*
            if ((target == stdout) && (GetStdHandle(STD_OUTPUT_HANDLE) != NULL)) {
                writeToConsole(GetStdHandle(STD_OUTPUT_HANDLE), TEXT("%s\n"), printBuffer);
            } else if ((target == stderr) && (GetStdHandle(STD_ERROR_HANDLE) != NULL)) {
                writeToConsole(GetStdHandle(STD_ERROR_HANDLE), TEXT("%s\n"), printBuffer);
            } else
            */
            if (TRUE) {
#endif                
                _ftprintf(target, TEXT("%s\n"), printBuffer);
                if (consoleFlush) {
                    fflush(target);
                }
#ifdef WIN32
            }
#endif
        }
    }

    /* Logfile output by format */

    /* Log the message to the log file */
    if (level >= currentLogfileLevel) {
        /* If the log file was set to a blank value then it will not be used. */
        if ( logFilePath && ( _tcslen( logFilePath ) > 0 ) )
        {
            /* If this the roll mode is date then we need a nowDate for this log entry. */
            if (logFileRollMode & ROLL_MODE_DATE) {
                _sntprintf(nowDate, 9, TEXT("%04d%02d%02d"), nowTM->tm_year + 1900, nowTM->tm_mon + 1, nowTM->tm_mday );
            } else {
                nowDate[0] = TEXT('\0');
            }

            /* Make sure that the log file does not need to be rolled. */
            checkAndRollLogs(nowDate);

            /* If the file needs to be opened then do so. */
            if (logfileFP == NULL) {
                /* Generate the log file name if it is not already set. */
                if (currentLogFileName[0] == TEXT('\0')) {
                    if (logFileRollMode & ROLL_MODE_DATE) {
                        generateLogFileName(currentLogFileName, logFilePath, nowDate, NULL);
                    } else {
                        generateLogFileName(currentLogFileName, logFilePath, NULL, NULL);
                    }
                    logFileChanged = TRUE;
                }

                old_umask = umask( logFileUmask );
                logfileFP = _tfopen(currentLogFileName, TEXT("a"));
                if (logfileFP == NULL) {
                    /* The log file could not be opened. */
                    log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                        TEXT("Unable to write to the configured log file: %s (%s)\n  Falling back to the default file in the current working directory: %s"),
                        currentLogFileName, getLastErrorText(), TEXT("wrapper.log"));
                    
                    /* Try the default file location. */
                    setLogfilePath(TEXT("wrapper.log"), NULL, TRUE);
                    _sntprintf(currentLogFileName, logFileNameSize, TEXT("wrapper.log"));
                    logFileChanged = TRUE;
                    logfileFP = _tfopen(currentLogFileName, TEXT("a"));
                    if (logfileFP == NULL) {
                        /* Still unable to write. */
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                            TEXT("Unable to write to the default log file: %s (%s)\n  Disabling log file."),
                            currentLogFileName, getLastErrorText());
                        setLogfileLevelInt(LEVEL_NONE);
                        logFileChanged = FALSE;
                    }
                }
                umask(old_umask);

#ifdef _DEBUG
                if (logfileFP != NULL) {
                    _tprintf(TEXT("Opened logfile\n"));
                }
#endif
            }

            if (logfileFP == NULL) {
                currentLogFileName[0] = TEXT('\0');
                /* Failure to write to logfile already reported. */
            } else {
                /* We need to store the date the file was opened for. */
                _tcsncpy(logFileLastNowDate, nowDate, 9);

                /* Build up the printBuffer. */
                printBuffer = buildPrintBuffer( source_id, level, threadId, queued, nowTM, nowMillis, logfileFormat, message );
                if (printBuffer) {
                    _ftprintf( logfileFP, TEXT("%s\n"), printBuffer );
                    logFileAccessed = TRUE;

                    /* Increment the activity counter. */
                    logfileActivityCount++;

                    /* Only close the file if autoClose is set.  Otherwise it will be closed later
                     *  after an appropriate period of inactivity. */
                    if (autoCloseLogfile) {
#ifdef _DEBUG
                        _tprintf(TEXT("Closing logfile immediately...\n"));
#endif

                        fclose(logfileFP);
                        logfileFP = NULL;
                        /* Do not clear the currentLogFileName here as we are not changing its name. */
                    }

                    /* Leave the file open.  It will be closed later after a period of inactivity. */
                }
            }
        }
    }

    /* Loginfo/Eventlog if levels match (not by format timecodes/status allready exists in evenlog) */
    switch ( level ) {
    case LEVEL_NOTICE:
    case LEVEL_ADVICE:
        /* Advice and Notice level messages are special in that they never get logged to the
         *  EventLog / SysLog. */
        break;

    default:
        if ( level >= currentLoginfoLevel ) {
#ifdef WIN32
                sendEventlogMessage(source_id, level, message);
#else
                sendLoginfoMessage(source_id, level, message);
#endif
        }
    }
    return logFileChanged;
}


/* General log functions */
void log_printf( int source_id, int level, const TCHAR *lpszFmt, ... ) {
    va_list     vargs;
    int         count;
    int         threadId;
    int         logFileChanged;
    TCHAR       *logFileCopy;
#if defined(UNICODE) && !defined(WIN32)
    TCHAR       *msg = NULL;
    int         i, flag;
#endif
#ifdef WIN32
    struct _timeb timebNow;
#else
    struct timeval timevalNow;
#endif
    time_t      startNow;
    int         startNowMillis;
    time_t      endNow;
    int         endNowMillis;
    
    /* If we are checking on the log time then store the start time. */
    if (logPrintfWarnThreshold > 0) {
#ifdef WIN32
        _ftime(&timebNow);
        startNow = (time_t)timebNow.time;
        startNowMillis = timebNow.millitm;
#else
        gettimeofday(&timevalNow, NULL);
        startNow = (time_t)timevalNow.tv_sec;
        startNowMillis = timevalNow.tv_usec / 1000;
#endif
    } else {
        startNow = 0;
        startNowMillis = 0;
    }

    /* We need to be very careful that only one thread is allowed in here
     *  at a time.  On Windows this is done using a Mutex object that is
     *  initialized in the initLogging function. */
    if (lockLoggingMutex()) {
        return;
    }
#if defined(UNICODE) && !defined(WIN32)
    if (wcsstr(lpszFmt, TEXT("%s")) != NULL) {
        msg = malloc(sizeof(wchar_t) * (wcslen(lpszFmt) + 1));
        if (msg) {
            /* Loop over the format and convert all '%s' patterns to %S' so the UNICODE displays correctly. */
            if (wcslen(lpszFmt) > 0) {
                for (i = 0; i < _tcslen(lpszFmt); i++){
                    msg[i] = lpszFmt[i];
                    if ((lpszFmt[i] == TEXT('%')) && (i  < _tcslen(lpszFmt)) && (lpszFmt[i + 1] == TEXT('s')) && ((i == 0) || (lpszFmt[i - 1] != TEXT('%')))){
                        msg[i+1] = TEXT('S'); i++;
                    }
                }
            }
            msg[wcslen(lpszFmt)] = TEXT('\0');
        } else {
            _tprintf(TEXT("Out of memory in logging code (%s)\n"), TEXT("P0"));
            return;
        }
        flag = TRUE;
    } else {
        msg = (TCHAR*) lpszFmt;
        flag = FALSE;
    }
#endif
    threadId = getThreadId();

    /* Loop until the buffer is large enough that we are able to successfully
     *  print into it. Once the buffer has grown to the largest message size,
     *  smaller messages will pass through this code without looping. */
    do {
        if ( threadMessageBufferSize == 0 )
        {
            /* No buffer yet. Allocate one to get started. */
            threadMessageBufferSize = 100;
            threadMessageBuffer = malloc(sizeof(TCHAR) * threadMessageBufferSize);
            if (!threadMessageBuffer) {
                _tprintf(TEXT("Out of memory in logging code (%s)\n"), TEXT("P1"));
                threadMessageBufferSize = 0;
#if defined(UNICODE) && !defined(WIN32)
                if (flag == TRUE) {
                    free(msg);
                }
#endif
                return;
            }
        }

        /* Try writing to the buffer. */
        va_start( vargs, lpszFmt );
#if defined(UNICODE) && !defined(WIN32)
        count = _vsntprintf( threadMessageBuffer, threadMessageBufferSize, msg, vargs );
#else
        count = _vsntprintf( threadMessageBuffer, threadMessageBufferSize, lpszFmt, vargs );
#endif
        va_end( vargs );
        /*
        _tprintf(TEXT(" vsnprintf->%d, size=%d\n"), count, threadMessageBufferSize );
        */
        if ( ( count < 0 ) || ( count >= (int)threadMessageBufferSize ) ) {
            /* If the count is exactly equal to the buffer size then a null TCHAR was not written.
             *  It must be larger.
             * Windows will return -1 if the buffer is too small. If the number is
             *  exact however, we still need to expand it to have room for the null.
             * UNIX will return the required size. */

            /* Free the old buffer for starters. */
            free( threadMessageBuffer );

            /* Decide on a new buffer size. */
            if ( count <= (int)threadMessageBufferSize ) {
                threadMessageBufferSize += 100;
            } else if ( count + 1 <= (int)threadMessageBufferSize + 100 ) {
                threadMessageBufferSize += 100;
            } else {
                threadMessageBufferSize = count + 1;
            }

            threadMessageBuffer = malloc(sizeof(TCHAR) * threadMessageBufferSize);
            if (!threadMessageBuffer) {
                _tprintf(TEXT("Out of memory in logging code (%s)\n"), TEXT("P2"));
                threadMessageBufferSize = 0;
#if defined(UNICODE) && !defined(WIN32)
                if (flag == TRUE) {
                    free(msg);
                }
#endif
                return;
            }

            /* Always set the count to -1 so we will loop again. */
            count = -1;
        }
    } while ( count < 0 );
#if defined(UNICODE) && !defined(WIN32)
    if (flag == TRUE) {
        free(msg);
    }
#endif
    logFileCopy = NULL;
    logFileChanged = log_printf_message( source_id, level, threadId, FALSE, threadMessageBuffer );
    if (logFileChanged) {
        /* We need to enqueue a notification that the log file name was changed.
         *  We can NOT directly send the notification here as that could cause a deadlock,
         *  depending on where exactly this function was called from. (See Wrapper protocol mutex.) */
        logFileCopy = malloc(sizeof(TCHAR) * (_tcslen(currentLogFileName) + 1));
        if (!logFileCopy) {
            _tprintf(TEXT("Out of memory in logging code (%s)\n"), TEXT("P3"));
        } else {
            _tcsncpy(logFileCopy, currentLogFileName, _tcslen(currentLogFileName) + 1);
            /* Now after we have 100% prepared the log file name.  Put into the queue variable
             *  so the maintainLogging() function can safely grab it at any time.
             * The reading code is also in a semaphore so we can do a quick test here safely as well. */
            if (pendingLogFileChange) {
                /* The previous file was still in the queue.  Free it up to avoid a memory leak.
                 *  This can happen if the log file size is 1k or something like that.   We will always
                 *  keep the most recent file however, so this should not be that big a problem. */
#ifdef _DEBUG
                _tprintf(TEXT("Log file name change was overwritten in queue: %s\n"), pendingLogFileChange);
#endif
                free(pendingLogFileChange);
            }
            pendingLogFileChange = logFileCopy;
        }
    }

    /* Release the lock we have on this function so that other threads can get in. */
    if (releaseLoggingMutex()) {
        return;
    }

    /* If we are checking on the log time then store the stop time.
     *  It is Ok that some of the error paths don't make it this far. */
    if (logPrintfWarnThreshold > 0) {
#ifdef WIN32
        _ftime(&timebNow);
        endNow = (time_t)timebNow.time;
        endNowMillis = timebNow.millitm;
#else
        gettimeofday(&timevalNow, NULL);
        endNow = (time_t)timevalNow.tv_sec;
        endNowMillis = timevalNow.tv_usec / 1000;
#endif
        previousLogLag = __min(endNow - startNow, 3600) * 1000 + endNowMillis - startNowMillis;
        if (previousLogLag >= logPrintfWarnThreshold) {
            log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Write to log took %d milliseconds."), previousLogLag);
        }
    }
}

/* Internal functions */

/**
 * Create an error message from GetLastError() using the
 *  FormatMessage API Call...
 */
#ifdef WIN32
TCHAR lastErrBuf[1024];
TCHAR* getLastErrorText() {
    DWORD dwRet;
    TCHAR* lpszTemp = NULL;

    dwRet = FormatMessage( FORMAT_MESSAGE_ALLOCATE_BUFFER |
                           FORMAT_MESSAGE_FROM_SYSTEM |FORMAT_MESSAGE_ARGUMENT_ARRAY,
                           NULL,
                           GetLastError(),
                           LANG_NEUTRAL,
                           (TCHAR*)&lpszTemp,
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
    if (req < 0) {
        invalidMultiByteSequence(TEXT("GLET"), 1);
        return NULL;
    }
    t = malloc(sizeof(TCHAR) * (req + 1));
    if (!t) {
        _tprintf(TEXT("Out of memory in logging code (%s)\n"), TEXT("GLET1"));
        return NULL;
    }
    mbstowcs(t, c, req + 1);
    return t;

#else
    return strerror(errno);
#endif
}
int getLastError() {
    return errno;
}
#endif

int registerSyslogMessageFile( ) {
#ifdef WIN32
    TCHAR buffer[_MAX_PATH];
    DWORD usedLen;
    TCHAR regPath[1024];
    HKEY hKey;
    DWORD categoryCount, typesSupported;

    /* Get absolute path to service manager */
    usedLen = GetModuleFileName(NULL, buffer, _MAX_PATH);
    if (usedLen == 0) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to obtain the full path to the Wrapper. %s"), getLastErrorText());
        return -1;
    } else if ((usedLen == _MAX_PATH) || (getLastError() == ERROR_INSUFFICIENT_BUFFER)) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to obtain the full path to the Wrapper. %s"), TEXT("Path to Wrapper binary too long."));
        return -1;
    } else {
        _sntprintf( regPath, 1024, TEXT("SYSTEM\\CurrentControlSet\\Services\\Eventlog\\Application\\%s"), loginfoSourceName );

        if( RegCreateKey( HKEY_LOCAL_MACHINE, regPath, (PHKEY) &hKey ) == ERROR_SUCCESS ) {
            RegCloseKey( hKey );

            if( RegOpenKeyEx( HKEY_LOCAL_MACHINE, regPath, 0, KEY_WRITE, (PHKEY) &hKey ) == ERROR_SUCCESS ) {
                /* Set EventMessageFile */
                if( RegSetValueEx( hKey, TEXT("EventMessageFile"), (DWORD) 0, (DWORD) REG_SZ, (LPBYTE) buffer, (DWORD)(sizeof(TCHAR) * (_tcslen(buffer) + 1))) != ERROR_SUCCESS ) {
                    RegCloseKey( hKey );
                    return -1;
                }

                /* Set CategoryMessageFile */
                if( RegSetValueEx( hKey, TEXT("CategoryMessageFile"), (DWORD) 0, (DWORD) REG_SZ, (LPBYTE) buffer, (DWORD)(sizeof(TCHAR) * (_tcslen(buffer) + 1))) != ERROR_SUCCESS ) {
                    RegCloseKey( hKey );
                    return -1;
                }

                /* Set CategoryCount */
                categoryCount = 12;
                if( RegSetValueEx( hKey, TEXT("CategoryCount"), (DWORD) 0, (DWORD) REG_DWORD, (LPBYTE) &categoryCount, sizeof(DWORD) ) != ERROR_SUCCESS ) {
                    RegCloseKey( hKey );
                    return -1;
                }

                /* Set TypesSupported */
                typesSupported = 7;
                if( RegSetValueEx( hKey, TEXT("TypesSupported"), (DWORD) 0, (DWORD) REG_DWORD, (LPBYTE) &typesSupported, sizeof(DWORD) ) != ERROR_SUCCESS ) {
                    RegCloseKey( hKey );
                    return -1;
                }

                RegCloseKey( hKey );
                return 0;
            }
        }
    }

    return -1; /* Failure */
#else
    return 0;
#endif
}

int unregisterSyslogMessageFile( ) {
#ifdef WIN32
    /* If we deregister this application, then the event viewer will not work when the program is not running. */
    /* Don't want to clutter up the Registry, but is there another way?  */
    TCHAR regPath[ 1024 ];

    /* Get absolute path to service manager */
    _sntprintf( regPath, 1024, TEXT("SYSTEM\\CurrentControlSet\\Services\\Eventlog\\Application\\%s"), loginfoSourceName );

    if( RegDeleteKey( HKEY_LOCAL_MACHINE, regPath ) == ERROR_SUCCESS )
        return 0;

    return -1; /* Failure */
#else
    return 0;
#endif
}

#ifdef WIN32
void sendEventlogMessage( int source_id, int level, const TCHAR *szBuff ) {
    TCHAR   header[16];
    const TCHAR   **strings;
    WORD   eventType;
    HANDLE handle;
    WORD   eventID, categoryID;
    int    result;

    strings = malloc(sizeof(TCHAR *) * 3);
    if (!strings) {
        _tprintf(TEXT("Out of memory in logging code (%s)\n"), TEXT("SEM1"));
        return;
    }

    /* Build the source header */
    switch(source_id) {
    case WRAPPER_SOURCE_WRAPPER:
        _sntprintf( header, 16, TEXT("wrapper") );
        break;

    case WRAPPER_SOURCE_PROTOCOL:
        _sntprintf( header, 16, TEXT("wrapperp") );
        break;

    default:
        _sntprintf( header, 16, TEXT("jvm %d"), source_id );
        break;
    }

    /* Build event type by level */
    switch(level) {
    case LEVEL_NOTICE: /* Will not get in here. */
    case LEVEL_ADVICE: /* Will not get in here. */
    case LEVEL_FATAL:
        eventType = EVENTLOG_ERROR_TYPE;
        break;

    case LEVEL_ERROR:
    case LEVEL_WARN:
        eventType = EVENTLOG_WARNING_TYPE;
        break;

    case LEVEL_STATUS:
    case LEVEL_INFO:
    case LEVEL_DEBUG:
        eventType = EVENTLOG_INFORMATION_TYPE;
        break;
    }

    /* Set the category id to the appropriate resource id. */
    if ( source_id == WRAPPER_SOURCE_WRAPPER ) {
        categoryID = MSG_EVENT_LOG_CATEGORY_WRAPPER;
    } else if ( source_id == WRAPPER_SOURCE_PROTOCOL ) {
        categoryID = MSG_EVENT_LOG_CATEGORY_PROTOCOL;
    } else {
        /* Source is a JVM. */
        switch ( source_id ) {
        case 1:
            categoryID = MSG_EVENT_LOG_CATEGORY_JVM1;
            break;

        case 2:
            categoryID = MSG_EVENT_LOG_CATEGORY_JVM2;
            break;

        case 3:
            categoryID = MSG_EVENT_LOG_CATEGORY_JVM3;
            break;

        case 4:
            categoryID = MSG_EVENT_LOG_CATEGORY_JVM4;
            break;

        case 5:
            categoryID = MSG_EVENT_LOG_CATEGORY_JVM5;
            break;

        case 6:
            categoryID = MSG_EVENT_LOG_CATEGORY_JVM6;
            break;

        case 7:
            categoryID = MSG_EVENT_LOG_CATEGORY_JVM7;
            break;

        case 8:
            categoryID = MSG_EVENT_LOG_CATEGORY_JVM8;
            break;

        case 9:
            categoryID = MSG_EVENT_LOG_CATEGORY_JVM9;
            break;

        default:
            categoryID = MSG_EVENT_LOG_CATEGORY_JVMXX;
            break;
        }
    }

    /* Place event in eventlog */
    strings[0] = header;
    strings[1] = szBuff;
    strings[2] = 0;
    eventID = level;

    handle = RegisterEventSource( NULL, loginfoSourceName );
    if( !handle )
        return;

    result = ReportEvent(
        handle,                   /* handle to event log */
        eventType,                /* event type */
        categoryID,               /* event category */
        MSG_EVENT_LOG_MESSAGE,    /* event identifier */
        NULL,                     /* user security identifier */
        2,                        /* number of strings to merge */
        0,                        /* size of binary data */
        (const TCHAR**) strings,  /* array of strings to merge */
        NULL                      /* binary data buffer */
    );
    if (result == 0) {
        /* If there are any errors accessing the event log, like it is full, then disable its output. */
        setSyslogLevelInt(LEVEL_NONE);

        /* Recurse so this error gets set in the log file and console.  The syslog
         *  output has been disabled so we will not get back here. */
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to write to the EventLog due to: %s"), getLastErrorText());
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Internally setting wrapper.syslog.loglevel=NONE to prevent further messages."));
    }

    DeregisterEventSource( handle );

    free( (void *) strings );
    strings = NULL;
}
#else
void sendLoginfoMessage( int source_id, int level, const TCHAR *szBuff ) {
    int eventType;

    /* Build event type by level */
    switch( level ) {
        case LEVEL_FATAL:
            eventType = LOG_CRIT;
        break;

        case LEVEL_ERROR:
            eventType = LOG_ERR;
        break;

        case LEVEL_WARN:
        case LEVEL_STATUS:
            eventType = LOG_NOTICE;
        break;

        case LEVEL_INFO:
            eventType = LOG_INFO;
        break;

        case LEVEL_DEBUG:
            eventType = LOG_DEBUG;
        break;

        default:
            eventType = LOG_DEBUG;
    }

    _topenlog( loginfoSourceName, LOG_PID | LOG_NDELAY, currentLogfacilityLevel );
    _tsyslog( eventType, szBuff );
    closelog( );
}
#endif

#ifdef WIN32
int vWriteToConsoleBufferSize = 100;
TCHAR *vWriteToConsoleBuffer = NULL;
void vWriteToConsole( HANDLE hdl, TCHAR *lpszFmt, va_list vargs ) {
    int cnt;
    DWORD wrote;

    /* This should only be called if consoleStdoutHandle is set. */
    if ( consoleStdoutHandle == NULL && hdl == NULL) {
        return;
    }

    if ( vWriteToConsoleBuffer == NULL ) {
        vWriteToConsoleBuffer = malloc(sizeof(TCHAR) * vWriteToConsoleBufferSize);
        if (!vWriteToConsoleBuffer) {
            _tprintf(TEXT("Out of memory in logging code (%s)\n"), TEXT("WTC1"));
            return;
        }
    }

    /* The only way I could figure out how to write to the console
     *  returned by AllocConsole when running as a service was to
     *  do all of this special casing and use the handle to the new
     *  console's stdout and the WriteConsole function.  If anyone
     *  puzzling over this code knows a better way of doing this
     *  let me know.
     * WriteConsole takes a fixed buffer and does not do any expansions
     *  We need to prepare the string to be displayed ahead of time.
     *  This means storing the message into a temporary buffer.  The
     *  following loop will expand the global buffer to hold the current
     *  message.  It will grow as needed to handle any arbitrarily large
     *  user message.  The buffer needs to be able to hold all available
     *  characters + a null TCHAR. */
    while ( ( cnt = _vsntprintf( vWriteToConsoleBuffer, vWriteToConsoleBufferSize - 1, lpszFmt, vargs ) ) < 0 ) {
        /* Expand the size of the buffer */
        free( vWriteToConsoleBuffer );
        vWriteToConsoleBufferSize += 100;
        vWriteToConsoleBuffer = malloc(sizeof(TCHAR) * vWriteToConsoleBufferSize);
        if (!vWriteToConsoleBuffer) {
            _tprintf(TEXT("Out of memory in logging code (%s)\n"), TEXT("WTC2"));
            return;
        }
    }

    /* We can now write the message. */
    if (hdl == NULL) {
        WriteConsole(consoleStdoutHandle, vWriteToConsoleBuffer, (DWORD)_tcslen( vWriteToConsoleBuffer ), &wrote, NULL);
    } else {
        WriteConsole(hdl, vWriteToConsoleBuffer, (DWORD)_tcslen( vWriteToConsoleBuffer ), &wrote, NULL);
    }
}
void writeToConsole( HANDLE hdl, TCHAR *lpszFmt, ... ) {
    va_list        vargs;

    va_start( vargs, lpszFmt );
    vWriteToConsole( hdl, lpszFmt, vargs );
    va_end( vargs );
}
#endif

/**
 * Does a search for all files matching the specified pattern and deletes all
 *  but the most recent 'count' files.  The files are sorted by their names.
 */
void limitLogFileCount(const TCHAR *current, const TCHAR *pattern, int sortMode, int count) {
    TCHAR **files;
    int index;
    int foundCurrent;

#ifdef _DEBUG
    _tprintf(TEXT("limitLogFileCount(%s, %s, %d, %d)\n"), current, pattern, sortMode, count);
#endif

    files = wrapperFileGetFiles(pattern, sortMode);
    if (!files) {
        /* Failed */
        return;
    }

    /* When this loop runs we keep the first COUNT files in the list and everything thereafter is deleted. */
    foundCurrent = FALSE;
    index = 0;
    while (files[index]) {
        if (index < count) {
#ifdef _DEBUG
            _tprintf(TEXT("Keep files[%d] %s\n"), index, files[index]);
#endif
            if (_tcscmp(current, files[index]) == 0) {
                /* This is the current file, as expected. */
#ifdef _DEBUG
                _tprintf(TEXT("  Current\n"));
#endif
                foundCurrent = TRUE;
            }
        } else {
#ifdef _DEBUG
            _tprintf(TEXT("Delete files[%d] %s\n"), index, files[index]);
#endif
            if (_tcscmp(current, files[index]) == 0) {
                /* This is the current file, we don't want to delete it. */
                _tprintf(TEXT("Log file sort order would result in current log file being deleted: %s\n"), current);
                foundCurrent = TRUE;
            } else if (_tremove(files[index])) {
                _tprintf(TEXT("Unable to delete old log file: %s (%s)\n"), files[index], getLastErrorText());
            }
        }

        index++;
    }

    /* Now if we did not find the current file, and there are <count> files
       still in the directory, then we want to also delete the oldest one.
       Otherwise, the addition of the current file would result in too many
       files. */
    if (!foundCurrent) {
        if (index >= count) {
#ifdef _DEBUG
            _tprintf(TEXT("Delete files[%d] %s\n"), count - 1, files[count - 1]);
#endif
            if (_tremove(files[count - 1])) {
                _tprintf(TEXT("Unable to delete old log file: %s (%s)\n"), files[count - 1], getLastErrorText());
            }
        }
    }

    wrapperFileFreeFiles(files);
}

/**
 * Sets the current uptime in seconds.
 *
 * @param uptime Uptime in seconds.
 * @param flipped TRUE when the uptime is no longer meaningful.
 */
void setUptime(int uptime, int flipped) {
    uptimeSeconds = uptime;
    uptimeFlipped = flipped;
}

int rollFailure = FALSE;
/**
 * Rolls log files using the ROLLNUM system.
 */
void rollLogs() {
    int i;
    TCHAR rollNum[11];
#if defined(WIN32) && !defined(WIN64)
    struct _stat64i32 fileStat;
#else
    struct stat fileStat;
#endif
    int result;

#ifdef _DEBUG
    _tprintf(TEXT("rollLogs()\n"));
#endif
    if (!logFilePath) {
        return;
    }

    /* If the log file is currently open, it needs to be closed. */
    if (logfileFP != NULL) {
#ifdef _DEBUG
        _tprintf(TEXT("Closing logfile so it can be rolled...\n"));
#endif

        fclose(logfileFP);
        logfileFP = NULL;
        currentLogFileName[0] = TEXT('\0');
    }

#ifdef _DEBUG
    _tprintf(TEXT("Rolling log files... (rollFailure=%d)\n"), rollFailure);
#endif

    /* We don't know how many log files need to be rotated yet, so look. */
    i = 0;
    do {
        i++;
        _sntprintf(rollNum, 11, TEXT("%d"), i);
        generateLogFileName(workLogFileName, logFilePath, NULL, rollNum);
        result = _tstat(workLogFileName, &fileStat);
#ifdef _DEBUG
        if (result == 0) {
            _tprintf(TEXT("Rolled log file %s exists.\n"), workLogFileName);
        }
#endif
    } while (result == 0);

    /* Now, starting at the highest file rename them up by one index. */
    for (; i > 1; i--) {
        _tcsncpy(currentLogFileName, workLogFileName, _tcslen(logFilePath) + 11);
        _sntprintf(rollNum, 11, TEXT("%d"), i - 1);
        generateLogFileName(workLogFileName, logFilePath, NULL, rollNum);

        if ((logFileMaxLogFiles > 0) && (i > logFileMaxLogFiles) && (!logFilePurgePattern)) {
            /* The file needs to be deleted rather than rolled.   If a purge pattern was not specified,
             *  then the files will be deleted here.  Otherwise they will be deleted below. */

#ifdef _DEBUG
            _tprintf(TEXT("Remove old log file %s\n"), workLogFileName);
#endif
            if (_tremove(workLogFileName)) {
#ifdef _DEBUG
                _tprintf(TEXT("Failed to remove old log file %s. err=%d\n"), workLogFileName, getLastError());
#endif
                if (getLastError() == 2) {
                    /* The file did not exist. */
                } else if (getLastError() == 3) {
                    /* The path did not exist. */
                } else {
                    if (rollFailure == FALSE) {
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Unable to delete old log file: %s (%s)"), workLogFileName, getLastErrorText());
                    }
                    rollFailure = TRUE;
                    generateLogFileName(currentLogFileName, logFilePath, NULL, NULL); /* Set the name back so we don't cause a logfile name changed event. */
                    return;
                }
            } else {
                /* On Windows, in some cases if the file can't be deleted, we still get here without an error. Double check. */
                if (_tstat(workLogFileName, &fileStat) == 0) {
                    /* The file still existed. */
#ifdef _DEBUG
                        _tprintf(TEXT("Failed to remove old log file %s\n"), workLogFileName);
#endif
                    if (rollFailure == FALSE) {
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Unable to delete old log file: %s"), workLogFileName);
                    }
                    rollFailure = TRUE;
                    generateLogFileName(currentLogFileName, logFilePath, NULL, NULL); /* Set the name back so we don't cause a logfile name changed event. */
                    return;
                }
#ifdef _DEBUG
                else {
                    _tprintf(TEXT("Deleted %s\n"), workLogFileName);
                }
#endif
            }
        } else {
            if (_trename(workLogFileName, currentLogFileName) != 0) {
                if (rollFailure == FALSE) {
#ifdef WIN32
                    if (errno == EACCES) {
                        /* This access denied message is treated as a special case, but the use by other applications issue only happens on Windows. */
                        /* Don't log this as with other errors as that would cause recursion. */
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Unable to rename log file %s to %s.  File is in use by another application."),
                            workLogFileName, currentLogFileName);
                    } else {
#endif
                        /* Don't log this as with other errors as that would cause recursion. */
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Unable to rename log file %s to %s. (%s)"),
                            workLogFileName, currentLogFileName, getLastErrorText());
#ifdef WIN32
                    }
#endif
                } 
                rollFailure = TRUE;
                generateLogFileName(currentLogFileName, logFilePath, NULL, NULL); /* Set the name back so we don't cause a logfile name changed event. */
                return;
            }
#ifdef _DEBUG
            else {
                _tprintf(TEXT("Renamed %s to %s\n"), workLogFileName, currentLogFileName);
            }
#endif
        }
    }

    /* Rename the current file to the #1 index position */
    generateLogFileName(currentLogFileName, logFilePath, NULL, NULL);
    if (_trename(currentLogFileName, workLogFileName) != 0) {
        if (rollFailure == FALSE) {
            if (getLastError() == 2) {
                 /* File does not yet exist. */
            } else if (getLastError() == 3) {
                /* Path does not yet exist. */
            } else if (errno == 13) {
                /* Don't log this as with other errors as that would cause recursion. */
                    log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, 
                        TEXT("Unable to rename log file %s to %s.  File is in use by another application."),
                        currentLogFileName, workLogFileName);
            } else {
                /* Don't log this as with other errors as that would cause recursion. */
                log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Unable to rename log file %s to %s. (%s)"),
                    currentLogFileName, workLogFileName, getLastErrorText());
            } 
        }
        rollFailure = TRUE;
        generateLogFileName(currentLogFileName, logFilePath, NULL, NULL); /* Set the name back so we don't cause a logfile name changed event. */
        return;
    }
#ifdef _DEBUG
    else {
        _tprintf(TEXT("Renamed %s to %s\n"), currentLogFileName, workLogFileName);
    }
#endif

    /* Now limit the number of files using the standard method. */
    if (logFileMaxLogFiles > 0) {
        if (logFilePurgePattern) {
            limitLogFileCount(currentLogFileName, logFilePurgePattern, logFilePurgeSortMode, logFileMaxLogFiles + 1);
        }
    }
    if (rollFailure == TRUE) {
        /* We made it here, but the rollFailure flag had been previously set.  Make a note that we are back and then continue. */
        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
            TEXT("Logfile rolling is working again."));
    }
    rollFailure = FALSE;
    
    /* Reset the current log file name as it is not being used yet. */
    currentLogFileName[0] = TEXT('\0'); /* Log file was rolled, so we want to cause a logfile change event. */
}

/**
 * Check to see whether or not the log file needs to be rolled.
 *  This is only called when synchronized.
 */
void checkAndRollLogs(const TCHAR *nowDate) {
    long position;
#if defined(WIN32) && !defined(WIN64)
    struct _stat64i32 fileStat;
#else
    struct stat fileStat;
#endif

    /* Depending on the roll mode, decide how to roll the log file. */
    if (logFileRollMode & ROLL_MODE_SIZE) {
        /* Roll based on the size of the file. */
        if (logFileMaxSize <= 0) {
            return;
        }

        /* Find out the current size of the file.  If the file is currently open then we need to
         *  use ftell to make sure that the buffered data is also included. */
        if (logfileFP != NULL) {
            /* File is open */
            if ((position = ftell(logfileFP)) < 0) {
                _tprintf(TEXT("Unable to get the current logfile size with ftell: %s\n"), getLastErrorText());
                return;
            }
        } else {
            /* File is not open */
            if (_tstat(logFilePath, &fileStat) != 0) {
                if (getLastError() == 2) {
                    /* File does not yet exist. */
                    position = 0;
                } else if (getLastError() == 3) {
                    /* Path does not exist. */
                    position = 0;
                } else {
                    _tprintf(TEXT("Unable to get the current logfile size with stat: %s\n"), getLastErrorText());
                    return;
                }
            } else {
                position = fileStat.st_size;
            }
        }

        /* Does the log file need to rotated? */
        if (position >= logFileMaxSize) {
            rollLogs();
        }
    } else if (logFileRollMode & ROLL_MODE_DATE) {
        /* Roll based on the date of the log entry. */
        if (_tcscmp(nowDate, logFileLastNowDate) != 0) {
            /* The date has changed.  Close the file. */
            if (logfileFP != NULL) {
#ifdef _DEBUG
                _tprintf(TEXT("Closing logfile because the date changed...\n"));
#endif

                fclose(logfileFP);
                logfileFP = NULL;
            }
            /* Always reset the name so the the log file name will be regenerated correctly. */
            currentLogFileName[0] = TEXT('\0');

            /* This will happen just before a new log file is created.
             *  Check the maximum file count. */
            if (logFileMaxLogFiles > 0) {
                /* We will check for too many files here and then clear the current log file name so it will be set later. */
                generateLogFileName(currentLogFileName, logFilePath, nowDate, NULL);

                if (logFilePurgePattern) {
                    limitLogFileCount(currentLogFileName, logFilePurgePattern, logFilePurgeSortMode, logFileMaxLogFiles + 1);
                } else {
                    generateLogFileName(workLogFileName, logFilePath, TEXT("????????"), NULL);
                    limitLogFileCount(currentLogFileName, workLogFileName, WRAPPER_FILE_SORT_MODE_NAMES_DEC, logFileMaxLogFiles + 1);
                }

                currentLogFileName[0] = TEXT('\0');
                workLogFileName[0] = TEXT('\0');
            }
        }
    }
}

void log_printf_queue( int useQueue, int source_id, int level, const TCHAR *lpszFmt, ... ) {
    int threadId;
    int localWriteIndex;
    int localReadIndex;
    va_list     vargs;
    int         count;
    TCHAR       *buffer;

    /* Start by processing any arguments so that we can store a simple string. */
#ifdef _DEBUG_QUEUE
    _tprintf(TEXT("log_printf_queue(%d, %d, %d, %S)\n"), useQueue, source_id, level, lpszFmt);
#endif

#if defined(UNICODE) && !defined(WIN32)
    if (wcsstr(lpszFmt, TEXT("%s")) != NULL) {
        /* This is a coding error as strings coming into this function should NEVER use this format.
         *  If the token below is not '%S' then this would recurse. */
        log_printf_queue(useQueue, source_id, LEVEL_ERROR, TEXT("Coding Error.  String contains invalid string token for queued logging: %S"), lpszFmt);
        return;
    }
#endif
    
    /** For queued logging, we have a fixed length buffer to work with.  Just to make it easy to catch
     *   problems, always use the same sized fixed buffer even if we will be using the non-queued logging. */
    if (useQueue) {
        /* Care needs to be taken both with this code and the code below to get done as quick as possible.
         *  It is generally safe because each thread has its own queue.  The only danger is if a message is
         *  being queued while that thread is interupted by a signal.  If things are setup correctly however
         *  then non-signal threads should not be here in the first place. */
        threadId = getThreadId();
        
        localWriteIndex = queueWriteIndex[threadId];
        localReadIndex = queueReadIndex[threadId];
        
        if ((localWriteIndex == localReadIndex - 1) || ((localWriteIndex == QUEUE_SIZE - 1) && (localReadIndex == 0))) {
            _tprintf(TEXT("WARNING log queue overflow for thread[%d]:%d:%d dropping entry: %s\n"), threadId, localWriteIndex, localReadIndex, lpszFmt);
            return;
        }
        
        /* Get a reference to the message buffer we will use. */
        buffer = queueMessages[threadId][queueWriteIndex[threadId]];
    } else {
        /* This will not be queued so we can use malloc to create a new buffer. */
        buffer = malloc(sizeof(TCHAR) * QUEUED_BUFFER_SIZE);
        if (!buffer) {
            _tprintf(TEXT("Out of memory in logging code (%s)\n"), TEXT("PQ1"));
            return;
        }
        
        /* For compiler */
        threadId = -1;
        localWriteIndex = -1;
    }
    
    /* Now actually generate our buffer. */
    va_start(vargs, lpszFmt);
    count = _vsntprintf(buffer, QUEUED_BUFFER_SIZE_USABLE, lpszFmt, vargs);
    va_end(vargs);
    
    /* vswprintf returns -1 on overflow. */
    if ((count < 0) || (count >= QUEUED_BUFFER_SIZE_USABLE - 1)) {
        _tcsncat(buffer, TEXT("..."), QUEUED_BUFFER_SIZE);
    }
    
    if (useQueue) {
#ifdef _DEBUG_QUEUE
        _tprintf(TEXT("LOG ENQUEUE[%d] Thread[%d]: %s\n"), localWriteIndex, threadId, buffer);
#endif
        /* Store additional information about the call. */
        queueSourceIds[threadId][localWriteIndex] = source_id;
        queueLevels[threadId][localWriteIndex] = level;
    
        /* Lastly increment and wrap the write index. */
        queueWriteIndex[threadId]++;
        if (queueWriteIndex[threadId] >= QUEUE_SIZE) {
            queueWriteIndex[threadId] = 0;
            queueWrapped[threadId] = 1;
        }
    } else {
        /* Make a normal logging call with our new buffer.  Parameters are already expanded. */
        log_printf(source_id, level,
#if defined(UNICODE) && !defined(WIN32)
            TEXT("%S"),
#else
            TEXT("%s"),
#endif
            buffer);
        
        free(buffer);
    }
}

/**
 * Perform any required logger maintenance at regular intervals.
 *
 * One operation is to log any queued messages.  This must be done very
 *  carefully as it is possible that a signal handler could be thrown at
 *  any time as this function is being executed.
 */
void maintainLogger() {
    int localWriteIndex;
    int source_id;
    int level;
    int threadId;
    TCHAR *buffer;
    int logFileChanged;
    TCHAR *logFileCopy;
        
    /* Check to see if there is a pending log file change notification. Do this first as we could
     *  generate our own here as well.  It is important that we do our best to keep them in order.
     *  Grab it and clear the reference quick in case another is set.  This order is thread safe. */
    if (pendingLogFileChange) {
        /* Lock the logging mutex. */
        if (lockLoggingMutex()) {
            return;
        }
        
        logFileCopy = pendingLogFileChange;
        pendingLogFileChange = NULL;
        
        /* Release the lock we have on the logging mutex so that other threads can get in. */
        if (releaseLoggingMutex()) {
            return;
        }
        
        /* Now see if a log file name was queued, using our local copy. */
        if (logFileCopy) {
#ifdef _DEBUG
            _tprintf(TEXT("Sending notification of queued log file name change: %s"), logFileCopy);
#endif
            logFileChangedCallback(logFileCopy);
            free(logFileCopy);
            logFileCopy = NULL;
        }
    }
    
    for (threadId = 0; threadId < WRAPPER_THREAD_COUNT; threadId++) {
        /* NOTE - The queue variables are not synchronized so we need to access them
         *        carefully and assume that data could possibly be corrupted. */
        localWriteIndex = queueWriteIndex[threadId]; /* Snapshot the value to maintain a constant reference. */
        if ( queueReadIndex[threadId] != localWriteIndex ) {
            logFileChanged = FALSE;
            logFileCopy = NULL;

            /* Lock the logging mutex. */
            if (lockLoggingMutex()) {
                return;
            }
        
            /* Empty the queue of any logged messages. */
            localWriteIndex = queueWriteIndex[threadId]; /* Snapshot the value to maintain a constant reference. */
            while (queueReadIndex[threadId] != localWriteIndex) {
                /* Snapshot the values in the queue at that index. */
                source_id = queueSourceIds[threadId][queueReadIndex[threadId]];
                level = queueLevels[threadId][queueReadIndex[threadId]];
                buffer = queueMessages[threadId][queueReadIndex[threadId]];

                /* The buffer is static in the queue and will be reused. */
#ifdef _DEBUG_QUEUE
                _tprintf(TEXT("LOG QUEUED[%d]: %s\n"), queueReadIndex[threadId], buffer );
#endif

                logFileChanged = log_printf_message( source_id, level, threadId, TRUE, buffer );
                if (logFileChanged) {
                    logFileCopy = malloc(sizeof(TCHAR) * (_tcslen(currentLogFileName) + 1));
                    if (!logFileCopy) {
                        _tprintf(TEXT("Out of memory in logging code (%s)\n"), TEXT("ML1"));
                    } else {
                        _tcsncpy(logFileCopy, currentLogFileName, _tcslen(currentLogFileName) + 1);
                    }
                }
#ifdef _DEBUG_QUEUE
                _tprintf(TEXT("  Queue lw=%d, qw=%d, qr=%d\n"), localWriteIndex, queueWriteIndex[threadId], queueReadIndex[threadId]);
#endif
                /* Clear the string we just wrote. */
                buffer[0] = TEXT('\0');
                
                queueReadIndex[threadId]++;
                if ( queueReadIndex[threadId] >= QUEUE_SIZE ) {
                    queueReadIndex[threadId] = 0;
                }
            }

            /* Release the lock we have on the logging mutex so that other threads can get in. */
            if (releaseLoggingMutex()) {
                if (logFileChanged && logFileCopy) {
                    free(logFileCopy);
                }
                return;
            }

            /* Now that we are no longer in the semaphore, register the change of the logfile. */
            if (logFileChanged && logFileCopy) {
                logFileChangedCallback(logFileCopy);
                free(logFileCopy);
            }
        }
    }
}

