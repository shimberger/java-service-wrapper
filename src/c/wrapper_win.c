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
 */

#ifdef WIN32

#include <direct.h>
#include <io.h>
#include <math.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <windows.h>
#include <winnt.h>
#include <Sddl.h>
#include <sys/timeb.h>
#include <conio.h>
#include <Softpub.h>
#include <wincrypt.h>
#include <wintrust.h>
#include <DbgHelp.h>
#include <Pdh.h>
#include <ntsecapi.h>
#include "psapi.h"

#include "wrapper_i18n.h"
#include "resource.h"
#include "wrapper.h"
#include "wrapperinfo.h"
#include "property.h"
#include "logger.h"
#include "wrapper_file.h"


#define ENCODING (X509_ASN_ENCODING | PKCS_7_ASN_ENCODING)

typedef struct {
    LPWSTR lpszProgramName;
    LPWSTR lpszPublisherLink;
    LPWSTR lpszMoreInfoLink;
} SPROG_PUBLISHERINFO, *PSPROG_PUBLISHERINFO;



#ifndef POLICY_AUDIT_SUBCATEGORY_COUNT
/* The current SDK is pre-Vista.  Add the required definitions. */
typedef struct _TOKEN_ELEVATION {
  DWORD TokenIsElevated;
} TOKEN_ELEVATION, *PTOKEN_ELEVATION;
#define TokenElevation TokenOrigin + 3
#endif

/*****************************************************************************
 * Win32 specific variables and procedures                                   *
 *****************************************************************************/
SERVICE_STATUS          ssStatus;
SERVICE_STATUS_HANDLE   sshStatusHandle;

#define SYSTEM_PATH_MAX_LEN 256
static TCHAR *systemPath[SYSTEM_PATH_MAX_LEN];
static HANDLE wrapperChildStdoutWr = INVALID_HANDLE_VALUE;
static HANDLE wrapperChildStdoutRd = INVALID_HANDLE_VALUE;

TCHAR wrapperClasspathSeparator = TEXT(';');

HANDLE javaIOThreadHandle;
DWORD javaIOThreadId;
int javaIOThreadStarted = FALSE;
int stopJavaIOThread = FALSE;
int javaIOThreadStopped = FALSE;

HANDLE timerThreadHandle;
DWORD timerThreadId;
int timerThreadStarted = FALSE;
int stopTimerThread = FALSE;
int timerThreadStopped = FALSE;
TICKS timerTicks = WRAPPER_TICK_INITIAL;

/** Flag which keeps track of whether or not the CTRL-C key has been pressed. */
int ctrlCTrapped = FALSE;

/** Flag which keeps track of whether or not PID files should be deleted on shutdown. */
int cleanUpPIDFilesOnExit = FALSE;

TCHAR* getExceptionName(DWORD exCode);

/* Dynamically loadedfunction types. */
typedef SERVICE_STATUS_HANDLE(*FTRegisterServiceCtrlHandlerEx)(LPCTSTR, LPHANDLER_FUNCTION_EX, LPVOID);

/* Dynamically loaded functions. */
FARPROC OptionalGetProcessTimes = NULL;
FARPROC OptionalGetProcessMemoryInfo = NULL;
FTRegisterServiceCtrlHandlerEx OptionalRegisterServiceCtrlHandlerEx = NULL;

/******************************************************************************
 * Windows specific code
 ******************************************************************************/
PDH_HQUERY pdhQuery = NULL;
PDH_HCOUNTER pdhCounterPhysicalDiskAvgQueueLen = NULL;
PDH_HCOUNTER pdhCounterPhysicalDiskAvgWriteQueueLen = NULL;
PDH_HCOUNTER pdhCounterPhysicalDiskAvgReadQueueLen = NULL;
PDH_HCOUNTER pdhCounterMemoryPageFaultsPSec = NULL;
PDH_HCOUNTER pdhCounterMemoryTransitionFaultsPSec = NULL;
PDH_HCOUNTER pdhCounterProcessWrapperPageFaultsPSec = NULL;
PDH_HCOUNTER pdhCounterProcessJavaPageFaultsPSec = NULL;

#define FILEPATHSIZE 1024
/**
 * Tests whether or not the current OS is at or below the version of Windows NT.
 *
 * @return TRUE if NT 4.0 or earlier, FALSE otherwise.
 */
BOOL isWindowsNT4_0OrEarlier()
{
   OSVERSIONINFOEX osvi;
   BOOL bOsVersionInfoEx;
   BOOL retval;

   /* Try calling GetVersionEx using the OSVERSIONINFOEX structure.
    *  If that fails, try using the OSVERSIONINFO structure. */
    retval = TRUE;
    ZeroMemory(&osvi, sizeof(OSVERSIONINFOEX));
    osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);

    if (!(bOsVersionInfoEx = GetVersionEx ((OSVERSIONINFO *)&osvi))) {
       /* If OSVERSIONINFOEX doesn't work, try OSVERSIONINFO. */
        osvi.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
        if (!GetVersionEx((OSVERSIONINFO *)&osvi)) {
            retval = TRUE;
        }
    }

    if (osvi.dwMajorVersion <= 4) {
        retval = TRUE;
    } else {
        retval = FALSE;
    }

    return retval;
}

#ifdef _UNICODE
#define FUNCTION_NAME_RegisterServiceCtrlHandlerEx "RegisterServiceCtrlHandlerExW"
#else
#define FUNCTION_NAME_RegisterServiceCtrlHandlerEx "RegisterServiceCtrlHandlerExA"
#endif
void loadDLLProcs() {
    HMODULE kernel32Mod;
    HMODULE psapiMod;
    HMODULE advapi32Mod;

    /* The KERNEL32 module was added in NT 3.5. */
    if ((kernel32Mod = GetModuleHandle(TEXT("KERNEL32.DLL"))) == NULL) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
            TEXT("The KERNEL32.DLL was not found.  Some functions will be disabled."));
    } else {
        if ((OptionalGetProcessTimes = GetProcAddress(kernel32Mod, "GetProcessTimes")) == NULL) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                TEXT("The GetProcessTimes is not available in this KERNEL32.DLL version.  Some functions will be disabled."));
        }
    }

    /* The PSAPI module was added in NT 4.0. */
    if ((psapiMod = LoadLibrary(TEXT("PSAPI.DLL"))) == NULL) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
            TEXT("The PSAPI.DLL was not found.  Some functions will be disabled."));
    } else {
        if ((OptionalGetProcessMemoryInfo = GetProcAddress(psapiMod, "GetProcessMemoryInfo")) == NULL) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                TEXT("The GetProcessMemoryInfo is not available in this PSAPI.DLL version.  Some functions will be disabled."));
        }
    }

    /* The ADVAPI32 module was added in NT 5.0. */
    if ((advapi32Mod = LoadLibrary(TEXT("ADVAPI32.DLL"))) == NULL) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
            TEXT("The ADVAPI32.DLL was not found.  Some functions will be disabled."));
    } else {
        if ((OptionalRegisterServiceCtrlHandlerEx = (FTRegisterServiceCtrlHandlerEx)GetProcAddress(advapi32Mod, FUNCTION_NAME_RegisterServiceCtrlHandlerEx)) == NULL) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                TEXT("The %s is not available in this ADVAPI32.DLL version.  Some functions will be disabled."),
                FUNCTION_NAME_RegisterServiceCtrlHandlerEx);
        }
    }
}

/**
 * Builds an array in memory of the system path.
 */
int buildSystemPath() {
    TCHAR *envBuffer;
    size_t len, i;
    TCHAR *c, *lc;

    /* Get the length of the PATH environment variable. */
    len = GetEnvironmentVariable(TEXT("PATH"), NULL, 0);
    if (len == 0) {
        /* PATH not set on this system.  Not an error. */
        systemPath[0] = NULL;
        return 0;
    }

    /* Allocate the memory to hold the PATH */
    envBuffer = malloc(sizeof(TCHAR) * len);
    if (!envBuffer) {
        outOfMemory(TEXT("BSP"), 1);
        return 1;
    }
    GetEnvironmentVariable(TEXT("PATH"), envBuffer, (DWORD)len);

#ifdef _DEBUG
    _tprintf(TEXT("Getting the system path: %s\n"), envBuffer);
#endif

    /* Build an array of the path elements.  To make it easy, just
     *  assume there won't be more than 255 path elements. Verified
     *  in the loop. */
    i = 0;
    lc = envBuffer;
    /* Get the elements ending in a ';' */
    while (((c = _tcschr(lc, TEXT(';'))) != NULL) && (i < SYSTEM_PATH_MAX_LEN - 2)) {
        len = (int)(c - lc);
        systemPath[i] = malloc(sizeof(TCHAR) * (len + 1));
        if (!systemPath[i]) {
            outOfMemory(TEXT("BSP"), 2);
            return 1;
        }

        memcpy(systemPath[i], lc, sizeof(TCHAR) * len);
        systemPath[i][len] = TEXT('\0');
#ifdef _DEBUG
        _tprintf(TEXT("PATH[%d]=%s\n"), i, systemPath[i]);
#endif
        lc = c + 1;
        i++;
    }
    /* There should be one more value after the last ';' */
    len = _tcslen(lc);
    systemPath[i] = malloc(sizeof(TCHAR) * (len + 1));
    if (!systemPath[i]) {
        outOfMemory(TEXT("BSP"), 3);
        return 1;
    }
    _tcsncpy(systemPath[i], lc, len + 1);
#ifdef _DEBUG
    _tprintf(TEXT("PATH[%d]=%s\n"), i, systemPath[i]);
#endif
    i++;
    /* NULL terminate the array. */
    systemPath[i] = NULL;
#ifdef _DEBUG
    _tprintf(TEXT("PATH[%d]=<null>\n"), i);
#endif
    i++;

    /* Release the environment variable memory. */
    free(envBuffer);

    return 0;
}
TCHAR** wrapperGetSystemPath() {
    return systemPath;
}

/**
 * Initializes the invocation mutex.  Returns 1 if the mutex already exists
 *  or can not be created.  0 if this is the first instance.
 */
HANDLE invocationMutexHandle = NULL;
int initInvocationMutex() {
    TCHAR *mutexName;
    if (wrapperData->isSingleInvocation) {
        mutexName = malloc(sizeof(TCHAR) * (23 + _tcslen(wrapperData->serviceName) + 1));
        if (!mutexName) {
            outOfMemory(TEXT("IIM"), 1);
            return 1;
        }
        _sntprintf(mutexName, 23 + _tcslen(wrapperData->serviceName) + 1, TEXT("Java Service Wrapper - %s"), wrapperData->serviceName);

        if (!(invocationMutexHandle = CreateMutex(NULL, FALSE, mutexName))) {
            free(mutexName);

            if (GetLastError() == ERROR_ACCESS_DENIED) {
                /* Most likely the app is running as a service and we tried to run it as a console. */
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                    TEXT("ERROR: Another instance of the %s application is already running."),
                    wrapperData->serviceName);
                return 1;
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                    TEXT("ERROR: Unable to create the single invocation mutex. %s"),
                    getLastErrorText());
                return 1;
            }
        } else {
            free(mutexName);
        }

        if (GetLastError() == ERROR_ALREADY_EXISTS) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("ERROR: Another instance of the %s application is already running."),
                wrapperData->serviceName);
            return 1;
        }
    }

    return 0;
}

/**
 * exits the application after running shutdown code.
 */
void appExit(int exitCode) {
    /* We only want to delete the pid files if we created them. Some Wrapper
     *  invocations are meant to run in parallel with Wrapper instances
     *  controlling a JVM. */
    if (cleanUpPIDFilesOnExit) {
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
    }

    /* Close the invocation mutex if we created or looked it up. */
    if (invocationMutexHandle) {
        CloseHandle(invocationMutexHandle);
        invocationMutexHandle = NULL;
    }

    /* Common wrapper cleanup code. */
    wrapperDispose();

    /* Do this here to unregister the syslog resources on exit.*/
    /*unregisterSyslogMessageFile(); */
    exit(exitCode);
}

/**
 * Gets the error code for the last operation that failed.
 */
int wrapperGetLastError() {
    return WSAGetLastError();
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

    old_umask = _umask(newUmask);
    pid_fp = _tfopen(filename, TEXT("w"));
    _umask(old_umask);

    if (pid_fp != NULL) {
        _ftprintf(pid_fp, TEXT("%d\n"), pid);
        fclose(pid_fp);
    } else {
        return 1;
    }
    return 0;
}

/**
 * Initialize the pipe which will be used to capture the output from the child
 * process.
 */
int wrapperInitChildPipe() {
    SECURITY_ATTRIBUTES saAttr;
    HANDLE childStdoutRd = INVALID_HANDLE_VALUE;

    /* Set the bInheritHandle flag so pipe handles are inherited. */
    saAttr.nLength = sizeof(SECURITY_ATTRIBUTES);
    saAttr.lpSecurityDescriptor = NULL;
    saAttr.bInheritHandle = TRUE;

    /* Create a pipe for the child process's STDOUT. */
    if (!CreatePipe(&childStdoutRd, &wrapperChildStdoutWr, &saAttr, 0)) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Stdout pipe creation failed  Err(%ld : %s)"),
            GetLastError(), getLastErrorText());
        return -1;
    }

    /* Create a noninheritable read handle and close the inheritable read handle. */
    if (!DuplicateHandle(GetCurrentProcess(), childStdoutRd, GetCurrentProcess(), &wrapperChildStdoutRd, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("DuplicateHandle failed"));
        return -1;
    }
    CloseHandle(childStdoutRd);

    return 0;
}

/**
 * Handler to take care of the case where the user hits CTRL-C when the wrapper
 * is being run as a console.
 *
 * Handlers are called in the reverse order that they are registered until one
 *  returns TRUE.  So last registered is called first until the default handler
 *  is called.
 */
int wrapperConsoleHandler(int key) {
    /* Immediately register this thread with the logger. */
    logRegisterThread(WRAPPER_THREAD_SIGNAL);

    /* Enclose the contents of this call in a try catch block so we can
     *  display and log useful information should the need arise. */
    __try {
        switch (key) {
        case CTRL_C_EVENT:
            /* The user hit CTRL-C.  Can only happen when run as a console. */
            if (wrapperData->ignoreSignals & WRAPPER_IGNORE_SIGNALS_WRAPPER) {
                log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                    TEXT("CTRL-C trapped, but ignored."));
            } else {
                wrapperData->ctrlEventCTRLCTrapped = TRUE;
            }
            break;

        case CTRL_CLOSE_EVENT:
            /* The user tried to close the console.  Can only happen when run as a console. */
            if (wrapperData->ignoreSignals & WRAPPER_IGNORE_SIGNALS_WRAPPER) {
                log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                    TEXT("Close trapped, but ignored."));
            } else {
                wrapperData->ctrlEventCloseTrapped = TRUE;
            }
            break;

        case CTRL_BREAK_EVENT:
            /* The user hit CTRL-BREAK */
            log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                TEXT("CTRL-BREAK/PAUSE trapped.  Asking the JVM to dump its state."));

            /* If the java process was launched using the same console, ie where
             *  processflags=CREATE_NEW_PROCESS_GROUP; then the java process will
             *  also get this message, so it can be ignored here. */
            /*
            If we ever do something here, remember that this can't be called directly from here.
            wrapperRequestDumpJVMState();
            */
            break;

        case CTRL_LOGOFF_EVENT:
            wrapperData->ctrlEventLogoffTrapped = TRUE;
            break;

        case CTRL_SHUTDOWN_EVENT:
            wrapperData->ctrlEventShutdownTrapped = TRUE;
            break;

        default:
            /* Unknown.  Don't quit here. */
            log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("Trapped unexpected console signal (%d).  Ignored."), key);
        }
    } __except (exceptionFilterFunction(GetExceptionInformation())) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
            TEXT("<-- Wrapper Stopping due to error in console handler."));
        appExit(1);
    }

    return TRUE; /* We handled the event. */
}

/******************************************************************************
 * Platform specific methods
 *****************************************************************************/

/**
 * Send a signal to the JVM process asking it to dump its JVM state.
 */
void wrapperRequestDumpJVMState() {
    if (wrapperData->javaProcess != NULL) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
            TEXT("Dumping JVM state."));
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
            TEXT("Sending BREAK event to process group %ld."), wrapperData->javaPID);
        if (GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, wrapperData->javaPID) == 0) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("Unable to send BREAK event to JVM process.  Err(%ld : %s)"),
                GetLastError(), getLastErrorText());
        }
    }
}

/**
 * Build the java command line.
 *
 * @return TRUE if there were any problems.
 */
int wrapperBuildJavaCommand() {
    size_t commandLen;
    size_t commandLen2;
    TCHAR **strings;
    int length, i;

    /* If this is not the first time through, then dispose the old command */
    if (wrapperData->jvmCommand) {
#ifdef _DEBUG
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Clearing up old command line"));
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Old Command Line \"%s\""), wrapperData->jvmCommand);
#endif
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
    if (wrapperBuildJavaCommandArray(&strings, &length, TRUE, wrapperData->classpath)) {
        /* Failed. */
        return TRUE;
    }

#ifdef _DEBUG
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("JVM Command Line Parameters"));
    for (i = 0; i < length; i++) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("%d : %s"), i, strings[i]);
    }
#endif

    /* Build a single string from the array */
    /* Calculate the length */
    commandLen = 0;
    for (i = 0; i < length; i++) {
        if (i > 0) {
            commandLen++; /* Space */
        }
        commandLen += _tcslen(strings[i]);
    }
    commandLen++; /* '\0' */
    commandLen2 = commandLen;
    /* Build the actual command */
    wrapperData->jvmCommand = malloc(sizeof(TCHAR) * commandLen);
    if (!wrapperData->jvmCommand) {
        outOfMemory(TEXT("WBJC"), 1);
        return TRUE;
    }
    commandLen = 0;
    for (i = 0; i < length; i++) {
        if (i > 0) {
            wrapperData->jvmCommand[commandLen++] = TEXT(' ');
        }
        _sntprintf(wrapperData->jvmCommand + commandLen, commandLen2 - commandLen, TEXT("%s"), strings[i]);
        commandLen += _tcslen(strings[i]);
    }
    wrapperData->jvmCommand[commandLen++] = TEXT('\0');

    /* Free up the temporary command array */
    wrapperFreeJavaCommandArray(strings, length);

    return FALSE;
}

int hideConsoleWindow(HWND consoleHandle, const TCHAR *name) {
    WINDOWPLACEMENT consolePlacement;

    memset(&consolePlacement, 0, sizeof(WINDOWPLACEMENT));
    consolePlacement.length = sizeof(WINDOWPLACEMENT);

    if (IsWindowVisible(consoleHandle)) {
#ifdef _DEBUG
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("%s console window is visible, attempt to hide."), name);
#endif

        /* Hide the Window. */
        consolePlacement.showCmd = SW_HIDE;

        if (!SetWindowPlacement(consoleHandle, &consolePlacement)) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                TEXT("Unable to set window placement information: %s"), getLastErrorText());
        }

        if (IsWindowVisible(consoleHandle)) {
            if (wrapperData->isDebugging) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Failed to hide the %s console window."), name);
            }
            return FALSE;
        } else {
            if (wrapperData->isDebugging) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("%s console window hidden successfully."), name);
            }
            return TRUE;
        }
    } else {
        /* Already hidden */
        return TRUE;
    }
}

/**
 * Look for and hide the wrapper or JVM console windows if they should be hidden.
 * Some users have reported that if the user logs on to windows quickly after booting up,
 *  the console window will be redisplayed even though it was hidden once.  The forceCheck
 *  will continue to attempt to check and hide the window if this does happen for up to a
 *  predetermined period of time.
 */
void wrapperCheckConsoleWindows() {
    int forceCheck = TRUE;

    /* See if the Wrapper console needs to be hidden. */
    if (wrapperData->wrapperConsoleHide && (wrapperData->wrapperConsoleHandle != NULL) && (wrapperData->wrapperConsoleVisible || forceCheck)) {
        if (hideConsoleWindow(wrapperData->wrapperConsoleHandle, TEXT("Wrapper"))) {
            wrapperData->wrapperConsoleVisible = FALSE;
        }
    }

    /* See if the Java console needs to be hidden. */
    if ((wrapperData->jvmConsoleHandle != NULL) && (wrapperData->jvmConsoleVisible || forceCheck)) {
        if (hideConsoleWindow(wrapperData->jvmConsoleHandle, TEXT("JVM"))) {
            wrapperData->jvmConsoleVisible = FALSE;
        }
    }
}

HWND findConsoleWindow( TCHAR *title ) {
    HWND consoleHandle;
    int i = 0;

    /* Allow up to 2 seconds for the window to show up, but don't hang
     *  up if it doesn't */
    consoleHandle = NULL;
    while ((!consoleHandle) && (i < 200)) {
        wrapperSleep(10);
        consoleHandle = FindWindow(TEXT("ConsoleWindowClass"), title);
        i++;
    }

    return consoleHandle;
}

void showConsoleWindow(HWND consoleHandle, const TCHAR *name) {
    WINDOWPLACEMENT consolePlacement;

    if (wrapperData->isDebugging) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Show %s console window which JVM is launched."), name);
    }
    if (GetWindowPlacement(consoleHandle, &consolePlacement)) {
        /* Show the Window. */
        consolePlacement.showCmd = SW_SHOW;

        if (!SetWindowPlacement(consoleHandle, &consolePlacement)) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                TEXT("Unable to set window placement information: %s"), getLastErrorText());
        }
    } else {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
            TEXT("Unable to obtain window placement information: %s"), getLastErrorText());
    }
}

/**
 * The main entry point for the javaio thread which is started by
 *  initializeJavaIO().  Once started, this thread will run for the
 *  life of the process.
 *
 * This thread will only be started if we are configured to use a
 *  dedicated thread to read JVM output.
 */
DWORD WINAPI javaIORunner(LPVOID parameter) {
    int nextSleep;

    /* In case there are ever any problems in this thread, enclose it in a try catch block. */
    __try {
        javaIOThreadStarted = TRUE;

        /* Immediately register this thread with the logger. */
        logRegisterThread(WRAPPER_THREAD_JAVAIO);

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
    } __except (exceptionFilterFunction(GetExceptionInformation())) {
        /* This call is not queued to make sure it makes it to the log prior to a shutdown. */
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Fatal error in the JavaIO thread."));
        javaIOThreadStopped = TRUE; /* Before appExit() */
        appExit(1);
        return 1; /* For the compiler, we will never get here. */
    }

    javaIOThreadStopped = TRUE;
    if (wrapperData->isJavaIOOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("JavaIO thread stopped."));
    }
    return 0;
}

/**
 * Creates a process whose job is to loop and process and stdio and stderr
 *  output from the JVM.
 */
int initializeJavaIO() {
    if (wrapperData->isJavaIOOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Launching JavaIO thread."));
    }

    javaIOThreadHandle = CreateThread(
        NULL, /* No security attributes as there will not be any child processes of the thread. */
        0,    /* Use the default stack size. */
        javaIORunner,
        NULL, /* No parameters need to passed to the thread. */
        0,    /* Start the thread running immediately. */
        &javaIOThreadId);
    if (!javaIOThreadHandle) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
            TEXT("Unable to create a javaIO thread: %s"), getLastErrorText());
        return 1;
    } else {
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
DWORD WINAPI timerRunner(LPVOID parameter) {
    TICKS sysTicks;
    TICKS lastTickOffset = 0;
    TICKS tickOffset;
    TICKS nowTicks;
    int offsetDiff;
    int first = TRUE;

    /* In case there are ever any problems in this thread, enclose it in a try catch block. */
    __try {
        timerThreadStarted = TRUE;

        /* Immediately register this thread with the logger. */
        logRegisterThread(WRAPPER_THREAD_TIMER);

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
                return 1;
            }

            /* Advance the timer tick count. */
            nowTicks = timerTicks++;

            if (wrapperData->useTickMutex && wrapperReleaseTickMutex()) {
                timerThreadStopped = TRUE;
                return 1;
            }

            /* Calculate the offset between the two tick counts. This will always work due to overflow. */
            tickOffset = sysTicks - nowTicks;

            /* The number we really want is the difference between this tickOffset and the previous one. */
            offsetDiff = wrapperGetTickAgeTicks(lastTickOffset, tickOffset);

            if (first) {
                first = FALSE;
            } else {
                if (offsetDiff > wrapperData->timerSlowThreshold) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT(
                        "The timer fell behind the system clock by %dms."), (int)(offsetDiff * WRAPPER_TICK_MS));
                } else if (offsetDiff < -1 * wrapperData->timerFastThreshold) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT(
                        "The system clock fell behind the timer by %dms."), (int)(-1 * offsetDiff * WRAPPER_TICK_MS));
                }

                if (wrapperData->isTickOutputEnabled) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT(
                        "    Timer: ticks=%08x, system ticks=%08x, offset=%08x, offsetDiff=%08x"),
                        nowTicks, sysTicks, tickOffset, offsetDiff);
                }
            }

            /* Store this tick offset for the next time through the loop. */
            lastTickOffset = tickOffset;
        }
    } __except (exceptionFilterFunction(GetExceptionInformation())) {
        /* This call is not queued to make sure it makes it to the log prior to a shutdown. */
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Fatal error in the Timer thread."));
        timerThreadStopped = TRUE; /* Before appExit() */
        appExit(1);
        return 1; /* For the compiler, we will never get here. */
    }

    timerThreadStopped = TRUE;
    if (wrapperData->isTickOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Timer thread stopped."));
    }
    return 0;
}

/**
 * Creates a process whose job is to loop and simply increment a ticks
 *  counter.  The tick counter can then be used as a clock as an alternative
 *  to using the system clock.
 */
int initializeTimer() {
    if (wrapperData->isTickOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Launching Timer thread."));
    }

    timerThreadHandle = CreateThread(
        NULL, /* No security attributes as there will not be any child processes of the thread. */
        0,    /* Use the default stack size. */
        timerRunner,
        NULL, /* No parameters need to passed to the thread. */
        0,    /* Start the thread running immediately. */
        &timerThreadId);
    if (!timerThreadHandle) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
            TEXT("Unable to create a timer thread: %s"), getLastErrorText());
        return 1;
    } else {
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
    }
}

int initializeWinSock() {
    WORD ws_version=MAKEWORD(1, 1);
    WSADATA ws_data;
    int res;

    /* Initialize Winsock */
    if ((res = WSAStartup(ws_version, &ws_data)) != 0) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Cannot initialize Windows socket DLLs."));
        return res;
    }

    return 0;
}

/**
 * Collects the current process's username and domain name.
 *
 * @return TRUE if there were any problems.
 */
int collectUserInfo() {
    int result;

    DWORD processId;
    HANDLE hProcess;
    HANDLE hProcessToken;
    TOKEN_USER *tokenUser;
    DWORD tokenUserSize;

    TCHAR *sidText;
    DWORD userNameSize;
    DWORD domainNameSize;
    SID_NAME_USE sidType;

    processId = wrapperData->wrapperPID;
    wrapperData->userName = NULL;
    wrapperData->domainName = NULL;

    if (hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId)) {
        if (OpenProcessToken(hProcess, TOKEN_QUERY, &hProcessToken)) {
            GetTokenInformation(hProcessToken, TokenUser, NULL, 0, &tokenUserSize);
            tokenUser = (TOKEN_USER *)malloc(tokenUserSize);
            if (!tokenUser) {
                outOfMemory(TEXT("CUI"), 1);
                result = TRUE;
            } else {
                if (GetTokenInformation(hProcessToken, TokenUser, tokenUser, tokenUserSize, &tokenUserSize)) {
                    /* Get the text representation of the sid. */
                    if (ConvertSidToStringSid(tokenUser->User.Sid, &sidText) == 0) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Failed to Convert SId to String: %s"), getLastErrorText());
                        result = TRUE;
                    } else {
                        /* We now have an SID, use it to lookup the account. */
                        userNameSize = 0;
                        domainNameSize = 0;
                        LookupAccountSid(NULL, tokenUser->User.Sid, NULL, &userNameSize, NULL, &domainNameSize, &sidType);
                        wrapperData->userName = (TCHAR*)malloc(sizeof(TCHAR) * userNameSize);
                        if (!wrapperData->userName) {
                            outOfMemory(TEXT("CUI"), 2);
                            result = TRUE;
                        } else {
                            wrapperData->domainName = (TCHAR*)malloc(sizeof(TCHAR) * domainNameSize);
                            if (!wrapperData->domainName) {
                                outOfMemory(TEXT("CUI"), 3);
                                result = TRUE;
                            } else {
                                if (LookupAccountSid(NULL, tokenUser->User.Sid, wrapperData->userName, &userNameSize, wrapperData->domainName, &domainNameSize, &sidType)) {
                                    /* Success. */
                                    result = FALSE;
                                } else {
                                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to get the current username and domain: %s"), getLastErrorText());
                                    result = TRUE;
                                }
                            }
                        }

                        LocalFree(sidText);
                    }
                } else {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to get token information: %s"), getLastErrorText());
                    result = TRUE;
                }

                free(tokenUser);
            }

            CloseHandle(hProcessToken);
        } else {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to open process token: %s"), getLastErrorText());
            result = TRUE;
        }

        CloseHandle(hProcess);
    } else {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to open process: %s"), getLastErrorText());
        result = TRUE;
    }

    return result;
}

/**
 * Execute initialization code to get the wrapper set up.
 */
int wrapperInitializeRun() {
    HANDLE hStdout;
#ifdef WIN32
    struct _timeb timebNow;
#else
    struct timeval timevalNow;
#endif
    time_t      now;
    int         nowMillis;
    int res;
    TCHAR titleBuffer[80];

    /* Set the process priority. */
    HANDLE process = GetCurrentProcess();
    if (!SetPriorityClass(process, wrapperData->ntServicePriorityClass)) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
            TEXT("Unable to set the process priority:  %s"), getLastErrorText());
    }

    /* Initialize the random seed. */
#ifdef WIN32
    _ftime(&timebNow);
    now = (time_t)timebNow.time;
    nowMillis = timebNow.millitm;
#else
    gettimeofday(&timevalNow, NULL);
    now = (time_t)timevalNow.tv_sec;
    nowMillis = timevalNow.tv_usec / 1000;
#endif
    srand(nowMillis);

    /* Initialize the pipe to capture the child process output */
    if ((res = wrapperInitChildPipe()) != 0) {
        return res;
    }

    /* Initialize the Wrapper console handle to null */
    wrapperData->wrapperConsoleHandle = NULL;

    /* The Wrapper will not have its own console when running as a service.  We need
     *  to create one here. */
    if ((!wrapperData->isConsole) && (wrapperData->ntAllocConsole)) {
        if (wrapperData->isDebugging) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Allocating a console for the service."));
        }

        if (!AllocConsole()) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("ERROR: Unable to allocate a console for the service: %s"), getLastErrorText());
            return 1;
        }

        hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        if (hStdout == INVALID_HANDLE_VALUE) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("ERROR: Unable to get the new stdout handle: %s"), getLastErrorText());
           return 1;
        }
        setConsoleStdoutHandle(hStdout);

        if (wrapperData->ntHideWrapperConsole) {
            /* A console needed to be allocated for the process but it should be hidden. */

            /* Generate a unique time for the console so we can look for it below. */
            _sntprintf(titleBuffer, 80, TEXT("Wrapper Console Id %d-%d (Do not close)"), wrapperData->wrapperPID, rand());
#ifdef _DEBUG
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Wrapper console title: %s"), titleBuffer);
#endif

            SetConsoleTitle(titleBuffer);

            wrapperData->wrapperConsoleHide = TRUE;
            if (wrapperData->wrapperConsoleHandle = findConsoleWindow(titleBuffer)) {
                wrapperData->wrapperConsoleVisible = TRUE;
                if (wrapperData->isDebugging) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Found console window."));
                }

                /* Attempt to hide the console window here once so it goes away as quickly as possible.
                 *  This may not succeed yet however.  If the system is still coming up. */
                wrapperCheckConsoleWindows();
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Failed to locate the console window so it can be hidden."));
            }
        }
    }

    /* Attempt to set the console title if it exists and is accessable. */
    if (wrapperData->consoleTitle) {
        if (wrapperData->isConsole || (wrapperData->ntServiceInteractive && !wrapperData->ntHideWrapperConsole)) {
            /* The console should be visible. */
            if (!SetConsoleTitle(wrapperData->consoleTitle)) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                    TEXT("Attempt to set the console title failed: %s"), getLastErrorText());
            }
        }
    }

    /* Set the handler to trap console signals.  This must be done after the console
     *  is created or it will not be applied to that console. */
    SetConsoleCtrlHandler((PHANDLER_ROUTINE)wrapperConsoleHandler, TRUE);

    if (wrapperData->useSystemTime) {
        /* We are going to be using system time so there is no reason to start up a timer thread. */
        timerThreadHandle = NULL;
        timerThreadId = 0;
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
        javaIOThreadHandle = NULL;
        javaIOThreadId = 0;
    }
    
    if (wrapperData->isPageFaultOutputEnabled) {
        wrapperInitializeProfileCounters();
    }

    return 0;
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
    if (wrapperData->isSleepOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
            TEXT("    Sleep: sleep %dms"), ms);
    }

    Sleep(ms);

    if (wrapperData->isSleepOutputEnabled) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Sleep: awake"));
    }

    return FALSE;
}

/**
 * Detaches the Java process so the Wrapper will if effect forget about it.
 */
void wrapperDetachJava() {
    wrapperSetJavaState(WRAPPER_JSTATE_DOWN_CLEAN, 0, -1);
}


/**
 * Reports the status of the wrapper to the service manager
 * Possible status values:
 *   WRAPPER_WSTATE_STARTING
 *   WRAPPER_WSTATE_STARTED
 *   WRAPPER_WSTATE_PAUSING
 *   WRAPPER_WSTATE_PAUSED
 *   WRAPPER_WSTATE_RESUMING
 *   WRAPPER_WSTATE_STOPPING
 *   WRAPPER_WSTATE_STOPPED
 */
void wrapperReportStatus(int useLoggerQueue, int status, int errorCode, int waitHint) {
    int natState;
    TCHAR *natStateName;
    static DWORD dwCheckPoint = 1;
    BOOL bResult = TRUE;

    /*
    log_printf_queue(useLoggerQueue, WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
        "wrapperReportStatus(%d, %d, %d, %d)", useLoggerQueue, status, errorCode, waitHint);
    */

    switch (status) {
    case WRAPPER_WSTATE_STARTING:
        natState = SERVICE_START_PENDING;
        natStateName = TEXT("SERVICE_START_PENDING");
        break;
    case WRAPPER_WSTATE_STARTED:
        natState = SERVICE_RUNNING;
        natStateName = TEXT("SERVICE_RUNNING");
        break;
    case WRAPPER_WSTATE_PAUSING:
        natState = SERVICE_PAUSE_PENDING;
        natStateName = TEXT("SERVICE_PAUSE_PENDING");
        break;
    case WRAPPER_WSTATE_PAUSED:
        natState = SERVICE_PAUSED;
        natStateName = TEXT("SERVICE_PAUSED");
        break;
    case WRAPPER_WSTATE_RESUMING:
        natState = SERVICE_CONTINUE_PENDING;
        natStateName = TEXT("SERVICE_CONTINUE_PENDING");
        break;
    case WRAPPER_WSTATE_STOPPING:
        natState = SERVICE_STOP_PENDING;
        natStateName = TEXT("SERVICE_STOP_PENDING");
        break;
    case WRAPPER_WSTATE_STOPPED:
        natState = SERVICE_STOPPED;
        natStateName = TEXT("SERVICE_STOPPED");
        break;
    default:
        log_printf_queue(useLoggerQueue, WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unknown status: %d"), status);
        return;
    }

    if (!wrapperData->isConsole) {
        ssStatus.dwControlsAccepted = 0;
        if (natState != SERVICE_START_PENDING) {
            ssStatus.dwControlsAccepted |= SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
            if (wrapperData->pausable) {
                ssStatus.dwControlsAccepted |= SERVICE_ACCEPT_PAUSE_CONTINUE;
            }
        }
        if (isWindowsNT4_0OrEarlier()) {
            /* Old Windows - Does not support power events. */
        } else {
            /* Supports power events. */
            ssStatus.dwControlsAccepted |= SERVICE_ACCEPT_POWEREVENT;
        }
        /*
        if (wrapperData->isDebugging) {
            log_printf_queue(useLoggerQueue, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG,
                "  Service %s accepting STOP=%s, SHUTDOWN=%s, PAUSE/CONTINUE=%s, POWEREVENT=%s",
                natStateName,
                (ssStatus.dwControlsAccepted & SERVICE_ACCEPT_STOP ? "True" : "False"),
                (ssStatus.dwControlsAccepted & SERVICE_ACCEPT_SHUTDOWN ? "True" : "False"),
                (ssStatus.dwControlsAccepted & SERVICE_ACCEPT_PAUSE_CONTINUE ? "True" : "False"),
                (ssStatus.dwControlsAccepted & SERVICE_ACCEPT_POWEREVENT ? "True" : "False"));
        }
        */

        ssStatus.dwCurrentState = natState;
        if (errorCode == 0) {
            ssStatus.dwWin32ExitCode = NO_ERROR;
            ssStatus.dwServiceSpecificExitCode = 0;
        } else {
            ssStatus.dwWin32ExitCode = ERROR_SERVICE_SPECIFIC_ERROR;
            ssStatus.dwServiceSpecificExitCode = errorCode;
        }
        ssStatus.dwWaitHint = waitHint;

        if ((natState == SERVICE_RUNNING) || (natState == SERVICE_STOPPED) || (natState == SERVICE_PAUSED)) {
            ssStatus.dwCheckPoint = 0;
        } else {
            ssStatus.dwCheckPoint = dwCheckPoint++;
        }

        if (wrapperData->isStateOutputEnabled) {
            log_printf_queue(useLoggerQueue, WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                TEXT("calling SetServiceStatus with status=%s, waitHint=%d, checkPoint=%u, errorCode=%d"),
                natStateName, waitHint, dwCheckPoint, errorCode);
        }

        if (!(bResult = SetServiceStatus(sshStatusHandle, &ssStatus))) {
            log_printf_queue(useLoggerQueue, WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("SetServiceStatus failed"));
        }
    }
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
    DWORD currentBlockAvail;

    /* See how many characters are available in the pipe so we can say how much to read. */
    if (!PeekNamedPipe(wrapperChildStdoutRd, NULL, 0, NULL, &currentBlockAvail, NULL)) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
            TEXT("Failed to peek at output from the JVM: %s"), getLastErrorText());
        return TRUE;
    }

#ifdef DEBUG_CHILD_OUTPUT
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("Peeked %d chars from pipe."), currentBlockAvail);
#endif

    if (currentBlockAvail > 0) {
        /* Attempt to read in an additional CHILD_BLOCK_SIZE characters. */
        if (!ReadFile(wrapperChildStdoutRd, blockBuffer, blockSize, readCount, NULL)) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                    TEXT("Failed to read output from the JVM: %s"), getLastErrorText());
            return TRUE;
        }
#ifdef DEBUG_CHILD_OUTPUT
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("Read %d chars from pipe."), *readCount);
#endif
    } else {
        *readCount = 0;
    }

    return FALSE;
}

/**
 * Checks on the status of the JVM Process.
 * Returns WRAPPER_PROCESS_UP or WRAPPER_PROCESS_DOWN
 */
int wrapperGetProcessStatus(TICKS nowTicks, int sigChild) {
    int res;
    DWORD exitCode;
    TCHAR *exName;

    switch (WaitForSingleObject(wrapperData->javaProcess, 0)) {
    case WAIT_ABANDONED:
    case WAIT_OBJECT_0:
        res = WRAPPER_PROCESS_DOWN;

        /* Get the exit code of the process. */
        if (!GetExitCodeProcess(wrapperData->javaProcess, &exitCode)) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                TEXT("Critical error: unable to obtain the exit code of the JVM process: %s"), getLastErrorText());
            appExit(1);
        }

        if (exitCode == STILL_ACTIVE) {
            /* Should never happen, but check for it. */
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                TEXT("The JVM returned JVM exit code was STILL_ACTIVE.") );
        }

        /* If the JVM crashed then GetExitCodeProcess could have returned an uncaught exception. */
        exName = getExceptionName(exitCode);
        if (exName != NULL) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("The JVM process terminated due to an uncaught exception: %s (0x%08x)"), exName, exitCode);

            /* Reset the exit code as the exeption value will confuse users. */
            exitCode = 1;
        }

        wrapperJVMProcessExited(nowTicks, exitCode);
        break;

    case WAIT_TIMEOUT:
        res = WRAPPER_PROCESS_UP;
        break;

    default:
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
            TEXT("Critical error: wait for JVM process failed"));
        appExit(1);
    }

    return res;
}

/**
 * Launches a JVM process and store it internally
 */
void wrapperExecute() {
    SECURITY_ATTRIBUTES process_attributes;
    STARTUPINFO startup_info;
    PROCESS_INFORMATION process_info;
    int ret;
    /* Do not show another console for the new process */
    /*int processflags=CREATE_NEW_PROCESS_GROUP | DETACHED_PROCESS; */

    /* Create a new process group as part of this console so that signals can */
    /*  be sent to the JVM. */
    DWORD processflags=CREATE_NEW_PROCESS_GROUP;

    /* Do not show another console for the new process, but show its output in the current console. */
    /*int processflags=CREATE_NEW_PROCESS_GROUP; */

    /* Show a console for the new process */
    /*int processflags=CREATE_NEW_PROCESS_GROUP | CREATE_NEW_CONSOLE; */

    TCHAR *commandline=NULL;
    TCHAR *environment=NULL;
    TCHAR *binparam=NULL;
    int char_block_size = 8196;
    int string_size = 0;
    int temp_int = 0;
    TCHAR szPath[_MAX_PATH];
    DWORD usedLen;
    TCHAR *c;
    TCHAR titleBuffer[80];
    int hideConsole;
    int old_umask;

    FILE *pid_fp = NULL;

    /* Reset the exit code when we launch a new JVM. */
    wrapperData->exitCode = 0;

    /* Add the priority class of the new process to the processflags */
    processflags = processflags | wrapperData->ntServicePriorityClass;

    /* Setup the command line */
    commandline = wrapperData->jvmCommand;
    if (wrapperData->commandLogLevel != LEVEL_NONE) {
        log_printf(WRAPPER_SOURCE_WRAPPER, wrapperData->commandLogLevel,
            TEXT("Command: %s"), commandline);

        if (wrapperData->environmentClasspath) {
            log_printf(WRAPPER_SOURCE_WRAPPER, wrapperData->commandLogLevel,
                TEXT("Classpath in Environment : %s"), wrapperData->classpath);
        }
    }

    if (wrapperData->environmentClasspath) {
        setEnv(TEXT("CLASSPATH"), wrapperData->classpath, ENV_SOURCE_WRAPPER);
    }

    /* Setup environment. Use parent's for now */
    environment = NULL;

    /* Initialize a SECURITY_ATTRIBUTES for the process attributes of the new process. */
    process_attributes.nLength = sizeof(SECURITY_ATTRIBUTES);
    process_attributes.lpSecurityDescriptor = NULL;
    process_attributes.bInheritHandle = TRUE;

    /* Generate a unique time for the console so we can look for it below. */
    _sntprintf(titleBuffer, 80, TEXT("Wrapper Controlled JVM Console Id %d-%d (Do not close)"), wrapperData->wrapperPID, rand());

    /* Initialize a STARTUPINFO structure to use for the new process. */
    startup_info.cb=sizeof(STARTUPINFO);
    startup_info.lpReserved=NULL;
    startup_info.lpDesktop=NULL;
    startup_info.lpTitle=titleBuffer;
    startup_info.dwX=0;
    startup_info.dwY=0;
    startup_info.dwXSize=0;
    startup_info.dwYSize=0;
    startup_info.dwXCountChars=0;
    startup_info.dwYCountChars=0;
    startup_info.dwFillAttribute=0;

    /* Set the default flags which will not hide any windows opened by the JVM. */
    /* Using Show Window and SW_HIDE seems to make it impossible to show any windows when the 32-bit version runs as a service.
    startup_info.dwFlags=STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;
    startup_info.wShowWindow=SW_HIDE;
    */
    startup_info.dwFlags=STARTF_USESTDHANDLES;
    startup_info.wShowWindow=0;

    hideConsole = FALSE;
    if (wrapperData->isConsole) {
        /* We are running as a console so no special console handling needs to be done. */
    } else {
        /* Running as a service. */
        if (wrapperData->ntAllocConsole) {
            /* A console was allocated when the service was started so the JVM will not create
             *  its own. */
            if (wrapperData->wrapperConsoleHandle) {
                /* The console exists but is currently hidden. */
                if (!wrapperData->ntHideJVMConsole) {
                    /* In order to support older JVMs we need to show the console when the
                     *  JVM is launched.  We need to remember to hide it below. */
                    showConsoleWindow(wrapperData->wrapperConsoleHandle, TEXT("Wrapper"));
                    wrapperData->wrapperConsoleVisible = TRUE;
                    wrapperData->wrapperConsoleHide = FALSE;
                    hideConsole = TRUE;
                }
            }
        } else {
            /* A console does not yet exist so the JVM will create and display one itself. */
            if (wrapperData->ntHideJVMConsole) {
                /* The console that the JVM creates should be surpressed and never shown.
                 *  JVMs of version 1.4.0 and above will still display a GUI.  But older JVMs
                 *  will not. */
                startup_info.dwFlags=STARTF_USESHOWWINDOW | STARTF_USESTDHANDLES;
                startup_info.wShowWindow=SW_HIDE;
            } else {
                /* The new JVM console should be allowed to be displayed.  But we need to
                 *  remember to hide it below. */
                hideConsole = TRUE;
            }
        }
    }

    startup_info.cbReserved2 = 0;
    startup_info.lpReserved2 = NULL;
    startup_info.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    startup_info.hStdOutput = wrapperChildStdoutWr;
    startup_info.hStdError = wrapperChildStdoutWr;

    /* Initialize a PROCESS_INFORMATION structure to use for the new process */
    process_info.hProcess = NULL;
    process_info.hThread = NULL;
    process_info.dwProcessId = 0;
    process_info.dwThreadId = 0;

    /* Need the directory that this program exists in.  Not the current directory. */
    /*    Note, the current directory when run as an NT service is the windows system directory. */
    /* Get the full path and filename of this program */
    usedLen = GetModuleFileName(NULL, szPath, _MAX_PATH);
    if (usedLen == 0) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to launch %s -%s"),
                     wrapperData->serviceDisplayName, getLastErrorText());
        wrapperData->javaProcess = NULL;
        return;
    } else if ((usedLen == _MAX_PATH) || (getLastError() == ERROR_INSUFFICIENT_BUFFER)) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to launch %s -%s"),
                     wrapperData->serviceDisplayName, TEXT("Path to Wrapper binary too long."));
        wrapperData->javaProcess = NULL;
        return;
    }
    c = _tcsrchr(szPath, TEXT('\\'));
    if (c == NULL) {
        szPath[0] = TEXT('\0');
    } else {
        c[1] = TEXT('\0'); /* terminate after the slash */
    }

    /* Make sure the log file is closed before the Java process is created.  Failure to do
     *  so will give the Java process a copy of the open file.  This means that this process
     *  will not be able to rename the file even after closing it because it will still be
     *  open in the Java process.  Also set the auto close flag to make sure that other
     *  threads do not reopen the log file as the new process is being created. */
    setLogfileAutoClose(TRUE);
    closeLogfile();

    /* Set the umask of the JVM */
    old_umask = _umask(wrapperData->javaUmask);

    /* Create the new process */
    ret=CreateProcess(NULL,
                      commandline,    /* the command line to start */
                      NULL,           /* process security attributes */
                      NULL,           /* primary thread security attributes */
                      TRUE,           /* handles are inherited */
                      processflags,   /* we specify new process group */
                      environment,    /* use parent's environment */
                      NULL,           /* use the Wrapper's current working directory */
                      &startup_info,  /* STARTUPINFO pointer */
                      &process_info); /* PROCESS_INFORMATION pointer */

    /* Restore the umask. */
    _umask(old_umask);

    /* As soon as the new process is created, restore the auto close flag. */
    setLogfileAutoClose(wrapperData->logfileInactivityTimeout <= 0);

    /* Check if virtual machine started */
    if (ret==FALSE) {
        int err=GetLastError();
        /* Make sure the process was launched correctly. */
        if (err!=NO_ERROR) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                TEXT("Unable to execute Java command.  %s"), getLastErrorText());
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("    %s"), commandline);
            wrapperData->javaProcess = NULL;

            if ((err == ERROR_FILE_NOT_FOUND) || (err == ERROR_PATH_NOT_FOUND)) {
                if (wrapperData->isAdviserEnabled) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE, TEXT("") );
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                        TEXT("--------------------------------------------------------------------") );
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                        TEXT("Advice:" ));
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                        TEXT("Usually when the Wrapper fails to start the JVM process, it is\nbecause of a problem with the value of the configured Java command.\nCurrently:" ));
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                        TEXT("wrapper.java.command=%s"), getStringProperty(properties, TEXT("wrapper.java.command"), TEXT("java")));
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                        TEXT("Please make sure that the PATH or any other referenced environment\nvariables are correctly defined for the current environment." ));
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                        TEXT("--------------------------------------------------------------------") );
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE, TEXT("") );
                }
            } else if (err == ERROR_ACCESS_DENIED) {
                if (wrapperData->isAdviserEnabled) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE, TEXT("") );
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                        TEXT("--------------------------------------------------------------------") );
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE, TEXT(
                        "Advice:" ));
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE, TEXT(
                        "Access denied errors when attempting to launch the Java process are\nusually caused by strict access permissions assigned to the\ndirectory in which Java is installed." ));
                    if (!wrapperData->isConsole) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE, TEXT(
                            "Unless you have configured the Wrapper to run as a different user\nwith wrapper.ntservice.account property, the Wrapper and its JVM\nwill be as the SYSTEM user by default when run as a service." ));
                    }
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE,
                        TEXT("--------------------------------------------------------------------") );
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE, TEXT("") );
                }
            }

            return;
        }
    }

    /* Now check if we have a process handle again for the Swedish WinNT bug */
    if (process_info.hProcess == NULL) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("can not execute \"%s\""), commandline);
        wrapperData->javaProcess = NULL;
        return;
    }

    if (hideConsole) {
        /* Now that the JVM has been launched we need to hide the console that it
         *  is using. */
        if (wrapperData->wrapperConsoleHandle) {
            /* The wrapper's console needs to be hidden. */
            wrapperData->wrapperConsoleHide = TRUE;
            wrapperCheckConsoleWindows();
        } else {
            /* We need to locate the console that was created by the JVM on launch
             *  and hide it. */
            wrapperData->jvmConsoleHandle = findConsoleWindow(titleBuffer);
            wrapperData->jvmConsoleVisible = TRUE;
            wrapperCheckConsoleWindows();
        }
    }

    if (wrapperData->isDebugging) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("JVM started (PID=%d)"), process_info.dwProcessId);
    }

    /* We keep a reference to the process handle, but need to close the thread handle. */
    wrapperData->javaProcess = process_info.hProcess;
    wrapperData->javaPID = process_info.dwProcessId;
    CloseHandle(process_info.hThread);

    /* If a java pid filename is specified then write the pid of the java process. */
    if (wrapperData->javaPidFilename) {
        if (writePidFile(wrapperData->javaPidFilename, wrapperData->javaPID, wrapperData->javaPidFileUmask)) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                TEXT("Unable to write the Java PID file: %s"), wrapperData->javaPidFilename);
        }
    }

    /* If a java id filename is specified then write the id of the java process. */
    if (wrapperData->javaIdFilename) {
        if (writePidFile(wrapperData->javaIdFilename, wrapperData->jvmRestarts, wrapperData->javaIdFileUmask)) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                TEXT("Unable to write the Java Id file: %s"), wrapperData->javaIdFilename);
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
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
        TEXT("Wrapper memory: PageFaultcount, WorkingSetSize (Peak), QuotaPagePoolUsage (Peak), QuotaNonPagedPoolUsage (Peak), PageFileUsage (Peak)  Java memory: PageFaultcount, WorkingSetSize (Peak), QuotaPagePoolUsage (Peak), QuotaNonPagedPoolUsage (Peak), PageFileUsage (Peak)  System memory: MemoryLoad, Available/PhysicalSize (%%), Available/PageFileSize (%%), Available/VirtualSize (%%), ExtendedVirtualSize"));
}

/**
 * Outputs a log entry at regular intervals to track the memory usage of the
 *  Wrapper and its JVM.
 */
void wrapperDumpMemory() {
    PROCESS_MEMORY_COUNTERS wCounters;
    PROCESS_MEMORY_COUNTERS jCounters;
    MEMORYSTATUSEX statex;

    if (OptionalGetProcessMemoryInfo) {
        /* Start with the Wrapper process. */
        if (OptionalGetProcessMemoryInfo(wrapperData->wrapperProcess, &wCounters, sizeof(wCounters)) == 0) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("Call to GetProcessMemoryInfo failed for Wrapper process %08x: %s"),
                wrapperData->wrapperPID, getLastErrorText());
            return;
        }

        if (wrapperData->javaProcess != NULL) {
            /* Next the Java process. */
            if (OptionalGetProcessMemoryInfo(wrapperData->javaProcess, &jCounters, sizeof(jCounters)) == 0) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                    TEXT("Call to GetProcessMemoryInfo failed for Java process %08x: %s"),
                    wrapperData->javaPID, getLastErrorText());
                return;
            }
        } else {
            memset(&jCounters, 0, sizeof(jCounters));
        }

        statex.dwLength = sizeof(statex);
        GlobalMemoryStatusEx(&statex);

        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
            TEXT("Wrapper memory: %lu, %lu (%lu), %lu (%lu), %lu (%lu), %lu (%lu)  Java memory: %lu, %lu (%lu), %lu (%lu), %lu (%lu), %lu (%lu)  System memory: %lu%%, %I64u/%I64u (%u%%), %I64u/%I64u (%u%%), %I64u/%I64u (%u%%), %I64u"),
            wCounters.PageFaultCount,
            wCounters.WorkingSetSize, wCounters.PeakWorkingSetSize,
            wCounters.QuotaPagedPoolUsage, wCounters.QuotaPeakPagedPoolUsage,
            wCounters.QuotaNonPagedPoolUsage, wCounters.QuotaPeakNonPagedPoolUsage,
            wCounters.PagefileUsage, wCounters.PeakPagefileUsage,
            jCounters.PageFaultCount,
            jCounters.WorkingSetSize, jCounters.PeakWorkingSetSize,
            jCounters.QuotaPagedPoolUsage, jCounters.QuotaPeakPagedPoolUsage,
            jCounters.QuotaNonPagedPoolUsage, jCounters.QuotaPeakNonPagedPoolUsage,
            jCounters.PagefileUsage, jCounters.PeakPagefileUsage,
            statex.dwMemoryLoad,
            statex.ullAvailPhys,
            statex.ullTotalPhys,
            (int)(100 * statex.ullAvailPhys / statex.ullTotalPhys),
            statex.ullAvailPageFile,
            statex.ullTotalPageFile,
            (int)(100 * statex.ullAvailPageFile / statex.ullTotalPageFile),
            statex.ullAvailVirtual,
            statex.ullTotalVirtual,
            (int)(100 * statex.ullAvailVirtual / statex.ullTotalVirtual),
            statex.ullAvailExtendedVirtual);
    }
}

DWORD filetimeToMS(FILETIME* filetime) {
    LARGE_INTEGER li;

    memcpy(&li, filetime, sizeof(li));
    li.QuadPart /= 10000;

    return li.LowPart;
}

/**
 * Outputs a log entry at regular intervals to track the CPU usage over each
 *  interval for the Wrapper and its JVM.
 *
 * In order to make sense of the timing values, it is also necessary to see how
 *  far the system performance counter has progressed.  By carefully comparing
 *  these values, it is possible to very accurately calculate the CPU usage over
 *  any period of time.
 */
LONGLONG lastPerformanceCount = 0;
LONGLONG lastWrapperKernelTime = 0;
LONGLONG lastWrapperUserTime = 0;
LONGLONG lastJavaKernelTime = 0;
LONGLONG lastJavaUserTime = 0;
LONGLONG lastIdleKernelTime = 0;
LONGLONG lastIdleUserTime = 0;
void wrapperDumpCPUUsage() {
    LARGE_INTEGER count;
    LARGE_INTEGER frequency;
    LARGE_INTEGER li;
    LONGLONG performanceCount;

    FILETIME creationTime;
    FILETIME exitTime;
    FILETIME wKernelTime;
    FILETIME wUserTime;
    FILETIME jKernelTime;
    FILETIME jUserTime;

    DWORD wKernelTimeMs; /* Will overflow in 49 days of usage. */
    DWORD wUserTimeMs;
    DWORD wTimeMs;
    DWORD jKernelTimeMs;
    DWORD jUserTimeMs;
    DWORD jTimeMs;

    double age;
    double wKernelPercent;
    double wUserPercent;
    double wPercent;
    double jKernelPercent;
    double jUserPercent;
    double jPercent;

    if (OptionalGetProcessTimes) {
        if (!QueryPerformanceCounter(&count)) {
            /* no high-resolution performance counter support. */
            return;
        }
        if (!QueryPerformanceFrequency(&frequency)) {
        }

        performanceCount = count.QuadPart;

        /* Start with the Wrapper process. */
        if (!OptionalGetProcessTimes(wrapperData->wrapperProcess, &creationTime, &exitTime, &wKernelTime, &wUserTime)) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("Call to GetProcessTimes failed for Wrapper process %08x: %s"),
                wrapperData->wrapperPID, getLastErrorText());
            return;
        }

        if (wrapperData->javaProcess != NULL) {
            /* Next the Java process. */
            if (!OptionalGetProcessTimes(wrapperData->javaProcess, &creationTime, &exitTime, &jKernelTime, &jUserTime)) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                    TEXT("Call to GetProcessTimes failed for Java process %08x: %s"),
                    wrapperData->javaPID, getLastErrorText());
                return;
            }
        } else {
            memset(&jKernelTime, 0, sizeof(jKernelTime));
            memset(&jUserTime, 0, sizeof(jUserTime));
            lastJavaKernelTime = 0;
            lastJavaUserTime = 0;
        }


        /* Convert the times to ms. */
        wKernelTimeMs = filetimeToMS(&wKernelTime);
        wUserTimeMs = filetimeToMS(&wUserTime);
        wTimeMs = wKernelTimeMs + wUserTimeMs;
        jKernelTimeMs = filetimeToMS(&jKernelTime);
        jUserTimeMs = filetimeToMS(&jUserTime);
        jTimeMs = jKernelTimeMs + jUserTimeMs;

        /* Calculate the number of seconds since the last call. */
        age = (double)(performanceCount - lastPerformanceCount) / frequency.QuadPart;

        /* Calculate usage percentages. */
        memcpy(&li, &wKernelTime, sizeof(li));
        wKernelPercent = 100.0 * ((li.QuadPart - lastWrapperKernelTime) / 10000000.0) / age;
        lastWrapperKernelTime = li.QuadPart;

        memcpy(&li, &wUserTime, sizeof(li));
        wUserPercent = 100.0 * ((li.QuadPart - lastWrapperUserTime) / 10000000.0) / age;
        lastWrapperUserTime = li.QuadPart;

        wPercent = wKernelPercent + wUserPercent;

        memcpy(&li, &jKernelTime, sizeof(li));
        jKernelPercent = 100.0 * ((li.QuadPart - lastJavaKernelTime) / 10000000.0) / age;
        lastJavaKernelTime = li.QuadPart;

        memcpy(&li, &jUserTime, sizeof(li));
        jUserPercent = 100.0 * ((li.QuadPart - lastJavaUserTime) / 10000000.0) / age;
        lastJavaUserTime = li.QuadPart;

        jPercent = jKernelPercent + jUserPercent;

        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
            TEXT("Wrapper CPU: kernel %ldms (%5.2f%%), user %ldms (%5.2f%%), total %ldms (%5.2f%%)  Java CPU: kernel %ldms (%5.2f%%), user %ldms (%5.2f%%), total %ldms (%5.2f%%)"),
            wKernelTimeMs, wKernelPercent, wUserTimeMs, wUserPercent, wTimeMs, wPercent,
            jKernelTimeMs, jKernelPercent, jUserTimeMs, jUserPercent, jTimeMs, jPercent);

        lastPerformanceCount = performanceCount;
    }
}
    
void wrapperInitializeProfileCounters() {
    PDH_STATUS pdhStatus;
    FARPROC pdhAddUnlocalizedCounter;
    BOOL couldLoad;
    HMODULE dbgHelpDll = GetModuleHandle(TEXT("Pdh.dll"));


    if( dbgHelpDll == NULL) {
        couldLoad = FALSE;
    } else {
        if (isVista()) {
#ifdef UNICODE
            pdhAddUnlocalizedCounter = GetProcAddress(dbgHelpDll, "PdhAddEnglishCounterW");
#else
            pdhAddUnlocalizedCounter = GetProcAddress(dbgHelpDll, "PdhAddEnglishCounterA");
#endif
        } else {
#ifdef UNICODE
            pdhAddUnlocalizedCounter = GetProcAddress(dbgHelpDll, "PdhAddCounterW");
#else
            pdhAddUnlocalizedCounter = GetProcAddress(dbgHelpDll, "PdhAddCounterA");
#endif
        }
        if(pdhAddUnlocalizedCounter == NULL) {
            couldLoad = FALSE;
        } else {
            couldLoad = TRUE;
        }
    }
    /* We want to set up system profile monitoring to keep track of the state of the system. */
    pdhStatus = PdhOpenQuery(NULL, 0, &pdhQuery);
    if (pdhStatus != ERROR_SUCCESS) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
            TEXT("Failed to initialize profiling: 0x%x"), pdhStatus);
        pdhQuery = NULL;
    } else {
        pdhStatus = (PDH_STATUS)pdhAddUnlocalizedCounter(pdhQuery, TEXT("\\PhysicalDisk(_Total)\\Avg. Disk Queue Length"), 0, &pdhCounterPhysicalDiskAvgQueueLen);
        if (pdhStatus != ERROR_SUCCESS) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                TEXT("Failed to initialize profiling counter %d: 0x%x"), 1, pdhStatus);
        }
        
        pdhStatus = (PDH_STATUS)pdhAddUnlocalizedCounter(pdhQuery, TEXT("\\PhysicalDisk(_Total)\\Avg. Disk Write Queue Length"), 0, &pdhCounterPhysicalDiskAvgWriteQueueLen);
        if (pdhStatus != ERROR_SUCCESS) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                TEXT("Failed to initialize profiling counter %d: 0x%x"), 2, pdhStatus);
        }
        
        pdhStatus = (PDH_STATUS)pdhAddUnlocalizedCounter(pdhQuery, TEXT("\\PhysicalDisk(_Total)\\Avg. Disk Read Queue Length"), 0, &pdhCounterPhysicalDiskAvgReadQueueLen);
        if (pdhStatus != ERROR_SUCCESS) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                TEXT("Failed to initialize profiling counter %d: 0x%x"), 3, pdhStatus);
        }
        
        pdhStatus = (PDH_STATUS)pdhAddUnlocalizedCounter(pdhQuery, TEXT("\\Memory\\Page Faults/sec"), 0, &pdhCounterMemoryPageFaultsPSec);
        if (pdhStatus != ERROR_SUCCESS) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                TEXT("Failed to initialize profiling counter %d: 0x%x"), 4, pdhStatus);
        }
        
        pdhStatus = (PDH_STATUS)pdhAddUnlocalizedCounter(pdhQuery, TEXT("\\Memory\\Transition Faults/sec"), 0, &pdhCounterMemoryTransitionFaultsPSec);
        if (pdhStatus != ERROR_SUCCESS) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                TEXT("Failed to initialize profiling counter %d: 0x%x"), 5, pdhStatus);
        }
        
        pdhStatus = (PDH_STATUS)pdhAddUnlocalizedCounter(pdhQuery, TEXT("\\Process(wrapper)\\Page Faults/sec"), 0, &pdhCounterProcessWrapperPageFaultsPSec);
        if (pdhStatus != ERROR_SUCCESS) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                TEXT("Failed to initialize profiling counter %d: 0x%x"), 6, pdhStatus);
        }
        
        pdhStatus = (PDH_STATUS)pdhAddUnlocalizedCounter(pdhQuery, TEXT("\\Process(java)\\Page Faults/sec"), 0, &pdhCounterProcessJavaPageFaultsPSec);
        if (pdhStatus != ERROR_SUCCESS) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
                TEXT("Failed to initialize profiling counter %d: 0x%x"), 7, pdhStatus);
        }
        if (couldLoad && dbgHelpDll != NULL) {
        FreeLibrary(dbgHelpDll);
        }
        /* This is the first call, since for some equations (e.g. for average) 2 values need to be polled */
        PdhCollectQueryData(pdhQuery);
        /* PdhGetCounterInfo to get info about the counters like scale, etc. */
    }
}

void wrapperDumpPageFaultUsage() {
    PDH_STATUS pdhStatus;
    DWORD counterType;
    PDH_FMT_COUNTERVALUE counterValue;
    double diskQueueLen = 0;
    double diskQueueWLen = 0;
    double diskQueueRLen = 0;
    double pageFaults = 0;
    double transitionPageFaults = 0;
    double wrapperPageFaults = 0;
    double javaPageFaults = 0;
    
    if (pdhQuery == NULL) {
        return;
    }
    
    pdhStatus = PdhCollectQueryData(pdhQuery);
    if (pdhStatus == ERROR_SUCCESS) {
        pdhStatus = PdhGetFormattedCounterValue(pdhCounterPhysicalDiskAvgQueueLen, PDH_FMT_DOUBLE, &counterType, &counterValue);
        if (pdhStatus == ERROR_SUCCESS) {
            diskQueueLen = counterValue.doubleValue;
            /*log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("\\PhysicalDisk(_Total)\\Avg. Disk Queue Length : %d %10.5f"), counterValue.CStatus, counterValue.doubleValue);*/
        }
        pdhStatus = PdhGetFormattedCounterValue(pdhCounterPhysicalDiskAvgWriteQueueLen, PDH_FMT_DOUBLE, &counterType, &counterValue);
        if (pdhStatus == ERROR_SUCCESS) {
            diskQueueWLen = counterValue.doubleValue;
            /*log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("\\PhysicalDisk(_Total)\\Avg. Disk Write Queue Length : %d %10.5f"), counterValue.CStatus, counterValue.doubleValue);*/
        }
        pdhStatus = PdhGetFormattedCounterValue(pdhCounterPhysicalDiskAvgReadQueueLen, PDH_FMT_DOUBLE, &counterType, &counterValue);
        if (pdhStatus == ERROR_SUCCESS) {
            diskQueueRLen = counterValue.doubleValue;
            /*log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("\\PhysicalDisk(_Total)\\Avg. Disk Read Queue Length : %d %10.5f"), counterValue.CStatus, counterValue.doubleValue);*/
        }

        pdhStatus = PdhGetFormattedCounterValue(pdhCounterMemoryPageFaultsPSec, PDH_FMT_DOUBLE, &counterType, &counterValue);
        if (pdhStatus == ERROR_SUCCESS) {
            pageFaults = counterValue.doubleValue;
            /*log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("\\Memory\\Page Faults/sec : %d %10.5f"), counterValue.CStatus, counterValue.doubleValue);*/
        }

        pdhStatus = PdhGetFormattedCounterValue(pdhCounterMemoryTransitionFaultsPSec, PDH_FMT_DOUBLE, &counterType, &counterValue);
        if (pdhStatus == ERROR_SUCCESS) {
            transitionPageFaults = counterValue.doubleValue;
            /*log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("\\Memory\\Transition Faults/sec : %d %10.5f"), counterValue.CStatus, counterValue.doubleValue);*/
        }
        
        pdhStatus = PdhGetFormattedCounterValue(pdhCounterProcessWrapperPageFaultsPSec, PDH_FMT_DOUBLE, &counterType, &counterValue);
        if (pdhStatus == ERROR_SUCCESS) {
            wrapperPageFaults = counterValue.doubleValue;
            /*log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("\\Process(wrapper)\\Page Faults/sec : %d %10.5f"), counterValue.CStatus, counterValue.doubleValue);*/
        }
        
        pdhStatus = PdhGetFormattedCounterValue(pdhCounterProcessJavaPageFaultsPSec, PDH_FMT_DOUBLE, &counterType, &counterValue);
        if (pdhStatus == ERROR_SUCCESS) {
            javaPageFaults = counterValue.doubleValue;
            /*log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("\\Process(java)\\Page Faults/sec : %d %10.5f"), counterValue.CStatus, counterValue.doubleValue);*/
        }
        
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("Page Faults (Total:%8.2f%8.2f:%8.2f Wrapper:%7.2f (%7.2f%%) Java:%7.2f (%7.2f%%))  Queue Len (Total:%7.2f Read:%7.2f Write:%7.2f)"),
            pageFaults, transitionPageFaults, pageFaults - transitionPageFaults,
            wrapperPageFaults, (pageFaults > 0 ? 100 * wrapperPageFaults / pageFaults : 0),
            javaPageFaults, (pageFaults > 0 ? 100 * javaPageFaults / pageFaults : 0),
            diskQueueLen, diskQueueRLen, diskQueueWLen);
    } else {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN,
            TEXT("Failed to collect profile data: 0x%x"), pdhStatus);
    } 
}

void disposeProfileCounters() {
    if (pdhQuery != NULL) {
        PdhCloseQuery(pdhQuery);
        pdhQuery = NULL;
    }
}

/******************************************************************************
 * NT Service Methods
 *****************************************************************************/

/**
 * This function goes through and checks flags for each of several signals to see if they
 *  have been fired since the last time this function was called.  This is the only thread
 *  which will ever clear these flags, but they can be set by other threads within the
 *  signal handlers at ANY time.  So only check the value of each flag once and reset them
 *  immediately to decrease the chance of missing duplicate signals.
 */
void wrapperMaintainControlCodes() {
    /* Allow for a large integer + \0 */
    TCHAR buffer[11];
    int ctrlCodeLast;
    int quit = FALSE;
    int halt = FALSE;

    /* CTRL_C_EVENT */
    if (wrapperData->ctrlEventCTRLCTrapped) {
        wrapperData->ctrlEventCTRLCTrapped = FALSE;

        /*  Always quit.  If the user has pressed CTRL-C previously then we want to force
         *   an immediate shutdown. */
        if (ctrlCTrapped) {
            /* Pressed CTRL-C more than once. */
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                TEXT("CTRL-C trapped.  Forcing immediate shutdown."));
            halt = TRUE;
        } else {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                TEXT("CTRL-C trapped.  Shutting down."));
            ctrlCTrapped = TRUE;
        }
        quit = TRUE;
    }

    /* CTRL_CLOSE_EVENT */
    if (wrapperData->ctrlEventCloseTrapped) {
        wrapperData->ctrlEventCloseTrapped = FALSE;

        /*  Always quit.  If the user has tried to close the console previously then we want to force
         *   an immediate shutdown. */
        if (ctrlCTrapped) {
            /* Pressed Close or CTRL-C more than once. */
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                TEXT("Close trapped.  Forcing immediate shutdown."));
            halt = TRUE;
        } else {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                TEXT("Close trapped.  Shutting down."));
            ctrlCTrapped = TRUE;
        }
        quit = TRUE;
    }

    /* CTRL_LOGOFF_EVENT */
    if (wrapperData->ctrlEventLogoffTrapped) {
        wrapperData->ctrlEventLogoffTrapped = FALSE;

        /* Happens when the user logs off.  We should quit when run as a */
        /*  console, but stay up when run as a service. */
        if ((wrapperData->isConsole) && (!wrapperData->ignoreUserLogoffs)) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("User logged out.  Shutting down."));
            quit = TRUE;
        } else {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("User logged out.  Ignored."));
            quit = FALSE;
        }
    }

    /* CTRL_SHUTDOWN_EVENT */
    if (wrapperData->ctrlEventShutdownTrapped) {
        wrapperData->ctrlEventShutdownTrapped = FALSE;

        /* Happens when the machine is shutdown or rebooted.  Always quit. */
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
            TEXT("Machine is shutting down."));
        quit = TRUE;
    }

    /* Queued control codes. */
    while (wrapperData->ctrlCodeQueueReadIndex != wrapperData->ctrlCodeQueueWriteIndex) {
        ctrlCodeLast = wrapperData->ctrlCodeQueue[wrapperData->ctrlCodeQueueReadIndex];
#ifdef _DEBUG
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Process queued control code: %d (r:%d w:%d)"), ctrlCodeLast, wrapperData->ctrlCodeQueueReadIndex, wrapperData->ctrlCodeQueueWriteIndex);
#endif
        wrapperData->ctrlCodeQueueReadIndex++;
        if (wrapperData->ctrlCodeQueueReadIndex >= CTRL_CODE_QUEUE_SIZE ) {
            wrapperData->ctrlCodeQueueReadIndex = 0;
        }

        _sntprintf(buffer, 11, TEXT("%d"), ctrlCodeLast);
        wrapperProtocolFunction(WRAPPER_MSG_SERVICE_CONTROL_CODE, buffer);
    }

    /* SERVICE_CONTROL_PAUSE */
    if (wrapperData->ctrlCodePauseTrapped) {
        wrapperData->ctrlCodePauseTrapped = FALSE;

        /* Tell the wrapper to pause */
        wrapperPauseProcess(WRAPPER_ACTION_SOURCE_CODE_WINDOWS_SERVICE_MANAGER);
    }

    /* SERVICE_CONTROL_CONTINUE */
    if (wrapperData->ctrlCodeContinueTrapped) {
        wrapperData->ctrlCodeContinueTrapped = FALSE;

        /* Tell the wrapper to resume */
        wrapperResumeProcess(WRAPPER_ACTION_SOURCE_CODE_WINDOWS_SERVICE_MANAGER);
    }

    /* SERVICE_CONTROL_STOP */
    if (wrapperData->ctrlCodeStopTrapped) {
        wrapperData->ctrlCodeStopTrapped = FALSE;

        /* Request to stop the service. Report SERVICE_STOP_PENDING */
        /* to the service control manager before calling ServiceStop() */
        /* to avoid a "Service did not respond" error. */
        wrapperReportStatus(FALSE, WRAPPER_WSTATE_STOPPING, 0, 0);

        /* Tell the wrapper to shutdown normally */
        /* Always force the shutdown as this is an external event. */
        wrapperStopProcess(0, TRUE);

        /* To make sure that the JVM will not be restarted for any reason,
         *  start the Wrapper shutdown process as well.
         *  In this case we do not want to allow any exit filters to be used
         *  so setting this here will force the shutdown. */
        if ((wrapperData->wState == WRAPPER_WSTATE_STOPPING) ||
            (wrapperData->wState == WRAPPER_WSTATE_STOPPED)) {
            /* Already stopping. */
        } else {
            wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
        }
    }

    /* SERVICE_CONTROL_SHUTDOWN */
    if (wrapperData->ctrlCodeShutdownTrapped) {
        wrapperData->ctrlCodeShutdownTrapped = FALSE;

        /* Request to stop the service. Report SERVICE_STOP_PENDING */
        /* to the service control manager before calling ServiceStop() */
        /* to avoid a "Service did not respond" error. */
        wrapperReportStatus(FALSE, WRAPPER_WSTATE_STOPPING, 0, 0);

        /* Tell the wrapper to shutdown normally */
        /* Always force the shutdown as this is an external event. */
        wrapperStopProcess(0, TRUE);

        /* To make sure that the JVM will not be restarted for any reason,
         *  start the Wrapper shutdown process as well. */
        if ((wrapperData->wState == WRAPPER_WSTATE_STOPPING) ||
            (wrapperData->wState == WRAPPER_WSTATE_STOPPED)) {
            /* Already stopping. */
        } else {
            wrapperSetWrapperState(WRAPPER_WSTATE_STOPPING);
        }
    }

    /* The configured thread dump control code */
    if (wrapperData->ctrlCodeDumpTrapped) {
        wrapperData->ctrlCodeDumpTrapped = FALSE;

        wrapperRequestDumpJVMState();
    }

    if (quit) {
        if (halt) {
            /* Disable the thread dump on exit feature if it is set because it
             *  should not be displayed when the user requested the immediate exit. */
            wrapperData->requestThreadDumpOnFailedJVMExit = FALSE;
            wrapperKillProcess();
        } else {
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
    }
}

/**
 * The service control handler is called by the service manager when there are
 *    events for the service.  registered using a call to
 *    RegisterServiceCtrlHandler in wrapperServiceMain.
 *
 * Note on PowerEvents prior to win2k: http://blogs.msdn.com/heaths/archive/2005/05/18/419791.aspx
 */
DWORD WINAPI wrapperServiceControlHandlerEx(DWORD dwCtrlCode,
                                            DWORD dwEvtType,
                                            LPVOID lpEvtData,
                                            LPVOID lpCntxt) {

    DWORD result = result = NO_ERROR;

    /* Forward the control code off to the JVM. */
    DWORD controlCode = dwCtrlCode;

    /* Enclose the contents of this call in a try catch block so we can
     *  display and log useful information should the need arise. */
    __try {
        /*
        if (wrapperData->isDebugging) {
            log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("ServiceControlHandlerEx(%d, %d, %p, %p)"), dwCtrlCode, dwEvtType, lpEvtData, lpCntxt);
        }
        */

        /* This thread appears to always be the same as the main thread.
         *  Just to be safe reregister it. */
        logRegisterThread(WRAPPER_THREAD_MAIN);

        if (dwCtrlCode == SERVICE_CONTROL_POWEREVENT) {
            switch (dwEvtType) {
                case PBT_APMQUERYSUSPEND: /* 0x0 */
                    /* system is hiberating
                     * send off power resume event */
                    if (wrapperData->isDebugging) {
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_POWEREVENT(PBT_APMQUERYSUSPEND)"));
                    }
                    controlCode = 0x0D00;
                    break;

                case PBT_APMQUERYSUSPENDFAILED: /* 0x2 */
                    /* system is waking up
                     * send off power resume event */
                    if (wrapperData->isDebugging) {
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_POWEREVENT(PBT_APMQUERYSUSPENDFAILED)"));
                    }
                    controlCode = 0x0D02;
                    break;

                case PBT_APMSUSPEND:/* 0x4 */
                    /* system is waking up
                     * send off power resume event */
                    if (wrapperData->isDebugging) {
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_POWEREVENT(PBT_APMSUSPEND)"));
                    }
                    controlCode = 0x0D04;
                    break;

                case PBT_APMRESUMECRITICAL: /* 0x6 */
                    /* system is waking up
                     * send off power resume event */
                    if (wrapperData->isDebugging) {
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_POWEREVENT(PBT_APMRESUMECRITICAL)"));
                    }
                    controlCode = 0x0D06;
                    break;

                case PBT_APMRESUMESUSPEND: /* 0x7 */
                    /* system is waking up
                     * send off power resume event */
                    if (wrapperData->isDebugging) {
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_POWEREVENT(PBT_APMRESUMESUSPEND)"));
                    }
                    controlCode = 0x0D07;
                    break;

                case PBT_APMBATTERYLOW: /* 0x9 */
                    /* batter is low warning. */
                    if (wrapperData->isDebugging) {
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_POWEREVENT(PBT_APMBATTERYLOW)"));
                    }
                    controlCode = 0x0D09;
                    break;

                case PBT_APMPOWERSTATUSCHANGE: /* 0xA */
                    /* the status of system power changed. */
                    if (wrapperData->isDebugging) {
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_POWEREVENT(PBT_APMPOWERSTATUSCHANGE)"));
                    }
                    controlCode = 0x0D0A;
                    break;

                case PBT_APMOEMEVENT: /* 0xB */
                    /* there was an OEM event. */
                    if (wrapperData->isDebugging) {
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_POWEREVENT(PBT_APMOEMEVENT)"));
                    }
                    controlCode = 0x0D0B;
                    break;

                case PBT_APMRESUMEAUTOMATIC: /* 0x12 */
                    /* system is waking up */
                    if (wrapperData->isDebugging) {
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_POWEREVENT(PBT_APMRESUMEAUTOMATIC)"));
                    }
                    controlCode = 0x0D12;
                    break;

                /* The following STANDBY values do not appear to be used but are defined in WinUser.h. */
                /*case PBT_APMQUERYSTANDBY:*/ /* 0x1 */
                /*case PBT_APMQUERYSTANDBYFAILED:*/ /* 0x3 */
                /*case PBT_APMSTANDBY:*/ /* 0x5 */
                /*case PBT_APMRESUMESTANDBY:*/ /* 0x8 */

                default:
                    /* Unexpected generic powerevent code */
                    if (wrapperData->isDebugging) {
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_POWEREVENT(%d)"), dwEvtType);
                    }
                    break;
            }
        }

        /* Forward the control code off to the JVM.  Write the signals into a rotating queue so we can process more than one per loop. */
        if ((wrapperData->ctrlCodeQueueWriteIndex == wrapperData->ctrlCodeQueueReadIndex - 1) || ((wrapperData->ctrlCodeQueueWriteIndex == CTRL_CODE_QUEUE_SIZE - 1) && (wrapperData->ctrlCodeQueueReadIndex == 0))) {
            log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Control code queue overflow (%d:%d).  Dropping control code: %d\n"), wrapperData->ctrlCodeQueueWriteIndex, wrapperData->ctrlCodeQueueReadIndex, controlCode);
        } else {
#ifdef _DEBUG
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Enqueue control code: %d (r:%d w:%d)"), controlCode, wrapperData->ctrlCodeQueueReadIndex, wrapperData->ctrlCodeQueueWriteIndex);
#endif
            wrapperData->ctrlCodeQueue[wrapperData->ctrlCodeQueueWriteIndex] = controlCode;

            wrapperData->ctrlCodeQueueWriteIndex++;
            if (wrapperData->ctrlCodeQueueWriteIndex >= CTRL_CODE_QUEUE_SIZE) {
                wrapperData->ctrlCodeQueueWriteIndex = 0;
                wrapperData->ctrlCodeQueueWrapped = TRUE;
            }
        }

        switch(dwCtrlCode) {
        case SERVICE_CONTROL_PAUSE:
            if (wrapperData->isDebugging) {
                log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_PAUSE"));
            }

            wrapperData->ctrlCodePauseTrapped = TRUE;

            break;

        case SERVICE_CONTROL_CONTINUE:
            if (wrapperData->isDebugging) {
                log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_CONTINUE"));
            }

            wrapperData->ctrlCodeContinueTrapped = TRUE;

            break;

        case SERVICE_CONTROL_STOP:
            if (wrapperData->isDebugging) {
                log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_STOP"));
            }

            wrapperData->ctrlCodeStopTrapped = TRUE;

            break;

        case SERVICE_CONTROL_INTERROGATE:
            if (wrapperData->isDebugging) {
                log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_INTERROGATE"));
            }

            /* This case MUST be processed, even though we are not */
            /* obligated to do anything substantial in the process. */
            break;

        case SERVICE_CONTROL_POWEREVENT:
            // we handled it
            if (wrapperData->isDebugging) {
                log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_POWEREVENT (handled)"));
            }
            break;

        case SERVICE_CONTROL_SHUTDOWN:
            if (wrapperData->isDebugging) {
                log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_SHUTDOWN"));
            }

            wrapperData->ctrlCodeShutdownTrapped = TRUE;

            break;

        default:
            if ((wrapperData->threadDumpControlCode > 0) && (dwCtrlCode == wrapperData->threadDumpControlCode)) {
                if (wrapperData->isDebugging) {
                    log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_(%d) Request Thread Dump."), dwCtrlCode);
                }

                wrapperData->ctrlCodeDumpTrapped = TRUE;
            } else {
                /* Any other cases... Did not handle */
                if (wrapperData->isDebugging) {
                    log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("  SERVICE_CONTROL_(%d) Not handled."), dwCtrlCode);
                }
                result = ERROR_CALL_NOT_IMPLEMENTED;
            }
            break;
        }

        /* After invocation of this function, we MUST call the SetServiceStatus */
        /* function, which is accomplished through our ReportStatus function. We */
        /* must do this even if the current status has not changed. */
        wrapperReportStatus(TRUE, wrapperData->wState, 0, 0);

    } __except (exceptionFilterFunction(GetExceptionInformation())) {
        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
            TEXT("<-- Wrapper Stopping due to error in service control handler."));
        appExit(1);
    }

    return result;
}

/**
 * The service control handler is called by the service manager when there are
 *    events for the service.  registered using a call to
 *    RegisterServiceCtrlHandler in wrapperServiceMain.
 */
void WINAPI wrapperServiceControlHandler(DWORD dwCtrlCode) {
    /*
    if (wrapperData->isDebugging) {
        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Service(%d)"), dwCtrlCode);
    }
    */
    wrapperServiceControlHandlerEx(dwCtrlCode, 0, NULL, NULL);
}

/**
 * The wrapperServiceMain function is the entry point for the NT service.
 *    It is called by the service manager.
 */
void WINAPI wrapperServiceMain(DWORD dwArgc, LPTSTR *lpszArgv) {
    int timeout;

    /* Enclose the contents of this call in a try catch block so we can
     *  display and log useful information should the need arise. */
    __try {
#ifdef _DEBUG
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("wrapperServiceMain()"));
#endif

        /* Immediately register this thread with the logger. */
        logRegisterThread(WRAPPER_THREAD_SRVMAIN);

        /* Call RegisterServiceCtrlHandler immediately to register a service control */
        /* handler function. The returned SERVICE_STATUS_HANDLE is saved with global */
        /* scope, and used as a service id in calls to SetServiceStatus. */
        if (OptionalRegisterServiceCtrlHandlerEx) {
            /* Use RegisterServiceCtrlHandlerEx if available. */
            sshStatusHandle = OptionalRegisterServiceCtrlHandlerEx(
                wrapperData->serviceName, wrapperServiceControlHandlerEx, (LPVOID)1);
        } else {
            sshStatusHandle = RegisterServiceCtrlHandler(
                wrapperData->serviceName, wrapperServiceControlHandler);
        }
        if (!sshStatusHandle) {
            goto finally;
        }

        /* The global ssStatus SERVICE_STATUS structure contains information about the */
        /* service, and is used throughout the program in calls made to SetStatus through */
        /* the ReportStatus function. */
        ssStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
        ssStatus.dwServiceSpecificExitCode = 0;


        /* If we could guarantee that all initialization would occur in less than one */
        /* second, we would not have to report our status to the service control manager. */
        /* For good measure, we will assign SERVICE_START_PENDING to the current service */
        /* state and inform the service control manager through our ReportStatus function. */
        if (wrapperData->startupTimeout > 0) {
            timeout = wrapperData->startupTimeout * 1000;
        } else {
            timeout = 86400000; // Set infinity at 1 day.
        }
        wrapperReportStatus(FALSE, WRAPPER_WSTATE_STARTING, 0, timeout);

        /* Now actually start the service */
        wrapperRunService();

 finally:

        /* Report that the service has stopped and set the correct exit code. */
        wrapperReportStatus(FALSE, WRAPPER_WSTATE_STOPPED, wrapperData->exitCode, 1000);

#ifdef _DEBUG
        /* The following message will not always appear on the screen if the STOPPED
         *  status was set above.  But the code in the appExit function below always
         *  appears to be getting executed.  Looks like some kind of a timing issue. */
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Exiting service process."));
#endif

        /* Actually exit the process, returning the current exit code. */
        appExit(wrapperData->exitCode);

    } __except (exceptionFilterFunction(GetExceptionInformation())) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
            TEXT("<-- Wrapper Stopping due to error in service main."));
        appExit(1);
    }
}

/**
 * Reads a password from the console and then returns it as a malloced string.
 *  This is only called once so the memory can leak.
 */
TCHAR *readPassword() {
    TCHAR *buffer;
    TCHAR c;
    int cnt = 0;

    buffer = malloc(sizeof(TCHAR) * 65);
    if (!buffer) {
        outOfMemory(TEXT("RP"), 1);
        appExit(0);
        return NULL;
    }
    buffer[0] = 0;

    do {
        c = _getch();
        switch (c) {
        case 0x03: /* Ctrl-C */
            _tprintf(TEXT("\n") );
            appExit(0);
            break;

        case 0x08: /* Backspace */
            if (cnt > 0) {
                _tprintf(TEXT("%c %c"), 0x08, 0x08);
                cnt--;
                buffer[cnt] = 0;
            }
            break;

        case 0xffffffe0: /* Arrow key. */
            /* Skip the next character as well. */
            _getch();
            break;

        case 0x0d: /* CR */
        case 0x0a: /* LF */
            /* Done */
            break;

        default:
            if (cnt < 64) {
                /* For now, ignore any non-standard ascii characters. */
                if ((c >= 0x20) && (c < 0x7f)) {
                    if (wrapperData->ntServicePasswordPromptMask) {
                        printf("*");
                    } else {
                        _tprintf(TEXT("%c"), c);
                    }
                    buffer[cnt] = c;
                    buffer[cnt + 1] = 0;
                    cnt++;
                }
            }
            break;
        }
        /*printf("(%02x)", c);*/
    } while ((c != 0x0d) && (c != 0x0a));
    _tprintf(TEXT("\n"));

    return buffer;
}


/**
 * RETURNS TRUE if the current Windows OS is Windows Vista or later...
 */
BOOL isVista() {
    OSVERSIONINFO osver;

    osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    if (GetVersionEx(&osver) &&
            osver.dwPlatformId == VER_PLATFORM_WIN32_NT &&
            osver.dwMajorVersion >= 6) {
        return TRUE;
    }
    return FALSE;
}


/**
 * RETURNS TRUE if the current Windows OS is Windows XP or later...
 * http://msdn.microsoft.com/en-us/library/ms724834%28VS.85%29.aspx
 */
BOOL isWinXP() {
    OSVERSIONINFO osver;

    osver.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);

    if (GetVersionEx(&osver) && osver.dwPlatformId == VER_PLATFORM_WIN32_NT) {
        if (osver.dwMajorVersion > 5 || osver.dwMajorVersion == 5 && osver.dwMinorVersion >= 1) {
            return TRUE;
        }
    }
    return FALSE;
}


BOOL isElevated() {
    TOKEN_ELEVATION te = {0};
    BOOL bIsElevated = FALSE;
    HRESULT hResult = E_FAIL; // assume an error occured
    HANDLE hToken   = NULL;
    DWORD dwReturnLength = 0;
    if (isVista()) {
        if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
            return bIsElevated ;
        }
        if (!GetTokenInformation(hToken, TokenElevation, &te, sizeof(te), &dwReturnLength)) {
            ;
        } else {
            hResult = te.TokenIsElevated ? S_OK : S_FALSE;
            bIsElevated = (te.TokenIsElevated != 0);
        }
        CloseHandle(hToken);
        return bIsElevated;
    } else {
        return TRUE;
    }
}


void wrapperCheckForMappedDrives() {
    TCHAR **propertyNames;
    TCHAR **propertyValues;
    long unsigned int *propertyIndices;
    int i;
    int advice = 0;
    if (!wrapperData->ntServiceAccount) {
        advice = wrapperGetUNCFilePath(getFileSafeStringProperty(properties, TEXT("wrapper.logfile"), TEXT("wrapper.log")), advice);
        advice = wrapperGetUNCFilePath(getFileSafeStringProperty(properties, TEXT("wrapper.logfile.purge.pattern"), TEXT("")), advice);
        advice = wrapperGetUNCFilePath(getFileSafeStringProperty(properties, TEXT("wrapper.pidfile"), NULL), advice);
        advice = wrapperGetUNCFilePath(getFileSafeStringProperty(properties, TEXT("wrapper.java.pidfile"), NULL), advice);
        advice = wrapperGetUNCFilePath(getFileSafeStringProperty(properties, TEXT("wrapper.lockfile"), NULL), advice);
        advice = wrapperGetUNCFilePath(getFileSafeStringProperty(properties, TEXT("wrapper.java.idfile"), NULL), advice);
        advice = wrapperGetUNCFilePath(getFileSafeStringProperty(properties, TEXT("wrapper.statusfile"), NULL), advice);
        advice = wrapperGetUNCFilePath(getFileSafeStringProperty(properties, TEXT("wrapper.java.statusfile"), NULL), advice);
        advice = wrapperGetUNCFilePath(getFileSafeStringProperty(properties, TEXT("wrapper.commandfile"), NULL), advice);
        advice = wrapperGetUNCFilePath(getFileSafeStringProperty(properties, TEXT("wrapper.anchorfile"), NULL), advice);
        i = 0;
        if (getStringProperties(properties, TEXT("wrapper.java.library.path."), TEXT(""), wrapperData->ignoreSequenceGaps, FALSE, &propertyNames, &propertyValues, &propertyIndices)) {
                /* Failed */
            return ;
        }
        while (propertyNames[i]) {
            if (propertyValues[i]) {
                advice = wrapperGetUNCFilePath(propertyValues[i], advice);
                i++;
            }

        }
        i = 0;
        if (getStringProperties(properties, TEXT("wrapper.java.classpath."), TEXT(""), wrapperData->ignoreSequenceGaps, FALSE, &propertyNames, &propertyValues, &propertyIndices)) {
                /* Failed */
            return ;
        }
        while (propertyNames[i]) {
            if (propertyValues[i]) {
                advice = wrapperGetUNCFilePath(propertyValues[i], advice);
                i++;
            }
        }
    }
}

/**
 * Generates the full binary path to register with the service manager when
 *  installing a service.
 *
 * @param buffer Buffer that will hold the binaryPath.  If NULL, the required
 *               length will be calculated and stored in reqBufferSize
 * @param reqBufferSize Pointer to an int that will store the required length in character
 *                      of the buffer that was used or is required.
 *
 * @return 0 if succeeded.
 */
int buildServiceBinaryPath(TCHAR *buffer, size_t *reqBufferLen) {
    DWORD moduleFileNameSize;
    TCHAR *moduleFileName;
    DWORD usedLen;
    TCHAR drive[4];
    TCHAR* uncTempBuffer;
    DWORD uncSize;
    int pathMapped;
    int pathMapFailed = FALSE;
    UNIVERSAL_NAME_INFO* unc;
    int i;
    int k;
    size_t originalSize;

    if (reqBufferLen) {
        originalSize = *reqBufferLen;
    } else {
        originalSize = 0;
    }

    /* We will calculate the size used. */
    if (buffer) {
        buffer[0] = TEXT('\0');
    }
    *reqBufferLen = 1;
    /* Get the full path and filename of this program.  Need to loop to make sure we get it all. */
    moduleFileNameSize = 0;
    moduleFileName = NULL;
    do {
        moduleFileNameSize += 100;
        moduleFileName = malloc(sizeof(TCHAR) * moduleFileNameSize);
        if (!moduleFileName) {
            outOfMemory(TEXT("BSBP"), 1);
            return 1;
        }

        /* On Windows XP and 2000, GetModuleFileName will return exactly "moduleFileNameSize" and
         *  leave moduleFileName in an unterminated state in the event that the module file name is too long.
         *  Newer versions of Windows will set the error code to ERROR_INSUFFICIENT_BUFFER but we can't rely on that. */
        usedLen = GetModuleFileName(NULL, moduleFileName, moduleFileNameSize);
        if (usedLen == 0) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to resolve the full Wrapper path - %s"), getLastErrorText());
            return 1;
        } else if ((usedLen == moduleFileNameSize) || (getLastError() == ERROR_INSUFFICIENT_BUFFER)) {
            /* Buffer too small.  Loop again. */
            free(moduleFileName);
            moduleFileName = NULL;
        }
    } while (!moduleFileName);
    /* Always start with the full path to the binary. */
    /* If the moduleFileName contains spaces, it needs to be quoted */
    /* Resolve to UNC-Name if we are on a mapped drive */
    if ((_tcslen(moduleFileName) >= 3) && (moduleFileName[1] == TEXT(':')) && (moduleFileName[2] == TEXT('\\'))) {
        _tcsncpy(drive, moduleFileName, 3);
        drive[3] = TEXT('\0');
    } else {
        drive[0] = TEXT('\0');
    }
    pathMapped = FALSE;
    if ((drive[0] != TEXT('\0')) && (GetDriveType(drive) == DRIVE_REMOTE)) {
        /* The Wrapper binary is located on a Network Drive.  Try to resolve the original Universal path.  We need to get a buffer big enough. */
        uncSize = 0;
        moduleFileNameSize = 100;
        do{
            uncTempBuffer = malloc((moduleFileNameSize) * sizeof(TCHAR));
            if (!uncTempBuffer) {
                outOfMemory(TEXT("BSBP"), 2);
                return 1;
            }
            unc = (UNIVERSAL_NAME_INFO *) uncTempBuffer;
            k = WNetGetUniversalName(moduleFileName, UNIVERSAL_NAME_INFO_LEVEL, unc, &moduleFileNameSize);
            if (k == ERROR_MORE_DATA) {
                free(uncTempBuffer);
            }
        } while (k == ERROR_MORE_DATA);
        uncSize = moduleFileNameSize;
        if (k != NO_ERROR) {
            if (buffer) { /* Otherwise logged on the next pass. */
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Unable to resolve Universal Path of mapped network path: %s (%s)"), moduleFileName, getLastErrorText());
            }
            pathMapFailed = TRUE;
        } else {
            /* Now we know the size.  Create the unc buffer. */
            if (_tcschr(unc->lpUniversalName, TEXT(' ')) == NULL) {
                if (buffer) {
                    _tcsncat(buffer, unc->lpUniversalName, originalSize);
                }
            *reqBufferLen += _tcslen(unc->lpUniversalName);
            } else {
                if (buffer) {
                    _tcsncat(buffer, TEXT("\""), originalSize);
                    _tcsncat(buffer, unc->lpUniversalName, originalSize);
                    _tcsncat(buffer, TEXT("\""), originalSize);
                }
                *reqBufferLen += (1 + _tcslen(unc->lpUniversalName) + 1);
            }
            pathMapped = TRUE;
            free(uncTempBuffer);
        }
    }

    if (!pathMapped) {
        if (_tcschr(moduleFileName, TEXT(' ')) == NULL) {
            if (buffer) {
                _tcsncat(buffer, moduleFileName, originalSize);
            }
            *reqBufferLen += _tcslen(moduleFileName);
        } else {
            if (buffer) {
                _tcsncat(buffer, TEXT("\""), originalSize);
                _tcsncat(buffer, moduleFileName, originalSize);
                _tcsncat(buffer, TEXT("\""), originalSize);
            }
            *reqBufferLen += (1 + _tcslen(moduleFileName) + 1);
        }
    }
    free(moduleFileName);

    /* Next write the command to start the service. */
    if (buffer) {
        _tcsncat(buffer, TEXT(" -s "), originalSize);
    }
    *reqBufferLen += 4;

    /* Third, the configuration file. */
    /* If the wrapperData->configFile contains spaces, it needs to be quoted */
    /* Try to convert the config file to a UNC path as well. */
    if ((_tcslen(wrapperData->configFile) >= 3) && (wrapperData->configFile[1] == TEXT(':')) && (wrapperData->configFile[2] == TEXT('\\'))) {
        _tcsncpy(drive, wrapperData->configFile, 3);
        drive[3] = TEXT('\0');
    } else {
        drive[0] = TEXT('\0');
    }
    pathMapped = FALSE;
    if ((drive[0] != TEXT('\0')) && (GetDriveType(drive) == DRIVE_REMOTE)) {
        /* The Wrapper config file is located on a Network Drive.  Try to resolve the original Universal path.  We need to get a buffer big enough. */
        moduleFileNameSize = 100;
        uncSize = 0;
        do {
            uncTempBuffer = malloc((moduleFileNameSize) * sizeof(TCHAR));
            if (!uncTempBuffer) {
                outOfMemory(TEXT("BSBP"), 3);
                return 1;
            }

            unc = (UNIVERSAL_NAME_INFO *) uncTempBuffer;

            k = WNetGetUniversalName(wrapperData->configFile, UNIVERSAL_NAME_INFO_LEVEL, unc, &moduleFileNameSize);
            if (k == ERROR_MORE_DATA) {
                free(uncTempBuffer);
            }
        } while (k == ERROR_MORE_DATA);
        if (k != NO_ERROR) {
            if (buffer) { /* Otherwise logged on the next pass. */
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Unable to resolve Universal Path of mapped network path: %s (%s)"), wrapperData->configFile, getLastErrorText());
            }
            pathMapFailed = TRUE;
        } else {
           /* Now we know the size.  Create the unc buffer. */
            if (_tcschr(unc->lpUniversalName, TEXT(' ')) == NULL) {
                if (buffer) {
                    _tcsncat(buffer, unc->lpUniversalName, originalSize);
                }
                *reqBufferLen += _tcslen(unc->lpUniversalName);
            } else {
                if (buffer) {
                    _tcsncat(buffer, TEXT("\""), originalSize);
                    _tcsncat(buffer, unc->lpUniversalName, originalSize);
                    _tcsncat(buffer, TEXT("\""), originalSize);
                }
                *reqBufferLen += (1 + _tcslen(unc->lpUniversalName) + 1);
            }
            pathMapped = TRUE;
            free(uncTempBuffer);
            unc = NULL;
        }
    }
    if (!pathMapped) {
        if (_tcschr(wrapperData->configFile, TEXT(' ')) == NULL) {
            if (buffer) {
                _tcsncat(buffer, wrapperData->configFile, originalSize);
            }
            *reqBufferLen += _tcslen(wrapperData->configFile);
        } else {
            if (buffer) {
                _tcsncat(buffer, TEXT("\""), originalSize);
                _tcsncat(buffer, wrapperData->configFile, originalSize);
                _tcsncat(buffer, TEXT("\""), originalSize);
            }
            *reqBufferLen += (1 + _tcslen(wrapperData->configFile) + 1);
        }
    }

    if (pathMapFailed) {
        if (buffer) { /* Otherwise logged on the next pass. */
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("There were problems converting mapped network paths the Universal Path format.  This may cause the service to fail to start now or when the system is rebooted."));
        }
    }

    /* All other arguments need to be appended as is. */
    for (i = 0; i < wrapperData->argCount; i++) {
        /* For security reasons, skip the wrapper.ntservice.account and
         *  wrapper.ntservice.password properties if they are declared on the
         *  command line.  They will not be needed  once the service is
         *  installed.  Having them in the registry would be an obvious
         *  security leak. */
        if ((_tcsstr(wrapperData->argValues[i], TEXT("wrapper.ntservice.account")) == NULL) &&
            (_tcsstr(wrapperData->argValues[i], TEXT("wrapper.ntservice.password")) == NULL)) {
            if (buffer) {
                _tcsncat(buffer, TEXT(" "), originalSize);
            }
            *reqBufferLen += 1;

            /* If the argument contains spaces, it needs to be quoted */
            if (_tcschr(wrapperData->argValues[i], TEXT(' ')) == NULL) {
                if (buffer) {
                    _tcsncat(buffer, wrapperData->argValues[i], originalSize);
                }
                *reqBufferLen += _tcslen(wrapperData->argValues[i]);
            } else {
                if (buffer) {
                    _tcsncat(buffer, TEXT("\""), originalSize);
                    _tcsncat(buffer, wrapperData->argValues[i], originalSize);
                    _tcsncat(buffer, TEXT("\""), originalSize);
                }
                *reqBufferLen += 1 + _tcslen(wrapperData->argValues[i]) + 1;
            }
        }
    }

    /* If there are any passthrough variables.  Then they also need to be appended as is. */
    if (wrapperData->javaArgValueCount > 0) {
        if (buffer) {
            _tcsncat(buffer, TEXT(" --"), originalSize);
        }
        *reqBufferLen += 3;

        for (i = 0; i < wrapperData->javaArgValueCount; i++) {
            if (buffer) {
                _tcsncat(buffer, TEXT(" "), originalSize);
            }
            *reqBufferLen += 1;

            /* If the argument contains spaces, it needs to be quoted */
            if (_tcschr(wrapperData->javaArgValues[i], TEXT(' ')) == NULL) {
                if (buffer) {
                    _tcsncat(buffer, wrapperData->javaArgValues[i], originalSize);
                }
                *reqBufferLen += _tcslen(wrapperData->javaArgValues[i]);
            } else {
                if (buffer) {
                    _tcsncat(buffer, TEXT("\""), originalSize);
                    _tcsncat(buffer, wrapperData->javaArgValues[i], originalSize);
                    _tcsncat(buffer, TEXT("\""), originalSize);
                }
                *reqBufferLen += (1 + _tcslen(wrapperData->javaArgValues[i]) + 1);
            }
        }
    }
    return 0;
}

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS  ((NTSTATUS)0x00000000L)
#endif

LSA_UNICODE_STRING InitLsaString(LPCTSTR pwszString) {
    USHORT dwLen = 0;
    LSA_UNICODE_STRING pLsaString;

    if ((pwszString != NULL) && ((dwLen = (USHORT)wcslen(pwszString)) > 0)) {
      pLsaString.Buffer = (WCHAR*)pwszString;
    }  
    pLsaString.Length =  dwLen * sizeof(wchar_t);
    pLsaString.MaximumLength= (dwLen+1) * sizeof(wchar_t);  
    return pLsaString;
}

/**
 *  Helperfunction which gets the Security Policy Handle of the specified system
 *  @param referencedDomainName, the system of which the Security Policy Handle should get retrieved
 *
 *  @return the Handle of the Security Policy, NULL in case of any error
 */
LSA_HANDLE wrapperGetPolicyHandle(LPCTSTR referencedDomainName) {
    LSA_OBJECT_ATTRIBUTES ObjectAttributes;
    LSA_UNICODE_STRING domain;
    NTSTATUS ntsResult;
    LSA_HANDLE lsahPolicyHandle;

    ZeroMemory(&ObjectAttributes, sizeof(ObjectAttributes));

    domain = InitLsaString(referencedDomainName);
    ntsResult = LsaOpenPolicy(&domain,    /* Name of the target system. */
                              &ObjectAttributes, /* Object attributes. */
                              POLICY_LOOKUP_NAMES | POLICY_CREATE_ACCOUNT, /* Desired access permissions. */
                              &lsahPolicyHandle); /*Receives the policy handle. */
    if (ntsResult != STATUS_SUCCESS) {
        /* An error occurred. Display it as a win32 error code. */
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("OpenPolicy failed %lu"),LsaNtStatusToWinError(ntsResult));
        return NULL;
    }
    return lsahPolicyHandle;
}

/**
 *  Helperfunction which gets the SID and domain of a given account name
 *  @param lpszAccountName, the account namespace
 *  @param referencedDomainName, output buffer for the domain
 *
 *  @return the SID of the account, 0 in case of any error
 */
PSID wrapperLookupName(LPCTSTR lpszAccountName, WCHAR **referencedDomainName) {
    PSID         Sid;
    DWORD        cbReferencedDomainName, cbSid, lastError;
    SID_NAME_USE eUse;  
    LPCTSTR formattedAccountName;

    if (_tcsstr(lpszAccountName, TEXT(".\\")) == lpszAccountName) {
        formattedAccountName = lpszAccountName + 2;
    } else { 
        formattedAccountName= lpszAccountName;
    }

    cbReferencedDomainName = cbSid = 0;
    if (LookupAccountName(NULL, formattedAccountName, 0, &cbSid, 0, &cbReferencedDomainName, &eUse)) {
        /* A straight success - that can't be... */
        return 0;
    }
    lastError = GetLastError();
    if (lastError != ERROR_INSUFFICIENT_BUFFER) {
        /* Any error except the one above is fatal.. */
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Failed to lookup the account (%s): %d - %s\n"), lpszAccountName, lastError, getLastErrorText());
        return 0;
    }
    if (!(Sid = (PSID)malloc(cbSid))) {
        outOfMemory(TEXT("WLN"), 1);
        return 0;
    }

    *referencedDomainName = (LPTSTR)malloc((cbReferencedDomainName + 1) * sizeof(TCHAR));
    if (!(*referencedDomainName)) {
        LocalFree(Sid);
        outOfMemory(TEXT("WLN"), 2);
        return 0;
    }
    if (!LookupAccountName(NULL, formattedAccountName, Sid, &cbSid, *referencedDomainName, &cbReferencedDomainName, &eUse)) {
        free(*referencedDomainName);
        free(Sid);
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Failed to lookup the account (%s): %d - %s\n"), lpszAccountName, lastError, getLastErrorText());
        return 0;
    }
    return Sid;
}

/**
 * This functions adds the Logon as Service privileges to the user account
 *
 * @param the account name for which the privilege should be added.
 *
 * @return FALSE if successful, TRUE otherwise
 */
BOOL wrapperAddPrivileges(TCHAR *account) {
    PLSA_UNICODE_STRING pointer;
    NTSTATUS ntsResult;
    LSA_HANDLE PolicyHandle;
    PSID AccountSID;
    TCHAR *referencedDomainName;
    ULONG counter = 1;
    WCHAR privileges[] = SE_SERVICE_LOGON_NAME;
    int retVal = TRUE;

    AccountSID = wrapperLookupName(account, &referencedDomainName);	

    if (AccountSID) {		
        if ((PolicyHandle = wrapperGetPolicyHandle(referencedDomainName)) != NULL) {
            /* Create an LSA_UNICODE_STRING for the privilege names. */
            pointer = malloc(sizeof(LSA_UNICODE_STRING));
            pointer[0] = InitLsaString(privileges);

            ntsResult = LsaAddAccountRights(PolicyHandle,  /* An open policy handle. */
                                        AccountSID,    /* The target SID. */
                                        pointer, /* The privileges. */
                                        counter);            /* Number of privileges. */
            free(pointer);
            if (ntsResult == STATUS_SUCCESS) {
                retVal =  FALSE;
            } else {
               log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Failed to add Logon As Service Permission: %lu\n"), LsaNtStatusToWinError(ntsResult));
            }
        } 
        free(AccountSID);
        free(referencedDomainName);
    } 
    return retVal;
} 

/**
 * Install the Wrapper as an NT Service using the information and service
 *  name in the current configuration file.
 *
 * Stores the parameters with the service name so that the wrapper.conf file
 *  can be located at runtime.
 */
int wrapperInstall() {
    SC_HANDLE schService;
    SC_HANDLE schSCManager;
    DWORD serviceType;
    DWORD startType;
    size_t binaryPathLen;
    TCHAR *binaryPath;
    int result = 0;
    HKEY hKey;
    TCHAR regPath[ 1024 ];
    TCHAR domain[ 1024 ];
    TCHAR account[ 1024 ];
    TCHAR *tempAccount;
    TCHAR *ntServicePassword;
    DWORD dsize = 1024, dwDesiredAccess;

    /* Initialization */
    dwDesiredAccess = 0;

    /* Generate the service binary path.  We need to figure out how big the buffer needs to be. */
    if (buildServiceBinaryPath(NULL, &binaryPathLen)) {
        /* Failed a reason should have been given. But show result. */
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to install the %s service"), wrapperData->serviceDisplayName);
        return 1;
    }

    binaryPath = malloc(binaryPathLen * sizeof(TCHAR));
    if (!binaryPath) {
        outOfMemory(TEXT("WI"), 1);
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to install the %s service"), wrapperData->serviceDisplayName);
        return 1;
    }
    if (buildServiceBinaryPath(binaryPath, &binaryPathLen)) {
        /* Failed a reason should have been given. But show result. */
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to install the %s service"), wrapperData->serviceDisplayName);
        free(binaryPath);
        return 1;
    }

    if (wrapperData->isDebugging) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Service command: %s"), binaryPath);
    }
    if (wrapperData->ntServicePrompt) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Prompting for account (DOMAIN\\ACCOUNT)..."));
        _tprintf(TEXT("Please input the domain name [%s]: "), wrapperData->domainName);
        if (isElevated() && getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL) != NULL) {
           _tprintf(TEXT("n"));
           fflush(NULL);
        }
        _fgetts(domain, dsize, stdin);
        if (!domain || _tcscmp(domain, TEXT("\n")) == 0) {
            _sntprintf(domain, dsize, TEXT("%s"), wrapperData->domainName);
        } else if (domain[_tcslen(domain) - 1] == TEXT('\n')) {
            domain[_tcslen(domain) - 1] = TEXT('\0');
        }

        _tprintf(TEXT("Please input the account name [%s]: "), wrapperData->userName);
        if (isElevated() && getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL) != NULL) {
           _tprintf(TEXT("n"));
           fflush(NULL);
        }
        _fgetts(account, dsize, stdin);
        if (!account || _tcscmp(account, TEXT("\n")) == 0) {
            _sntprintf(account, dsize, TEXT("%s"), wrapperData->userName);
        } else if (account[_tcslen(account) - 1] == TEXT('\n')) {
            account[_tcslen(account) - 1] = TEXT('\0');
        }
        tempAccount = malloc((_tcslen(domain) + _tcslen(account) + 2) * sizeof(TCHAR));
        if (!tempAccount) {
            outOfMemory(TEXT("WI"), 2);
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to install the %s service"), wrapperData->serviceDisplayName);
            free(binaryPath);
            return 1;
        }
        _sntprintf(tempAccount, _tcslen(domain) + _tcslen(account) + 2, TEXT("%s\\%s"), domain, account);
        updateStringValue(&wrapperData->ntServiceAccount, tempAccount);
        free(tempAccount);
    }


    if (wrapperData->ntServiceAccount && wrapperData->ntServicePasswordPrompt) {
        /* Prompt the user for a password. */
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Prompting for account password..."));
        _tprintf(TEXT("Please input the password for account '%s': "), wrapperData->ntServiceAccount);
        if (isElevated() && getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL) != NULL) {
            _tprintf(TEXT("p"));
            fflush(NULL);
            /* as this here is from the secondary instance we can read with _fgetts */
            wrapperData->ntServicePassword = calloc(65, sizeof(TCHAR));
            if (!wrapperData->ntServicePassword) {
                outOfMemory(TEXT("WI"), 21);
                free(binaryPath);
                return 1;
            }
            _fgetts(wrapperData->ntServicePassword, 65, stdin);
        } else {
            wrapperData->ntServicePassword = readPassword();
        }
    }

    /* Decide on the service type */
    if (wrapperData->ntServiceInteractive) {
        serviceType = SERVICE_WIN32_OWN_PROCESS | SERVICE_INTERACTIVE_PROCESS;
    } else {
        serviceType = SERVICE_WIN32_OWN_PROCESS;
    }

    /* Next, get a handle to the service control manager */
    schSCManager = OpenSCManager(
            NULL,
            NULL,
            SC_MANAGER_CONNECT | SC_MANAGER_CREATE_SERVICE
    );

    if (schSCManager) {
        if (wrapperData->ntServiceAccount && wrapperAddPrivileges(wrapperData->ntServiceAccount)) {
            /* adding failed it was reported already above */
        }

        /* Make sure that an empty length password is null. */
        ntServicePassword = wrapperData->ntServicePassword;
        if ((ntServicePassword != NULL) && (_tcslen(ntServicePassword) <= 0)) {
            ntServicePassword = NULL;
        }

        startType = wrapperData->ntServiceStartType;

        if (result != 1) {
            schService = CreateService(schSCManager, /* SCManager database */
                    wrapperData->serviceName, /* name of service */
                    wrapperData->serviceDisplayName, /* name to display */
                    dwDesiredAccess, /* desired access */
                    serviceType, /* service type */
                    startType, /* start type */
                    SERVICE_ERROR_NORMAL, /* error control type */
                    binaryPath, /* service's binary */
                    wrapperData->ntServiceLoadOrderGroup, /* load ordering group */
                    NULL, /* tag identifier not used because they are used for driver level services. */
                    wrapperData->ntServiceDependencies, /* dependencies */
                    wrapperData->ntServiceAccount, /* LocalSystem account if NULL */
                    ntServicePassword); /* NULL or empty for no password */

            if (schService) {
                /* Have the service, add a description to the registry. */
                _sntprintf(regPath, 1024, TEXT("SYSTEM\\CurrentControlSet\\Services\\%s"), wrapperData->serviceName);
                if ((wrapperData->serviceDescription != NULL && _tcslen(wrapperData->serviceDescription) > 0)
                        && (RegOpenKeyEx(HKEY_LOCAL_MACHINE, regPath, 0, KEY_WRITE, (PHKEY) &hKey) == ERROR_SUCCESS)) {

                    /* Set Description key in registry */
                    RegSetValueEx(hKey, TEXT("Description"), (DWORD) 0, (DWORD) REG_SZ,
                            (LPBYTE)wrapperData->serviceDescription,
                            (int)(sizeof(TCHAR) * (_tcslen(wrapperData->serviceDescription) + 1)));
                    RegCloseKey(hKey);
                }

                if (result !=1) {
                    /* Service was installed. */
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("%s service installed."),
                            wrapperData->serviceDisplayName);
                }
                /* Close the handle to this service object */
                CloseServiceHandle(schService);
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to install the %s service - %s"),
                        wrapperData->serviceDisplayName, getLastErrorText());
                result = 1;
            }

            /* Close the handle to the service control manager database */
            CloseServiceHandle(schSCManager);
        }
    } else {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to install the %s service - %s"),
                wrapperData->serviceDisplayName, getLastErrorText());
        if (isVista() && !isElevated()) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Performing this action requires that you run as an elevated process."));
        }
        result = 1;
    }

    return result;
}

void closeRegistryKey(HKEY hKey) {
    LONG result;
    LPSTR pBuffer = NULL;

    result = RegCloseKey(hKey);
    if (result != ERROR_SUCCESS) {
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, result, 0, (LPTSTR)&pBuffer, 0, NULL);
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to close the registry: %d : %s"), result, pBuffer);
        LocalFree(pBuffer);
    }
}

/**
 * Sets any environment variables stored in the system registry to the current
 *  environment.  The NT service environment only has access to the environment
 *  variables set when the machine was last rebooted.  This makes it possible
 *  to access the latest values in registry without a reboot.
 *
 * Note that this function is always called before the configuration file has
 *  been loaded this means that any logging that takes place will be sent to
 *  the default log file which may be difficult for the user to locate.
 *
 * Return TRUE if there were any problems.
 */
int wrapperLoadEnvFromRegistryInner(HKEY baseHKey, const TCHAR *regPath, int appendPath, int source) {
    LONG result;
    LPSTR pBuffer = NULL;
    int envCount = 0;
    int ret;
    HKEY hKey;
    DWORD dwIndex;
    DWORD valueCount;
    DWORD maxValueNameLength;
    DWORD maxValueLength;
    TCHAR *valueName;
    TCHAR *value;
    DWORD thisValueNameLength;
    DWORD thisValueLength;
    DWORD thisValueType;
    const TCHAR *oldVal;
    TCHAR *newVal;
    BOOL expanded;

    /* NOTE - Any log output here will be placed in the default log file as it happens
     *        before the wrapper.conf is loaded. */

    /* Open the registry entry where the current environment variables are stored. */
    result = RegOpenKeyEx(baseHKey, regPath, 0, KEY_ENUMERATE_SUB_KEYS | KEY_QUERY_VALUE, (PHKEY)&hKey);
    if (result == ERROR_SUCCESS) {
        /* Read in each of the environment variables and set them into the environment.
         *  These values will be set as is without doing any environment variable
         *  expansion.  In order for the ExpandEnvironmentStrings function to work all
         *  of the environment variables to be replaced must already be set.  To handle
         *  this, after we set the values as is from the registry, we need to go back
         *  through all the ones we set and Expand them if necessary. */

        /* Query the registry to find out how many values there are as well as info about how
         *  large the values names and data are. */
        result = RegQueryInfoKey(hKey, NULL, NULL, NULL, NULL, NULL, NULL, &valueCount, &maxValueNameLength, &maxValueLength, NULL, NULL);
        if (result != ERROR_SUCCESS) {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, result, 0, (LPTSTR)&pBuffer, 0, NULL);
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to query the registry to get the environment: %d : %s"), result, pBuffer);
            LocalFree(pBuffer);
            closeRegistryKey(hKey);
            return TRUE;
        }

#ifdef _DEBUG
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Registry contains %d variables.  Longest name=%d, longest value=%d"), valueCount, maxValueNameLength, maxValueLength);
#endif
        /* Add space for the null. */
        maxValueNameLength++;
        maxValueLength++;

        /* Allocate buffers to get the value names and values from the registry.  These can
         *  be reused because we are using the setEnv function to store the values into the
         *  environment.  setEnv allocates the memory required by the environment. */
        valueName = malloc(sizeof(TCHAR) * maxValueNameLength);
        if (!valueName) {
            outOfMemory(TEXT("WLEFRI"), 1);
            closeRegistryKey(hKey);
            return TRUE;
        }
        value = malloc(sizeof(TCHAR) * maxValueLength);
        if (!valueName) {
            outOfMemory(TEXT("WLEFRI"), 2);
            closeRegistryKey(hKey);
            return TRUE;
        }

        /* Loop over the values and load each of them into the local environment as is. */
        dwIndex = 0;
        do {
            thisValueNameLength = maxValueNameLength;
            thisValueLength = maxValueLength;

            result = RegEnumValue(hKey, dwIndex, valueName, &thisValueNameLength, NULL, &thisValueType, (LPBYTE)value, &thisValueLength);
            if (result == ERROR_SUCCESS) {
                if ((thisValueType == REG_SZ) || (thisValueType = REG_EXPAND_SZ)) {
                    /* Got a value. */
#ifdef _DEBUG
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Loaded var name=\"%s\", value=\"%s\""), valueName, value);
#endif
                    if (appendPath && (strcmpIgnoreCase(TEXT("path"), valueName) == 0)) {
                        /* The PATH variable is special, it needs to be appended to the existing value. */
                        oldVal = _tgetenv(TEXT("PATH"));
                        if (oldVal) {
                            newVal = malloc(sizeof(TCHAR) * (_tcslen(oldVal) + 1 + _tcslen(value) + 1));
                            if (!newVal) {
                                outOfMemory(TEXT("WLEFRI"), 3);
                                closeRegistryKey(hKey);
                                return TRUE;
                            }
                            _sntprintf(newVal, _tcslen(oldVal) + 1 + _tcslen(value) + 1, TEXT("%s;%s"), oldVal, value);
                            if (setEnv(valueName, newVal, source)) {
                                /* Already reported. */
                                free(newVal);
                                closeRegistryKey(hKey);
                                return TRUE;
                            }
#ifdef _DEBUG
                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("    Appended to existing value: %s=%s"), valueName, newVal);
#endif
                            free(newVal);
                        } else {
                            /* Did not exist, set normally. */
                            if (setEnv(valueName, value, source)) {
                                /* Already reported. */
                                closeRegistryKey(hKey);
                                return TRUE;
                            }
                        }
                    } else {
                        if (setEnv(valueName, value, source)) {
                            /* Already reported. */
                            closeRegistryKey(hKey);
                            return TRUE;
                        }
                    }
#ifdef _DEBUG
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Set to local environment."));
#endif
                } else {
#ifdef _DEBUG
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Loaded var name=\"%s\" but type is invalid: %d, skipping."), valueName, thisValueType);
#endif
                }
            } else if (result = ERROR_NO_MORE_ITEMS) {
                /* This means we are at the end.  Fall through. */
            } else {
                FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, result, 0, (LPTSTR)&pBuffer, 0, NULL);
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to query the registry to get environment variable #%d: %d : %s"), dwIndex, result, getLastErrorText());
                LocalFree(pBuffer);
                closeRegistryKey(hKey);
                return TRUE;
            }

            dwIndex++;
        } while (result != ERROR_NO_MORE_ITEMS);

#ifdef _DEBUG
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("All environment variables loaded.  Loop back over them to evaluate any nested variables."));
#endif
        /* Go back and loop over the environment variables we just set and expand any
         *  variables which contain % characters. Loop until we make a pass which does
         *  not perform any replacements. */
        do {
            expanded = FALSE;

            dwIndex = 0;
            do {
                thisValueNameLength = maxValueNameLength;
                result = RegEnumValue(hKey, dwIndex, valueName, &thisValueNameLength, NULL, &thisValueType, NULL, NULL);
                if (result == ERROR_SUCCESS) {
                    /* Found an environment variable in the registry.  Variables that contain references have a different type. */
                    if (thisValueType = REG_EXPAND_SZ) {
#ifdef _DEBUG
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Get the current local value of variable \"%s\""), valueName);
#endif
                        oldVal = _tgetenv(valueName);
                        if (oldVal == NULL) {
#ifdef _DEBUG
                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  The current local value of variable \"%s\" is null, meaning it was not in the registry.  Skipping."), valueName);
#endif
                        } else {
#ifdef _DEBUG
                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("     \"%s\"=\"%s\""), valueName, oldVal);
#endif
                            if (_tcschr(oldVal, TEXT('%'))) {
                                /* This variable contains tokens which need to be expanded. */
                                /* Find out how much space is required to store the expanded value. */
                                ret = ExpandEnvironmentStrings(oldVal, NULL, 0);
                                if (ret == 0) {
                                    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER) {
                                        /* The ExpandEnvironmentStrings function has an internal 32k size limit.  We hit it.
                                         *  All we can do is skip this particular variable by leaving it unexpanded. */
                                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Unable to expand environment variable \"%s\" because the result is larger than the system allowed 32k.  Leaving unexpanded and continuing."), valueName);
                                    } else {
                                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to expand environment variable \"%s\": %s"), valueName, getLastErrorText());
                                        closeRegistryKey(hKey);
                                        return TRUE;
                                    }
                                } else {
                                    /* Allocate a buffer to hold to the expanded value. */
                                    newVal = malloc(sizeof(TCHAR) * (ret + 2));
                                    if (!newVal) {
                                        outOfMemory(TEXT("WLEFRI"), 4);
                                        closeRegistryKey(hKey);
                                        return TRUE;
                                    }

                                    /* Actually expand the variable. */
                                    ret = ExpandEnvironmentStrings(oldVal, newVal, ret + 2);
                                    if (ret == 0) {
                                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to expand environment variable \"%s\" (2): %s"), valueName, getLastErrorText());
                                        free(newVal);
                                        closeRegistryKey(hKey);
                                        return TRUE;
                                    }

                                    /* Was anything changed? */
                                    if (_tcscmp(oldVal, newVal) == 0) {
#ifdef _DEBUG
                                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("       Value unchanged.  Referenced environment variable not set."));
#endif
                                    } else {
                                        /* Set the expanded environment variable */
                                        expanded = TRUE;
#ifdef _DEBUG
                                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Update local environment variable.  \"%s\"=\"%s\""), valueName, newVal);
#endif

                                        /* Update the environment. */
                                        if (setEnv(valueName, newVal, source)) {
                                            /* Already reported. */
                                            free(newVal);
                                            closeRegistryKey(hKey);
                                            return TRUE;
                                        }
                                    }
                                    free(newVal);
                                }
                            }
                        }
                    }
                } else if (result == ERROR_NO_MORE_ITEMS) {
                    /* No more environment variables. */
                } else {
                    FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, result, 0, (LPTSTR)&pBuffer, 0, NULL);
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to read registry - %s"), getLastErrorText());
                    LocalFree(pBuffer);
                    closeRegistryKey(hKey);
                    return TRUE;
                }
                dwIndex++;
            } while (result != ERROR_NO_MORE_ITEMS);

#ifdef _DEBUG
            if (expanded) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Rescan environment variables to varify that there are no more expansions necessary."));
            }
#endif
        } while (expanded);

#ifdef _DEBUG
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Done loading environment variables."));
#endif

        /* Close the registry entry */
        closeRegistryKey(hKey);
    } else {
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, result, 0, (LPTSTR)&pBuffer, 0, NULL);
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to access registry to obtain environment variables - %s"), getLastErrorText());
        LocalFree(pBuffer);
        return TRUE;
    }

    return FALSE;
}

/**
 * Loads the environment stored in the registry.
 *
 * (Only called for versions of Windows older than XP or 2003.)
 *
 * Return TRUE if there were any problems.
 */
int wrapperLoadEnvFromRegistry() {
    /* We can't access any properties here as they are not yet loaded when called. */
    /* Always load in the system wide variables. */
#ifdef _DEBUG
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Loading System environment variables from Registry:"));
#endif

    if (wrapperLoadEnvFromRegistryInner(HKEY_LOCAL_MACHINE, TEXT("SYSTEM\\CurrentControlSet\\Control\\Session Manager\\Environment\\"), FALSE, ENV_SOURCE_REG_SYSTEM)) {
        return TRUE;
    }

    /* Only load in the user specific variables if the USERNAME environment variable is set. */
    if (_tgetenv(TEXT("USERNAME"))) {
#ifdef _DEBUG
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Loading Account environment variables from Registry:"));
#endif

        if (wrapperLoadEnvFromRegistryInner(HKEY_CURRENT_USER, TEXT("Environment\\"), TRUE, ENV_SOURCE_REG_ACCOUNT)){
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * Gets the JavaHome absolute path from the windows registry
 */
int wrapperGetJavaHomeFromWindowsRegistry(TCHAR *javaHome) {
    LONG result;
    LPSTR pBuffer = NULL;
    const TCHAR *prop;
    TCHAR *c;
    TCHAR subKey[512];       /* Registry subkey that jvm creates when is installed */
    TCHAR *valueKey;
    TCHAR jreversion[10];    /* Will receive a registry value that has jvm version */
    HKEY baseHKey;
    HKEY openHKey = NULL; /* Will receive the handle to the opened registry key */
    DWORD valueType;
    DWORD valueSize;
    TCHAR *value;

    prop = getStringProperty(properties, TEXT("wrapper.registry.java_home"), NULL);
    if (prop) {
        /* A registry location was specified. */
        if (_tcsstr(prop, TEXT("HKEY_CLASSES_ROOT\\")) == prop) {
            baseHKey = HKEY_CLASSES_ROOT;
            _tcsncpy(subKey, prop + 18, 512);
        } else if (_tcsstr(prop, TEXT("HKEY_CURRENT_CONFIG\\")) == prop) {
            baseHKey = HKEY_CURRENT_USER;
            _tcsncpy(subKey, prop + 20, 512);
        } else if (_tcsstr(prop, TEXT("HKEY_CURRENT_USER\\")) == prop) {
            baseHKey = HKEY_CURRENT_USER;
            _tcsncpy(subKey, prop + 18, 512);
        } else if (_tcsstr(prop, TEXT("HKEY_LOCAL_MACHINE\\")) == prop) {
            baseHKey = HKEY_LOCAL_MACHINE;
            _tcsncpy(subKey, prop + 19, 512);
        } else if (_tcsstr(prop, TEXT("HKEY_USERS\\")) == prop) {
            baseHKey = HKEY_USERS;
            _tcsncpy(subKey, prop + 11, 512);
        } else {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("wrapper.registry.java_home does not begin with a known root key: %s"), prop);
            return 0;
        }

        /* log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("subKey=%s"), subKey); */

        /* We need to split the value from the key.  Find the last \ */
        c = _tcsrchr(subKey, TEXT('\\'));
        if (!c) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("wrapper.registry.java_home is an invalid key: %s"), prop);
            return 0;
        }
        valueKey = c + 1;
        /* Truncate the subKey. */
        *c = TEXT('\0');

        /*log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("subKey=%s valueKey=%s"), subKey, valueKey); */

        /*
         * Opens the Registry Key needed to query the jvm version
         */
        result = RegOpenKeyEx(baseHKey, subKey, 0, KEY_QUERY_VALUE, &openHKey);
        if (result != ERROR_SUCCESS) {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, result, 0, (LPTSTR)&pBuffer, 0, NULL);
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("Unable to access configured registry location for JAVA_HOME: %s - (%d)"), subKey, errno);
            LocalFree(pBuffer);
            return 0;
        }

        result = RegQueryValueEx(openHKey, valueKey, NULL, &valueType, NULL, &valueSize);
        if (result != ERROR_SUCCESS) {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, result, 0, (LPTSTR)&pBuffer, 0, NULL);
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("Unable to access configured registry location for JAVA_HOME: %s - (%d)"), prop, errno);
            LocalFree(pBuffer);
            closeRegistryKey(openHKey);
            return 0;
        }
        if (valueType != REG_SZ) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("Configured JAVA_HOME registry location is not of type REG_SZ: %s"), prop);
            closeRegistryKey(openHKey);
            return 0;
        }
        value = malloc(sizeof(TCHAR) * valueSize);
        if (!value) {
            outOfMemory(TEXT("WGJFWR"), 1);
            closeRegistryKey(openHKey);
            return 0;
        }
        result = RegQueryValueEx(openHKey, valueKey, NULL, &valueType, (LPBYTE)value, &valueSize);
        if (result != ERROR_SUCCESS) {
            FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, result, 0, (LPTSTR)&pBuffer, 0, NULL);
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                TEXT("Unable to access configured registry location %s - (%d)"), prop, errno);
            LocalFree(pBuffer);
            free(value);
            closeRegistryKey(openHKey);
            return 0;
        }

        closeRegistryKey(openHKey);

        /* Returns the JavaHome path */
        _tcsncpy(javaHome, value, 512);

        free(value);
        return 1;
    } else {
        /* Look for the java_home in the default location. */

        /* SubKey containing the jvm version */
        _tcsncpy(subKey, TEXT("SOFTWARE\\JavaSoft\\Java Runtime Environment"), 512);

        /*
         * Opens the Registry Key needed to query the jvm version
         */
        result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, subKey, 0, KEY_QUERY_VALUE, &openHKey);
        if (result != ERROR_SUCCESS) {
            /* Not found.  continue. */
            return 0;
        }

        /*
         * Queries for the jvm version
         */

        valueSize = sizeof(jreversion);
        result = RegQueryValueEx(openHKey, TEXT("CurrentVersion"), NULL, &valueType, (LPBYTE)jreversion, &valueSize);
        if (result != ERROR_SUCCESS) {
            closeRegistryKey(openHKey);
            return 0;
        }

        closeRegistryKey(openHKey);

        /* adds the jvm version to the subkey */
        _tcsncat(subKey, TEXT("\\"), 512);
        _tcsncat(subKey, jreversion, 512);

        /*
         * Opens the Registry Key needed to query the JavaHome
         */
        result = RegOpenKeyEx(HKEY_LOCAL_MACHINE, subKey, 0, KEY_QUERY_VALUE, &openHKey);
        if (result != ERROR_SUCCESS) {
            return 0;
        }

        /*
         * Queries for the JavaHome
         */
        result = RegQueryValueEx(openHKey, TEXT("JavaHome"), NULL, &valueType, NULL, &valueSize);
        if (result != ERROR_SUCCESS) {
            closeRegistryKey(openHKey);
            return 0;
        }
        value = malloc(sizeof(TCHAR) * valueSize);
        if (!value) {
            outOfMemory(TEXT("WGJFWR"), 2);
            closeRegistryKey(openHKey);
            return 0;
        }
        result = RegQueryValueEx(openHKey, TEXT("JavaHome"), NULL, &valueType, (LPBYTE)value, &valueSize);
        if (result != ERROR_SUCCESS) {
            closeRegistryKey(openHKey);
            return 0;
        }

        closeRegistryKey(openHKey);

        /* Returns the JavaHome path */
        _tcsncpy(javaHome, value, 512);
        free(value);

        return 1;
    }
}

TCHAR *getNTServiceStatusName(int status) {
    TCHAR *name;
    switch(status) {
    case SERVICE_STOPPED:
        name = TEXT("STOPPED");
        break;
    case SERVICE_START_PENDING:
        name = TEXT("START_PENDING");
        break;
    case SERVICE_STOP_PENDING:
        name = TEXT("STOP_PENDING");
        break;
    case SERVICE_RUNNING:
        name = TEXT("RUNNING");
        break;
    case SERVICE_CONTINUE_PENDING:
        name = TEXT("CONTINUE_PENDING");
        break;
    case SERVICE_PAUSE_PENDING:
        name = TEXT("PAUSE_PENDING");
        break;
    case SERVICE_PAUSED:
        name = TEXT("PAUSED");
        break;
    default:
        name = TEXT("UNKNOWN");
        break;
    }
    return name;
}

/** Starts a Wrapper instance running as an NT Service. */
int wrapperStartService() {
    SC_HANDLE   schService;
    SC_HANDLE   schSCManager;
    SERVICE_STATUS serviceStatus;
    const TCHAR *logfilePath;
    TCHAR fullPath[FILEPATHSIZE] = TEXT("");

    TCHAR *status;
    int msgCntr;
    int stopping;
    int result;

    logfilePath = getLogfilePath();
    result = GetFullPathName(logfilePath, FILEPATHSIZE, fullPath, NULL);
    if (result >= FILEPATHSIZE) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("The full path of %s is too large. (%d)"), logfilePath, result);
        _tcsncpy(fullPath, logfilePath, FILEPATHSIZE);
    } else if (result == 0) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Unable to resolve the full path of %s : %s"), logfilePath, getLastErrorText());
        _tcsncpy(fullPath, logfilePath, FILEPATHSIZE);
    }

    result = 0;

    /* First, get a handle to the service control manager */
    schSCManager = OpenSCManager(NULL,
                                 NULL,
                                 SC_MANAGER_CONNECT);
    if (schSCManager) {
        /* Next get the handle to this service... */
        schService = OpenService(schSCManager, wrapperData->serviceName, SERVICE_QUERY_STATUS | SERVICE_START);

        if (schService) {
            /* Make sure that the service is not already running. */
            if (QueryServiceStatus(schService, &serviceStatus)) {
                if (serviceStatus.dwCurrentState == SERVICE_STOPPED) {
                    /* The service is stopped, so try starting it. */
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Starting the %s service..."),
                        wrapperData->serviceDisplayName);
                    if (StartService(schService, 0, NULL)) {
                        /* We will get here immediately if the service process was launched.
                         *  We still need to wait for it to actually start. */
                        msgCntr = 0;
                        stopping = FALSE;
                        do {
                            if (QueryServiceStatus(schService, &serviceStatus)) {
                                if (serviceStatus.dwCurrentState == SERVICE_STOP_PENDING) {
                                    if (!stopping) {
                                        stopping = TRUE;
                                        msgCntr = 5; /* Trigger a message */
                                    }
                                    if (msgCntr >= 5) {
                                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("Stopping..."));
                                        msgCntr = 0;
                                    }
                                } else {
                                    if (msgCntr >= 5) {
                                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("Waiting to start..."));
                                        msgCntr = 0;
                                    }
                                }
                                wrapperSleep(1000);
                                msgCntr++;
                            } else {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                                    TEXT("Unable to query the status of the %s service - %s"),
                                    wrapperData->serviceDisplayName, getLastErrorText());
                                result = 1;
                                break;
                            }
                        } while ((serviceStatus.dwCurrentState != SERVICE_STOPPED)
                            && (serviceStatus.dwCurrentState != SERVICE_RUNNING)
                            && (serviceStatus.dwCurrentState != SERVICE_PAUSED));

                        /* Was the service started? */
                        if (serviceStatus.dwCurrentState == SERVICE_RUNNING) {
                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("%s started."), wrapperData->serviceDisplayName);
                        } else if (serviceStatus.dwCurrentState == SERVICE_PAUSED) {
                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("%s started but immediately paused.."), wrapperData->serviceDisplayName);
                        } else {
                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("The %s service was launched, but failed to start."),
                                wrapperData->serviceDisplayName);
                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Please check the log file more information: %s"), fullPath);
                            result = 1;
                        }
                    } else {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to start the %s service - %s"),
                            wrapperData->serviceDisplayName, getLastErrorText());
                        result = 1;
                    }
                } else {
                    status = getNTServiceStatusName(serviceStatus.dwCurrentState);
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("The %s service is already running with status: %s"),
                        wrapperData->serviceDisplayName, status);
                    result = 1;
                }
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to query the status of the %s service - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
                result = 1;
            }

            /* Close this service object's handle to the service control manager */
            CloseServiceHandle(schService);
        } else {
            if (GetLastError() == ERROR_ACCESS_DENIED) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to start the %s service - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
                if (isVista() && !isElevated()) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Performing this action requires that you run as an elevated process."));
                }
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("The %s service is not installed - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
            }
            result = 1;
        }

        /* Finally, close the handle to the service control manager's database */
        CloseServiceHandle(schSCManager);
    } else {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to start the %s service - %s"),
            wrapperData->serviceDisplayName, getLastErrorText());
        result = 1;
    }

    return result;
}

/** Stops a Wrapper instance running as an NT Service. */
int wrapperStopService(int command) {
    SC_HANDLE   schService;
    SC_HANDLE   schSCManager;
    SERVICE_STATUS serviceStatus;

    TCHAR *status;
    int msgCntr;
    int result = 0;

    /* First, get a handle to the service control manager */
    schSCManager = OpenSCManager(NULL,
                                 NULL,
                                 SC_MANAGER_CONNECT);
    if (schSCManager) {

        /* Next get the handle to this service... */
        schService = OpenService(schSCManager, wrapperData->serviceName, SERVICE_QUERY_STATUS | SERVICE_STOP);

        if (schService) {
            /* Find out what the current status of the service is so we can decide what to do. */
            if (QueryServiceStatus(schService, &serviceStatus)) {
                if (serviceStatus.dwCurrentState == SERVICE_STOPPED) {
                    if (command) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("The %s service was not running."),
                            wrapperData->serviceDisplayName);
                    }
                } else {
                    if (serviceStatus.dwCurrentState == SERVICE_STOP_PENDING) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                            TEXT("The %s service was already in the process of stopping."),
                            wrapperData->serviceDisplayName);
                    } else {
                        /* Stop the service. */
                        if (ControlService(schService, SERVICE_CONTROL_STOP, &serviceStatus)) {
                            if (command) {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Stopping the %s service..."),
                                    wrapperData->serviceDisplayName);
                            } else {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Service is running.  Stopping it..."));
                            }
                        } else {
                            if (serviceStatus.dwCurrentState == SERVICE_START_PENDING) {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                                    TEXT("The %s service was in the process of starting.  Stopping it..."),
                                    wrapperData->serviceDisplayName);
                            } else {
                                status = getNTServiceStatusName(serviceStatus.dwCurrentState);
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                                    TEXT("Attempt to stop the %s service failed.  Status: %s"),
                                    wrapperData->serviceDisplayName, status);
                                result = 1;
                            }
                        }
                    }
                    if (result == 0) {
                        /* Wait for the service to stop. */
                        msgCntr = 0;
                        do {
                            if (QueryServiceStatus(schService, &serviceStatus)) {
                                if (msgCntr >= 5) {
                                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("Waiting to stop..."));
                                    msgCntr = 0;
                                }
                                wrapperSleep(1000);
                                msgCntr++;
                            } else {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                                    TEXT("Unable to query the status of the %s service - %s"),
                                    wrapperData->serviceDisplayName, getLastErrorText());
                                result = 1;
                                break;
                            }
                        } while (serviceStatus.dwCurrentState != SERVICE_STOPPED);

                        if (serviceStatus.dwCurrentState == SERVICE_STOPPED) {
                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("%s stopped."), wrapperData->serviceDisplayName);
                        } else {
                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Failed to stop the %s service."), wrapperData->serviceDisplayName);
                            result = 1;
                        }
                    }
                }
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to query the status of the %s service - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
                result = 1;
            }

            /* Close this service object's handle to the service control manager */
            CloseServiceHandle(schService);
        } else {
            if (GetLastError() == ERROR_ACCESS_DENIED) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to stop the %s service - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
                if (isVista() && !isElevated()) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Performing this action requires that you run as an elevated process."));
                }
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("The %s service is not installed - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
            }
            result = 1;
        }

        /* Finally, close the handle to the service control manager's database */
        CloseServiceHandle(schSCManager);
    } else {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to stop the %s service - %s"),
            wrapperData->serviceDisplayName, getLastErrorText());
        result = 1;
    }

    return result;
}

/** Pauses a Wrapper instance running as an NT Service. */
int wrapperPauseService() {
    SC_HANDLE   schService;
    SC_HANDLE   schSCManager;
    SERVICE_STATUS serviceStatus;

    TCHAR *status;
    int msgCntr;
    int result = 0;

    /* First, get a handle to the service control manager */
    schSCManager = OpenSCManager(NULL,
                                 NULL,
                                 SC_MANAGER_CONNECT);
    if (schSCManager) {
        /* Next get the handle to this service... */
        schService = OpenService(schSCManager, wrapperData->serviceName, SERVICE_QUERY_STATUS | SERVICE_PAUSE_CONTINUE);

        if (schService) {
            /* Make sure that the service is in a state that can be paused. */
            if (QueryServiceStatus(schService, &serviceStatus)) {
                if (serviceStatus.dwCurrentState == SERVICE_STOPPED) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("The %s service was not running."),
                        wrapperData->serviceDisplayName);
                    result = 1;
                } else if (serviceStatus.dwCurrentState == SERVICE_STOP_PENDING) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("The %s service was in the process of stopping."),
                        wrapperData->serviceDisplayName);
                    result = 1;
                } else if (serviceStatus.dwCurrentState == SERVICE_PAUSED) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("The %s service was already paused."),
                        wrapperData->serviceDisplayName);
                } else if (serviceStatus.dwCurrentState == SERVICE_PAUSE_PENDING) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("The %s service was in the process of being paused."),
                        wrapperData->serviceDisplayName);
                } else {
                    /* The service is started, starting, or resuming, so try pausing it. */
                    if (ControlService(schService, SERVICE_CONTROL_PAUSE, &serviceStatus)) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Pausing the %s service..."),
                            wrapperData->serviceDisplayName);
                    } else {
                        status = getNTServiceStatusName(serviceStatus.dwCurrentState);
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                            TEXT("Attempt to pause the %s service failed.  Status: %s"),
                            wrapperData->serviceDisplayName, status);
                        result = 1;
                    }
                }
                if (result == 0) {
                    /* Wait for the service to pause. */
                    msgCntr = 0;
                    do {
                        if (QueryServiceStatus(schService, &serviceStatus)) {
                            if (msgCntr >= 5) {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("Waiting to pause..."));
                                msgCntr = 0;
                            }
                            wrapperSleep(1000);
                            msgCntr++;
                        } else {
                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                                TEXT("Unable to query the status of the %s service - %s"),
                                wrapperData->serviceDisplayName, getLastErrorText());
                            result = 1;
                            break;
                        }
                    } while (!((serviceStatus.dwCurrentState == SERVICE_PAUSED) || (serviceStatus.dwCurrentState == SERVICE_STOPPED)));

                    if (serviceStatus.dwCurrentState == SERVICE_PAUSED) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("%s service paused."), wrapperData->serviceDisplayName);
                    } else {
                        status = getNTServiceStatusName(serviceStatus.dwCurrentState);
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT(
                            "Failed to pause %s service.  Status: %s"),
                            wrapperData->serviceDisplayName, status);
                        result = 1;
                    }
                }
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to query the status of the %s service - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
                result = 1;
            }

            /* Close this service object's handle to the service control manager */
            CloseServiceHandle(schService);
        } else {
            if (GetLastError() == ERROR_ACCESS_DENIED) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to pause the %s service - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("The %s service is not installed - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
            }
            result = 1;
        }

        /* Finally, close the handle to the service control manager's database */
        CloseServiceHandle(schSCManager);
    } else {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to pause the %s service - %s"),
            wrapperData->serviceDisplayName, getLastErrorText());
        if (isVista() && !isElevated()) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Performing this action requires that you run as an elevated process."));
        }
        result = 1;
    }

    return result;
}

/** Resume a Wrapper instance running as an NT Service. */
int wrapperResumeService() {
    SC_HANDLE   schService;
    SC_HANDLE   schSCManager;
    SERVICE_STATUS serviceStatus;

    TCHAR *status;
    int msgCntr;
    int result = 0;

    /* First, get a handle to the service control manager */
    schSCManager = OpenSCManager(NULL,
                                 NULL,
                                 SC_MANAGER_CONNECT);
    if (schSCManager) {
        /* Next get the handle to this service... */
        schService = OpenService(schSCManager, wrapperData->serviceName, SERVICE_QUERY_STATUS | SERVICE_PAUSE_CONTINUE);

        if (schService) {
            /* Make sure that the service is in a state that can be resumed. */
            if (QueryServiceStatus(schService, &serviceStatus)) {
                if (serviceStatus.dwCurrentState == SERVICE_STOPPED) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("The %s service was not running."),
                        wrapperData->serviceDisplayName);
                    result = 1;
                } else if (serviceStatus.dwCurrentState == SERVICE_STOP_PENDING) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("The %s service was in the process of stopping."),
                        wrapperData->serviceDisplayName);
                    result = 1;
                } else if (serviceStatus.dwCurrentState == SERVICE_PAUSE_PENDING) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("The %s service was in the process of being paused."),
                        wrapperData->serviceDisplayName);
                    result = 1;
                } else if (serviceStatus.dwCurrentState == SERVICE_CONTINUE_PENDING) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("The %s service was in the process of being resumed."),
                        wrapperData->serviceDisplayName);
                } else if (serviceStatus.dwCurrentState == SERVICE_RUNNING) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("The %s service was already started."),
                        wrapperData->serviceDisplayName);
                } else {
                    /* The service is paused, so try resuming it. */
                    if (ControlService(schService, SERVICE_CONTROL_CONTINUE, &serviceStatus)) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Resuming the %s service..."),
                            wrapperData->serviceDisplayName);
                    } else {
                        status = getNTServiceStatusName(serviceStatus.dwCurrentState);
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                            TEXT("Attempt to resume the %s service failed.  Status: %s"),
                            wrapperData->serviceDisplayName, status);
                        result = 1;
                    }
                }
                if (result == 0) {
                    /* Wait for the service to resume. */
                    msgCntr = 0;
                    do {
                        if (QueryServiceStatus(schService, &serviceStatus)) {
                            if (msgCntr >= 5) {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_INFO, TEXT("Waiting to resume..."));
                                msgCntr = 0;
                            }
                            wrapperSleep(1000);
                            msgCntr++;
                        } else {
                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                                TEXT("Unable to query the status of the %s service - %s"),
                                wrapperData->serviceDisplayName, getLastErrorText());
                            result = 1;
                            break;
                        }
                    } while (!((serviceStatus.dwCurrentState == SERVICE_RUNNING) || (serviceStatus.dwCurrentState == SERVICE_STOPPED)));

                    if (serviceStatus.dwCurrentState == SERVICE_RUNNING) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("%s service resumed."), wrapperData->serviceDisplayName);
                    } else {
                        status = getNTServiceStatusName(serviceStatus.dwCurrentState);
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT(
                            "Failed to resume %s service.  Status: %s"),
                            wrapperData->serviceDisplayName, status);
                        result = 1;
                    }
                }
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to query the status of the %s service - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
                result = 1;
            }

            /* Close this service object's handle to the service control manager */
            CloseServiceHandle(schService);
        } else {
            if (GetLastError() == ERROR_ACCESS_DENIED) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to resume the %s service - %s"),
                wrapperData->serviceDisplayName, getLastErrorText());
                if (isVista() && !isElevated()) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Performing this action requires that you run as an elevated process."));
                }
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("The %s service is not installed - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
            }
            result = 1;
        }

        /* Finally, close the handle to the service control manager's database */
        CloseServiceHandle(schSCManager);
    } else {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to resume the %s service - %s"),
            wrapperData->serviceDisplayName, getLastErrorText());
        result = 1;
    }

    return result;
}

int sendServiceControlCodeInner(int controlCode) {
    SC_HANDLE   schService;
    SC_HANDLE   schSCManager;
    SERVICE_STATUS serviceStatus;
    TCHAR *status;
    int result = 0;

    /* First, get a handle to the service control manager */
    schSCManager = OpenSCManager(NULL,
                                 NULL,
                                 SC_MANAGER_CONNECT);
    if (schSCManager) {
        /* Next get the handle to this service... */
        schService = OpenService(schSCManager, wrapperData->serviceName, SERVICE_QUERY_STATUS | SERVICE_USER_DEFINED_CONTROL);

        if (schService) {
            /* Make sure that the service is in a state that can be resumed. */
            if (QueryServiceStatus(schService, &serviceStatus)) {
                if (serviceStatus.dwCurrentState == SERVICE_STOPPED) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("The %s service was not running."),
                        wrapperData->serviceDisplayName);
                    result = 1;
                } else if (serviceStatus.dwCurrentState == SERVICE_STOP_PENDING) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("The %s service was in the process of stopping."),
                        wrapperData->serviceDisplayName);
                    result = 1;
                } else if (serviceStatus.dwCurrentState == SERVICE_PAUSED) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("The %s service was currently paused."),
                        wrapperData->serviceDisplayName);
                    result = 1;
                } else if (serviceStatus.dwCurrentState == SERVICE_PAUSE_PENDING) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("The %s service was in the process of being paused."),
                        wrapperData->serviceDisplayName);
                    result = 1;
                } else if (serviceStatus.dwCurrentState == SERVICE_CONTINUE_PENDING) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("The %s service was in the process of being resumed."),
                        wrapperData->serviceDisplayName);
                } else {
                    /* The service is running, so try sending the code. */
                    if (ControlService(schService, controlCode, &serviceStatus)) {
                        result = 0;
                    } else {
                        status = getNTServiceStatusName(serviceStatus.dwCurrentState);
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR,
                            TEXT("Attempt to send the %s service control code %d failed.  Status: %s"),
                            wrapperData->serviceDisplayName, controlCode, status);
                        result = 1;
                    }
                }
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to query the status of the %s service - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
                result = 1;
            }

            /* Close this service object's handle to the service control manager */
            CloseServiceHandle(schService);
        } else {
            if (GetLastError() == ERROR_ACCESS_DENIED) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to send control code to the %s service - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("OpenService failed - %s"), getLastErrorText());
                if (isVista() && !isElevated()) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Performing this action requires that you run as an elevated process."));
                }
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("The %s service is not installed - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
            }
            result = 1;
        }

        /* Finally, close the handle to the service control manager's database */
        CloseServiceHandle(schSCManager);
    } else {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to send control code to the %s service - %s"),
            wrapperData->serviceDisplayName, getLastErrorText());
        result = 1;
    }

    return result;
}

/** Sends a service control code to a running as an NT Service. */
int wrapperSendServiceControlCode(TCHAR **argv, TCHAR *controlCodeS) {
    int controlCode;
    int result;

    /* Make sure the control code is valid. */
    if (controlCodeS == NULL) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Control code to send is missing."));
        wrapperUsage(argv[0]);
        return 1;
    }
    controlCode = _ttoi(controlCodeS);
    if ((controlCode < 128) || (controlCode > 255)) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("The service control code must be in the range 128-255."));
        return 1;
    }

    result = sendServiceControlCodeInner(controlCode);
    if (!result) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Sent the %s service control code %d."),
            wrapperData->serviceDisplayName, controlCode);
    }

    return result;
}

/**
 * Requests that the Wrapper perform a thread dump.
 */
int wrapperRequestThreadDump() {
    int result;

    if (wrapperData->threadDumpControlCode <= 0) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("The thread dump control code is disabled."));
        return 1;
    }

    result = sendServiceControlCodeInner(wrapperData->threadDumpControlCode);
    if (!result) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Requested that the %s service perform a thread dump."),
            wrapperData->serviceDisplayName);
    }

    return result;
}

/**
 * Obtains the current service status.
 * The returned result becomes the exitCode.  The exitCode is made up of
 *  a series of status bits:
 *
 * Bits:
 * 0: Service Installed. (1)
 * 1: Service Running. (2)
 * 2: Service Interactive. (4)
 * 3: Startup Mode: Auto. (8)
 * 4: Startup Mode: Manual. (16)
 * 5: Startup Mode: Disabled. (32)
 * 6: Service Running but Paused. (64)
 */
int wrapperServiceStatus(int consoleOutput) {
    SC_HANDLE   schService;
    SC_HANDLE   schSCManager;
    SERVICE_STATUS serviceStatus;
    QUERY_SERVICE_CONFIG *pQueryServiceConfig;
    DWORD reqSize;

    int result = 0;

    schSCManager = OpenSCManager(NULL,
                                 NULL,
                                 SC_MANAGER_CONNECT);
    if (schSCManager) {

        /* Next get the handle to this service... */
        schService = OpenService(schSCManager, wrapperData->serviceName, SERVICE_QUERY_STATUS | SERVICE_QUERY_CONFIG);

        if (schService) {
            /* Service is installed, so set that bit. */
            if (consoleOutput) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                    TEXT("The %s Service is installed."), wrapperData->serviceDisplayName);
            }
            result |= 1;

            /* Get the service configuration. */
            QueryServiceConfig(schService, NULL, 0, &reqSize);
            pQueryServiceConfig = malloc(reqSize);
            if (!pQueryServiceConfig) {
                outOfMemory(TEXT("WSS"), 1);
                CloseServiceHandle(schSCManager);
                return 0;
            }
            if (QueryServiceConfig(schService, pQueryServiceConfig, reqSize, &reqSize)) {
                switch (pQueryServiceConfig->dwStartType) {
                case SERVICE_BOOT_START:   /* Possible? */
                case SERVICE_SYSTEM_START: /* Possible? */
                case SERVICE_AUTO_START:
                    if (consoleOutput) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Start Type: Automatic"));
                    }
                    result |= 8;
                    break;

                case SERVICE_DEMAND_START:
                    if (consoleOutput) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Start Type: Manual"));
                    }
                    result |= 16;
                    break;

                case SERVICE_DISABLED:
                    if (consoleOutput) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Start Type: Disabled"));
                    }
                    result |= 32;
                    break;

                default:
                    if (consoleOutput) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("  Start Type: Unknown"));
                    }
                    break;
                }

                if (pQueryServiceConfig->dwServiceType & SERVICE_INTERACTIVE_PROCESS) {
                    /* This is an interactive service, so set that bit. */
                    if (consoleOutput) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Interactive: Yes"));
                    }
                    result |= 4;
                } else {
                    if (consoleOutput) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Interactive: No"));
                    }
                }

                free(pQueryServiceConfig);
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to query the configuration of the %s service - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
            }

            /* Find out what the current status of the service is so we can decide what to do. */
            if (QueryServiceStatus(schService, &serviceStatus)) {
                if (serviceStatus.dwCurrentState == SERVICE_STOPPED) {
                    /* The service is stopped. */
                    if (consoleOutput) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Running: No"));
                    }
                } else {
                    /* Any other state, it is running. Set that bit. */
                    if (consoleOutput) {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Running: Yes"));
                    }
                    result |= 2;

                    if (serviceStatus.dwCurrentState == SERVICE_PAUSED) {
                        if (consoleOutput) {
                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("  Paused: Yes"));
                        }
                        result |= 64;
                    }
                }

            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to query the status of the %s service - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
            }

            /* Close this service object's handle to the service control manager */
            CloseServiceHandle(schService);
        } else {
            if (GetLastError() == ERROR_ACCESS_DENIED) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to query the status of the %s service - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
                if (isVista() && !isElevated()) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Performing this action requires that you run as an elevated process."));
                }
            } else {
                if (consoleOutput) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS,
                        TEXT("The %s Service is not installed."), wrapperData->serviceDisplayName);
                }
            }
        }

        /* Finally, close the handle to the service control manager's database */
        CloseServiceHandle(schSCManager);
    } else {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to query the status of the %s service - %s"),
            wrapperData->serviceDisplayName, getLastErrorText());
    }

    return result;
}

/**
 * Uninstall the service and clean up
 */
int wrapperRemove() {
    SC_HANDLE   schService;
    SC_HANDLE   schSCManager;

    int result = 0;

    /* First attempt to stop the service if it is already running. */
    result = wrapperStopService(FALSE);
    if (result) {
        /* There was a problem stopping the service. */
        return result;
    }

    /* First, get a handle to the service control manager */
    schSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_CONNECT);
    if (schSCManager) {

        /* Next get the handle to this service... */
        schService = OpenService(schSCManager, wrapperData->serviceName, SERVICE_QUERY_STATUS | DELETE);

        if (schService) {
            /* Now try to remove the service... */
            if (DeleteService(schService)) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("%s service removed."), wrapperData->serviceDisplayName);
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to remove the %s service - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
                result = 1;
            }

            /* Close this service object's handle to the service control manager */
            CloseServiceHandle(schService);
        } else {
            if (GetLastError() == ERROR_ACCESS_DENIED) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to remove the %s service - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
                if (isVista() && !isElevated()) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Performing this action requires that you run as an elevated process."));
                }
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("The %s service is not installed - %s"),
                    wrapperData->serviceDisplayName, getLastErrorText());
            }
            result = 1;
        }

        /* Finally, close the handle to the service control manager's database */
        CloseServiceHandle(schSCManager);
    } else {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Unable to remove the %s service - %s"),
            wrapperData->serviceDisplayName, getLastErrorText());
        result = 1;
    }

    /* Remove message file registration on service remove */
    if (result == 0) {
        /* Do this here to unregister the syslog on uninstall of a resource. */
        /* unregisterSyslogMessageFile(); */
    }
    return result;
}

/**
 * Sets the working directory to that of the current executable
 */
int setWorkingDir() {
    int size = 128;
    TCHAR* szPath = NULL;
    DWORD usedLen;
    int result;
    TCHAR* pos;

    /* How large a buffer is needed? The GetModuleFileName function doesn't tell us how much
     *  is needed, only if it is too short. */
    do {
        szPath = malloc(sizeof(TCHAR) * size);
        if (!szPath) {
            outOfMemory(TEXT("SWD"), 1);
            return 1;
        }
        usedLen = GetModuleFileName(NULL, szPath, size);
        if (usedLen == 0) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to get the path-%s"), getLastErrorText());
            return 1;
        } else if ((usedLen == size) || (getLastError() == ERROR_INSUFFICIENT_BUFFER)) {
            /* Too small. */
            size += 128;
            free(szPath);
            szPath = NULL;
        }
    } while (!szPath);

    /* The wrapperData->isDebugging flag will never be set here, so we can't really use it. */
#ifdef _DEBUG
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("Executable Name: %s"), szPath);
#endif
    /* To get the path, strip everything off after the last '\' */
    pos = _tcsrchr(szPath, TEXT('\\'));
    if (pos == NULL) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Unable to extract path from: %s"), szPath);
        free(szPath);
        return 1;
    } else {
        /* Clip the path at the position of the last backslash */
        pos[0] = (TCHAR)0;
    }
    /* Set a variable to the location of the binary. */
    setEnv(TEXT("WRAPPER_BIN_DIR"), szPath, ENV_SOURCE_WRAPPER);
    result = wrapperSetWorkingDir(szPath);
    free(szPath);
    return result;
}

/******************************************************************************
 * Main function
 *****************************************************************************/

/** Attempts to resolve the name of an exception.  Returns null if it is unknown. */
TCHAR* getExceptionName(DWORD exCode) {
    TCHAR *exName;

    switch (exCode) {
    case EXCEPTION_ACCESS_VIOLATION:
        exName = TEXT("EXCEPTION_ACCESS_VIOLATION");
        break;
    case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:
        exName = TEXT("EXCEPTION_ARRAY_BOUNDS_EXCEEDED");
        break;
    case EXCEPTION_BREAKPOINT:
        exName = TEXT("EXCEPTION_BREAKPOINT");
        break;
    case EXCEPTION_DATATYPE_MISALIGNMENT:
        exName = TEXT("EXCEPTION_DATATYPE_MISALIGNMENT");
        break;
    case EXCEPTION_FLT_DENORMAL_OPERAND:
        exName = TEXT("EXCEPTION_FLT_DENORMAL_OPERAND");
        break;
    case EXCEPTION_FLT_DIVIDE_BY_ZERO:
        exName = TEXT("EXCEPTION_FLT_DIVIDE_BY_ZERO");
        break;
    case EXCEPTION_FLT_INEXACT_RESULT:
        exName = TEXT("EXCEPTION_FLT_INEXACT_RESULT");
        break;
    case EXCEPTION_FLT_INVALID_OPERATION:
        exName = TEXT("EXCEPTION_FLT_INVALID_OPERATION");
        break;
    case EXCEPTION_FLT_OVERFLOW:
        exName = TEXT("EXCEPTION_FLT_OVERFLOW");
        break;
    case EXCEPTION_FLT_STACK_CHECK:
        exName = TEXT("EXCEPTION_FLT_STACK_CHECK");
        break;
    case EXCEPTION_FLT_UNDERFLOW:
        exName = TEXT("EXCEPTION_FLT_UNDERFLOW");
        break;
    case EXCEPTION_ILLEGAL_INSTRUCTION:
        exName = TEXT("EXCEPTION_ILLEGAL_INSTRUCTION");
        break;
    case EXCEPTION_IN_PAGE_ERROR:
        exName = TEXT("EXCEPTION_IN_PAGE_ERROR");
        break;
    case EXCEPTION_INT_DIVIDE_BY_ZERO:
        exName = TEXT("EXCEPTION_INT_DIVIDE_BY_ZERO");
        break;
    case EXCEPTION_INT_OVERFLOW:
        exName = TEXT("EXCEPTION_INT_OVERFLOW");
        break;
    case EXCEPTION_INVALID_DISPOSITION:
        exName = TEXT("EXCEPTION_INVALID_DISPOSITION");
        break;
    case EXCEPTION_NONCONTINUABLE_EXCEPTION:
        exName = TEXT("EXCEPTION_NONCONTINUABLE_EXCEPTION");
        break;
    case EXCEPTION_PRIV_INSTRUCTION:
        exName = TEXT("EXCEPTION_PRIV_INSTRUCTION");
        break;
    case EXCEPTION_SINGLE_STEP:
        exName = TEXT("EXCEPTION_SINGLE_STEP");
        break;
    case EXCEPTION_STACK_OVERFLOW:
        exName = TEXT("EXCEPTION_STACK_OVERFLOW");
        break;
    default:
        exName = NULL;
        break;
    }

    return exName;
}

/**
 * Logs some dump information to the log output and then generate a minidump file.
 */
int exceptionFilterFunction(PEXCEPTION_POINTERS exceptionPointers) {
    DWORD exCode;
    TCHAR *exName;
    int i;
    size_t len;
    TCHAR curDir[MAX_PATH];
    TCHAR dumpFile[MAX_PATH];
    BOOL dumpSuccessful;
    HANDLE hDumpFile;
    SYSTEMTIME stLocalTime;
    MINIDUMP_EXCEPTION_INFORMATION expParam;
    int couldLoad;
    FARPROC miniDumpWriteDumpDyn;
    HMODULE dbgHelpDll = LoadLibrary(TEXT("Dbghelp.dll"));
    if( dbgHelpDll == NULL) {
        couldLoad = FALSE;
    } else {
        miniDumpWriteDumpDyn = GetProcAddress(dbgHelpDll, "MiniDumpWriteDump");
        if(miniDumpWriteDumpDyn == NULL) {
            couldLoad = FALSE;
        } else {
            couldLoad = TRUE;
        }
    }
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("--------------------------------------------------------------------") );
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("encountered a fatal error in Wrapper"));
    exCode = exceptionPointers->ExceptionRecord->ExceptionCode;
    exName = getExceptionName(exCode);
    if (exName == NULL) {
        exName = malloc(sizeof(TCHAR) * 64); /* Let this leak.  It only happens once before shutdown. */
        if (exName) {
            _sntprintf(exName, 64, TEXT("Unknown Exception (%ld)"), exCode);
        }
    }

    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("  exceptionCode    = %s"), exName);
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("  exceptionFlag    = %s"),
            (exceptionPointers->ExceptionRecord->ExceptionFlags == EXCEPTION_NONCONTINUABLE ? TEXT("EXCEPTION_NONCONTINUABLE") : TEXT("EXCEPTION_NONCONTINUABLE_EXCEPTION")));
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("  exceptionAddress = %p"), exceptionPointers->ExceptionRecord->ExceptionAddress);
    if (exCode == EXCEPTION_ACCESS_VIOLATION) {
        if (exceptionPointers->ExceptionRecord->ExceptionInformation[0] == 0) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("  Read access exception from %p"),
                    exceptionPointers->ExceptionRecord->ExceptionInformation[1]);
        } else {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("  Write access exception to %p"),
                    exceptionPointers->ExceptionRecord->ExceptionInformation[1]);
        }
    } else {
        for (i = 0; i < (int)exceptionPointers->ExceptionRecord->NumberParameters; i++) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("  exceptionInformation[%d] = %ld"), i,
                    exceptionPointers->ExceptionRecord->ExceptionInformation[i]);
        }
    }

    if (wrapperData) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("  Wrapper Main Loop Status:"));
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("    Current Ticks: 0x%08x"), wrapperGetTicks());
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("    Wrapper State: %s"), wrapperGetWState(wrapperData->wState));
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("    Java State: %s (Timeout: 0x%08x)"), wrapperGetJState(wrapperData->jState), wrapperData->jStateTimeoutTicks);
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("    Exit Requested: %s"), (wrapperData->exitRequested ? TEXT("true") : TEXT("false")));
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("    Restart Mode: %d"), wrapperData->restartRequested);
    }

    /* Get the current directory. */
    len = GetCurrentDirectory(MAX_PATH, curDir);
    if (len == 0) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("  Unable to request current directory.  %s"), getLastErrorText());
        _sntprintf(curDir, MAX_PATH, TEXT("."));
    }
    /* Generate the minidump. */
    GetLocalTime(&stLocalTime);

    _sntprintf(dumpFile, MAX_PATH, TEXT("wrapper-%s-%s-%s-%s-%04d%02d%02d%02d%02d%02d-%ld-%ld.dmp"),
            wrapperOS, wrapperArch, wrapperBits, wrapperVersion,
            stLocalTime.wYear, stLocalTime.wMonth, stLocalTime.wDay,
            stLocalTime.wHour, stLocalTime.wMinute, stLocalTime.wSecond,
            GetCurrentProcessId(), GetCurrentThreadId());
    if (couldLoad == TRUE) {
        hDumpFile = CreateFile(dumpFile, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_WRITE | FILE_SHARE_READ, 0, CREATE_ALWAYS, 0, 0);
        if (hDumpFile == INVALID_HANDLE_VALUE) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("  Failed to create dump file:\n    %s\\%s : %s"), curDir, dumpFile, getLastErrorText());
        } else {

            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("  Writing dump file: %s\\%s"), curDir, dumpFile);

            expParam.ThreadId = GetCurrentThreadId();
            expParam.ExceptionPointers = exceptionPointers;
            expParam.ClientPointers = TRUE;

            dumpSuccessful = (BOOL)miniDumpWriteDumpDyn(GetCurrentProcess(), GetCurrentProcessId(), hDumpFile, MiniDumpWithDataSegs, &expParam, NULL, NULL);
            FreeLibrary(dbgHelpDll);
            if (dumpSuccessful) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("    Dump completed."));
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("  Please send the dump file to support@tanukisoftware.com along with\n    your wrapper.conf and wrapper.log files."));
            } else {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("    Failed to generate dump file.  %s"), getLastErrorText());
            }

        }
    } else {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("  Please send the log file to support@tanukisoftware.com along with\n    your wrapper.conf file."));
    }
    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("--------------------------------------------------------------------") );

    return EXCEPTION_EXECUTE_HANDLER;
}

LPWSTR AllocateAndCopyWideString(LPCWSTR inputString)
{
    LPWSTR outputString = NULL;

    outputString = (LPWSTR)LocalAlloc(LPTR,
        (wcslen(inputString) + 1) * sizeof(WCHAR));
    if (outputString != NULL)
    {
        lstrcpyW(outputString, inputString);
    }
    return outputString;
}

BOOL GetProgAndPublisherInfo(PCMSG_SIGNER_INFO pSignerInfo, PSPROG_PUBLISHERINFO Info) {
    DWORD n;
    BOOL fReturn = FALSE;
    PSPC_SP_OPUS_INFO OpusInfo = NULL;  
    DWORD dwData;
    BOOL fResult;
    
    __try {
        // Loop through authenticated attributes and find
        // SPC_SP_OPUS_INFO_OBJID OID.
        for (n = 0; n < pSignerInfo->AuthAttrs.cAttr; n++) {           
            if (lstrcmpA(SPC_SP_OPUS_INFO_OBJID, pSignerInfo->AuthAttrs.rgAttr[n].pszObjId) == 0) {
                // Get Size of SPC_SP_OPUS_INFO structure.
                fResult = CryptDecodeObject(ENCODING,
                            SPC_SP_OPUS_INFO_OBJID,
                            pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].pbData,
                            pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].cbData,
                            0, NULL, &dwData);
                if (!fResult) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("CryptDecodeObject failed with %x"), GetLastError());
                    __leave;
                }

                // Allocate memory for SPC_SP_OPUS_INFO structure.
                OpusInfo = (PSPC_SP_OPUS_INFO)LocalAlloc(LPTR, dwData);
                if (!OpusInfo) {
                    outOfMemory(TEXT("GPAPI"), 1);
                    __leave;
                }

                // Decode and get SPC_SP_OPUS_INFO structure.
                fResult = CryptDecodeObject(ENCODING,
                            SPC_SP_OPUS_INFO_OBJID,
                            pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].pbData,
                            pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].cbData,
                            0, OpusInfo, &dwData);
                if (!fResult) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("CryptDecodeObject failed with %x"), GetLastError());
                    __leave;
                }

                // Fill in Program Name if present.
                if (OpusInfo->pwszProgramName) {
                    Info->lpszProgramName = AllocateAndCopyWideString(OpusInfo->pwszProgramName);
                } else {
                    Info->lpszProgramName = NULL;
                }
                // Fill in Publisher Information if present.
                if (OpusInfo->pPublisherInfo) {
                    switch (OpusInfo->pPublisherInfo->dwLinkChoice) {
                        case SPC_URL_LINK_CHOICE:
                            Info->lpszPublisherLink = AllocateAndCopyWideString(OpusInfo->pPublisherInfo->pwszUrl);
                            break;

                        case SPC_FILE_LINK_CHOICE:
                            Info->lpszPublisherLink = AllocateAndCopyWideString(OpusInfo->pPublisherInfo->pwszFile);
                            break;

                        default:
                            Info->lpszPublisherLink = NULL;
                            break;
                    }
                } else {
                    Info->lpszPublisherLink = NULL;
                }

                // Fill in More Info if present.
                if (OpusInfo->pMoreInfo) {
                    switch (OpusInfo->pMoreInfo->dwLinkChoice) {
                        case SPC_URL_LINK_CHOICE:
                            Info->lpszMoreInfoLink = AllocateAndCopyWideString(OpusInfo->pMoreInfo->pwszUrl);
                            break;

                        case SPC_FILE_LINK_CHOICE:
                            Info->lpszMoreInfoLink = AllocateAndCopyWideString(OpusInfo->pMoreInfo->pwszFile);
                            break;

                        default:
                            Info->lpszMoreInfoLink = NULL;
                            break;
                    }
                } else {
                    Info->lpszMoreInfoLink = NULL;
                }

                fReturn = TRUE;

                break; // Break from for loop.
            } // lstrcmp SPC_SP_OPUS_INFO_OBJID                 
        } // for 
    }
    __finally {
        if (OpusInfo != NULL) LocalFree(OpusInfo);      
    }

    return fReturn;
}

BOOL GetDateOfTimeStamp(PCMSG_SIGNER_INFO pSignerInfo, SYSTEMTIME *st) {   
    BOOL fResult;
    FILETIME lft, ft;   
    DWORD dwData;
    BOOL fReturn = FALSE;
    DWORD n;
    // Loop through authenticated attributes and find
    // szOID_RSA_signingTime OID.
    for (n = 0; n < pSignerInfo->AuthAttrs.cAttr; n++) {           
        if (lstrcmpA(szOID_RSA_signingTime, pSignerInfo->AuthAttrs.rgAttr[n].pszObjId) == 0) {               
            // Decode and get FILETIME structure.
            dwData = sizeof(ft);
            fResult = CryptDecodeObject(ENCODING,
                        szOID_RSA_signingTime,
                        pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].pbData,
                        pSignerInfo->AuthAttrs.rgAttr[n].rgValue[0].cbData,
                        0, (PVOID)&ft, &dwData);
            if (!fResult) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("CryptDecodeObject failed with %x"),
                    GetLastError());
                break;
            }

            // Convert to local time.
            FileTimeToLocalFileTime(&ft, &lft);
            FileTimeToSystemTime(&lft, st);

            fReturn = TRUE;

            break; // Break from for loop.
                        
        } //lstrcmp szOID_RSA_signingTime
    } // for 

    return fReturn;
}

BOOL GetTimeStampSignerInfo(PCMSG_SIGNER_INFO pSignerInfo, PCMSG_SIGNER_INFO *pCounterSignerInfo) {   
    PCCERT_CONTEXT pCertContext = NULL;
    BOOL fReturn = FALSE;
    BOOL fResult;       
    DWORD dwSize, n;   
   
    __try {
        *pCounterSignerInfo = NULL;

        // Loop through unathenticated attributes for
        // szOID_RSA_counterSign OID.
        for (n = 0; n < pSignerInfo->UnauthAttrs.cAttr; n++) {
            if (lstrcmpA(pSignerInfo->UnauthAttrs.rgAttr[n].pszObjId, szOID_RSA_counterSign) == 0) {
                // Get size of CMSG_SIGNER_INFO structure.
                fResult = CryptDecodeObject(ENCODING,
                           PKCS7_SIGNER_INFO,
                           pSignerInfo->UnauthAttrs.rgAttr[n].rgValue[0].pbData,
                           pSignerInfo->UnauthAttrs.rgAttr[n].rgValue[0].cbData,
                           0,
                           NULL,
                           &dwSize);
                if (!fResult) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("CryptDecodeObject failed with %x"), GetLastError());
                    __leave;
                }

                // Allocate memory for CMSG_SIGNER_INFO.
                *pCounterSignerInfo = (PCMSG_SIGNER_INFO)LocalAlloc(LPTR, dwSize);
                if (!*pCounterSignerInfo) {
                    outOfMemory(TEXT("GTSSI"), 1);
                    __leave;
                }

                // Decode and get CMSG_SIGNER_INFO structure
                // for timestamp certificate.
                fResult = CryptDecodeObject(ENCODING,
                           PKCS7_SIGNER_INFO,
                           pSignerInfo->UnauthAttrs.rgAttr[n].rgValue[0].pbData,
                           pSignerInfo->UnauthAttrs.rgAttr[n].rgValue[0].cbData,
                           0, (PVOID)*pCounterSignerInfo, &dwSize);
                if (!fResult) {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("CryptDecodeObject failed with %x"), GetLastError());
                    __leave;
                }
                fReturn = TRUE;                
                break; // Break from for loop.
            }           
        }
    }
    __finally {
        // Clean up.
        if (pCertContext != NULL) CertFreeCertificateContext(pCertContext);
    }

    return fReturn;
}

BOOL PrintCertificateInfo(PCCERT_CONTEXT pCertContext, int level) {
    BOOL fReturn = FALSE;
    LPTSTR szName = NULL;
    LPTSTR serialNr = NULL;
    DWORD dwData, serialNrLength = 0, n, i;

    __try {
        /* Print Serial Number. */
        log_printf(WRAPPER_SOURCE_WRAPPER, level, TEXT("    Serial Number: "));
        dwData = pCertContext->pCertInfo->SerialNumber.cbData;
        for (i = 0; i < 2; i++) {
            for (n = 0; n < dwData; n++) {
                if (serialNr) {
                    _sntprintf(serialNr + (n * 3) , serialNrLength, TEXT("%02x "), pCertContext->pCertInfo->SerialNumber.pbData[dwData - (n + 1)]);
                } else {
                    serialNrLength += 3;
                }
            }
            if (!serialNr) {
                serialNr = calloc(serialNrLength + 1, sizeof(TCHAR));
                if (!serialNr) {
                    outOfMemory(TEXT("PCI"), 1);
                    __leave;
                }
            }
        }
        log_printf(WRAPPER_SOURCE_WRAPPER, level, TEXT("      %s"), serialNr);
        
        // Get Issuer name size.
        if (!(dwData = CertGetNameString(pCertContext, 
                                         CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                         CERT_NAME_ISSUER_FLAG,
                                         NULL, NULL, 0))) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("CertGetNameString failed."));
            __leave;
        }

        // Allocate memory for Issuer name.
        szName = (LPTSTR)LocalAlloc(LPTR, dwData * sizeof(TCHAR));
        if (!szName) {
            outOfMemory(TEXT("PCI"), 2);
            __leave;
        }

        // Get Issuer name.
        if (!(CertGetNameString(pCertContext, 
                                CERT_NAME_SIMPLE_DISPLAY_TYPE,
                                CERT_NAME_ISSUER_FLAG,
                                NULL, szName, dwData))) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("CertGetNameString failed."));
            __leave;
        }

        // print Issuer name.
        log_printf(WRAPPER_SOURCE_WRAPPER, level, TEXT("    Issuer Name: %s"), szName);
        LocalFree(szName);
        szName = NULL;

        // Get Subject name size.
        if (!(dwData = CertGetNameString(pCertContext, CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, NULL, 0))) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("CertGetNameString failed."));
            __leave;
        }

        // Allocate memory for subject name.
        szName = (LPTSTR)LocalAlloc(LPTR, dwData * sizeof(TCHAR));
        if (!szName) {
            outOfMemory(TEXT("GTSSI"), 3);
            __leave;
        }

        // Get subject name.
        if (!(CertGetNameString(pCertContext,  CERT_NAME_SIMPLE_DISPLAY_TYPE, 0, NULL, szName, dwData))) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("CertGetNameString failed."));
            __leave;
        }

        // Print Subject Name.
        log_printf(WRAPPER_SOURCE_WRAPPER, level, TEXT("    Subject Name: %s"), szName);

        fReturn = TRUE;
    }
    __finally {
        if (szName != NULL) LocalFree(szName);
        if (serialNr != NULL) free(serialNr);
    }

    return fReturn;
}



int printWholeCertificateInfo(LPCWSTR wrapperExeName, int level) {
    HCERTSTORE hStore = NULL;
    HCRYPTMSG hMsg = NULL; 
    PCCERT_CONTEXT pCertContext = NULL;
    BOOL fResult;   
    DWORD dwEncoding, dwContentType, dwFormatType;
    PCMSG_SIGNER_INFO pSignerInfo = NULL;
    PCMSG_SIGNER_INFO pCounterSignerInfo = NULL;
    DWORD dwSignerInfo;
    CERT_INFO CertInfo;     
    SPROG_PUBLISHERINFO ProgPubInfo;
    SYSTEMTIME st;

    ZeroMemory(&ProgPubInfo, sizeof(ProgPubInfo));
    __try {
        // Get message handle and store handle from the signed file.
        fResult = CryptQueryObject(CERT_QUERY_OBJECT_FILE,
                                   wrapperExeName,
                                   CERT_QUERY_CONTENT_FLAG_PKCS7_SIGNED_EMBED,
                                   CERT_QUERY_FORMAT_FLAG_BINARY,
                                   0,
                                   &dwEncoding,
                                   &dwContentType,
                                   &dwFormatType,
                                   &hStore,
                                   &hMsg,
                                   NULL);
        if (!fResult) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("No certificate found! Error: %x"), GetLastError());
            __leave;
        }

        // Get signer information size.
        fResult = CryptMsgGetParam(hMsg, 
                                   CMSG_SIGNER_INFO_PARAM, 
                                   0, 
                                   NULL, 
                                   &dwSignerInfo);
        if (!fResult) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("CryptMsgGetParam failed with %x"), GetLastError());
            __leave;
        }

        // Allocate memory for signer information.
        pSignerInfo = (PCMSG_SIGNER_INFO)LocalAlloc(LPTR, dwSignerInfo);
        if (!pSignerInfo) {
            outOfMemory(TEXT("GWCI"), 1);
            __leave;
        }

        // Get Signer Information.
        fResult = CryptMsgGetParam(hMsg, 
                                   CMSG_SIGNER_INFO_PARAM, 
                                   0, 
                                   (PVOID)pSignerInfo, 
                                   &dwSignerInfo);
        if (!fResult) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("CryptMsgGetParam failed with %x"), GetLastError());
            __leave;
        }
        
        // Get program name and publisher information from 
        // signer info structure.
        if (GetProgAndPublisherInfo(pSignerInfo, &ProgPubInfo))  {
            if (ProgPubInfo.lpszProgramName != NULL) {
                log_printf(WRAPPER_SOURCE_WRAPPER, level, TEXT("    Program Name : %s"),
                    ProgPubInfo.lpszProgramName);
            }

            if (ProgPubInfo.lpszPublisherLink != NULL) {
                log_printf(WRAPPER_SOURCE_WRAPPER, level, TEXT("    Publisher Link : %s"),
                    ProgPubInfo.lpszPublisherLink);
            }

            if (ProgPubInfo.lpszMoreInfoLink != NULL) {
                log_printf(WRAPPER_SOURCE_WRAPPER, level, TEXT("    MoreInfo Link : %s"),
                    ProgPubInfo.lpszMoreInfoLink);
            }
        }


        // Search for the signer certificate in the temporary 
        // certificate store.
        CertInfo.Issuer = pSignerInfo->Issuer;
        CertInfo.SerialNumber = pSignerInfo->SerialNumber;

        pCertContext = CertFindCertificateInStore(hStore, ENCODING, 0, CERT_FIND_SUBJECT_CERT, (PVOID)&CertInfo, NULL);
        if (!pCertContext) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("CertFindCertificateInStore failed with %x"),
                GetLastError());
            __leave;
        }

        // Print Signer certificate information.
        log_printf(WRAPPER_SOURCE_WRAPPER, level, TEXT("  Signer Certificate:"));        
        PrintCertificateInfo(pCertContext, level);

        
        // Get the timestamp certificate signerinfo structure.
        if (GetTimeStampSignerInfo(pSignerInfo, &pCounterSignerInfo)) {
            // Search for Timestamp certificate in the temporary
            // certificate store.
            CertInfo.Issuer = pCounterSignerInfo->Issuer;
            CertInfo.SerialNumber = pCounterSignerInfo->SerialNumber;

            pCertContext = CertFindCertificateInStore(hStore,
                                                ENCODING,
                                                0,
                                                CERT_FIND_SUBJECT_CERT,
                                                (PVOID)&CertInfo,
                                                NULL);
            if (!pCertContext) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("CertFindCertificateInStore failed with %x"),
                    GetLastError());
                __leave;
            }

            // Print timestamp certificate information.
            log_printf(WRAPPER_SOURCE_WRAPPER, level, TEXT("  TimeStamp Certificate:"));
            PrintCertificateInfo(pCertContext, level);


            // Find Date of timestamp.
            if (GetDateOfTimeStamp(pCounterSignerInfo, &st)) {
                log_printf(WRAPPER_SOURCE_WRAPPER, level, TEXT("    Date of TimeStamp : %04d/%02d/%02d %02d:%02d"),
                                            st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
            }

        }
    }
    __finally {               
        // Clean up.
        if (ProgPubInfo.lpszProgramName != NULL)
            LocalFree(ProgPubInfo.lpszProgramName);
        if (ProgPubInfo.lpszPublisherLink != NULL)
            LocalFree(ProgPubInfo.lpszPublisherLink);
        if (ProgPubInfo.lpszMoreInfoLink != NULL)
            LocalFree(ProgPubInfo.lpszMoreInfoLink);

        if (pSignerInfo != NULL) LocalFree(pSignerInfo);
        if (pCounterSignerInfo != NULL) LocalFree(pCounterSignerInfo);
        if (pCertContext != NULL) CertFreeCertificateContext(pCertContext);
        if (hStore != NULL) CertCloseStore(hStore, 0);
        if (hMsg != NULL) CryptMsgClose(hMsg);
    }
    return 0;
}

BOOL verifyEmbeddedSignature() {
    LONG lStatus;
    DWORD dwLastError;
    TCHAR pwszSourceFile[_MAX_PATH];
    GUID WVTPolicyGUID = WINTRUST_ACTION_GENERIC_VERIFY_V2;
    WINTRUST_DATA WinTrustData;
    WINTRUST_FILE_INFO FileData;    
    
    if (!GetModuleFileName(NULL, pwszSourceFile, _MAX_PATH)) {
        return FALSE;
    }
    memset(&FileData, 0, sizeof(FileData));
    FileData.cbStruct = sizeof(WINTRUST_FILE_INFO);
    FileData.pcwszFilePath = pwszSourceFile;
    FileData.hFile = NULL;
    FileData.pgKnownSubject = NULL;
    memset(&WinTrustData, 0, sizeof(WinTrustData));
    WinTrustData.cbStruct = sizeof(WinTrustData);
    
    WinTrustData.pPolicyCallbackData = NULL;
    WinTrustData.pSIPClientData = NULL;
    WinTrustData.dwUIChoice = WTD_UI_NONE;
    WinTrustData.fdwRevocationChecks = WTD_REVOKE_NONE; 
    WinTrustData.dwUnionChoice = WTD_CHOICE_FILE;
    WinTrustData.dwStateAction = WTD_STATEACTION_VERIFY;
    WinTrustData.hWVTStateData = NULL;
    WinTrustData.pwszURLReference = NULL;
    WinTrustData.dwProvFlags = WTD_USE_DEFAULT_OSVER_CHECK;
    WinTrustData.dwUIContext = 0;
    WinTrustData.pFile = &FileData;
    lStatus = WinVerifyTrust(NULL, &WVTPolicyGUID, &WinTrustData);
    
    switch (lStatus) {
        case ERROR_SUCCESS:
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("The file \"%s\" is signed and the signature was verified."), pwszSourceFile);
            printWholeCertificateInfo(pwszSourceFile, LEVEL_DEBUG);
            break;
        
        case TRUST_E_NOSIGNATURE:
            /* The file was not signed or had a signature 
             that was not valid. */

            // Get the reason for no signature.
            dwLastError = GetLastError();
            if ((TRUST_E_SUBJECT_FORM_UNKNOWN == dwLastError) || (TRUST_E_NOSIGNATURE == dwLastError) || (TRUST_E_PROVIDER_UNKNOWN == dwLastError)) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("The file \"%s\" is not signed."), pwszSourceFile);
            } else {
                // The signature was not valid or there was an error 
                // opening the file.
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("An unknown error occurred trying to verify the signature of the \"%s\" file: %s"),
                    pwszSourceFile, getLastErrorText());
            }
            break;

        case TRUST_E_EXPLICIT_DISTRUST:
            // The hash that represents the subject or the publisher 
            // is not allowed by the admin or user.
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("The signature is present, but specifically disallowed."));
            printWholeCertificateInfo(pwszSourceFile, LEVEL_WARN);
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("The Wrapper will shutdown!"));
            appExit(0);

            break;

        case TRUST_E_SUBJECT_NOT_TRUSTED:
            // The user clicked "No" when asked to install and run.
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("The signature is present, but not trusted."));
            printWholeCertificateInfo(pwszSourceFile, LEVEL_WARN);
            break;

        case CRYPT_E_SECURITY_SETTINGS:
            /*
            The hash that represents the subject or the publisher 
            was not explicitly trusted by the admin and the 
            admin policy has disabled user trust. No signature, 
            publisher or time stamp errors.
            */
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_DEBUG, TEXT("CRYPT_E_SECURITY_SETTINGS - The hash\nrepresenting the subject or the publisher wasn't\nexplicitly trusted by the admin and admin policy\nhas disabled user trust. No signature, publisher or timestamp errors."));
            printWholeCertificateInfo(pwszSourceFile, LEVEL_DEBUG);
            break;

        default:
            dwLastError = GetLastError();
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("A signature was found in \"%s\", but checksum failed: (Errorcode: 0x%x) %s"), pwszSourceFile, lStatus, getLastErrorText());
            printWholeCertificateInfo(pwszSourceFile, LEVEL_FATAL);
            if (dwLastError == TRUST_E_BAD_DIGEST  || dwLastError == TRUST_E_CERT_SIGNATURE) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("The Wrapper will shutdown!"));
                appExit(0);
            }
            break;
    }
    return TRUE;
}



#ifndef CUNIT
void _tmain(int argc, TCHAR **argv) {
    int result;
#ifdef _DEBUG
    int i;
#endif

    /* The StartServiceCtrlDispatcher requires this table to specify
     * the ServiceMain function to run in the calling process. The first
     * member in this example is actually ignored, since we will install
     * our service as a SERVICE_WIN32_OWN_PROCESS service type. The NULL
     * members of the last entry are necessary to indicate the end of
     * the table; */
    SERVICE_TABLE_ENTRY serviceTable[2];

    if (buildSystemPath()) {
        appExit(1);
        return; /* For clarity. */
    }

    if (wrapperInitialize()) {
        appExit(1);
        return; /* For clarity. */
    }
    SetThreadLocale(GetUserDefaultLCID());
    /* Main thread initialized in wrapperInitialize. */

    /* Enclose the rest of the program in a try catch block so we can
     *  display and log useful information should the need arise.  This
     *  must be done after logging has been initialized as the catch
     *  block makes use of the logger. */
    __try {
#ifdef _DEBUG
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Wrapper DEBUG build!"));
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_STATUS, TEXT("Logging initialized."));
#endif
        /* Get the current process. */
        wrapperData->wrapperProcess = GetCurrentProcess();
        wrapperData->wrapperPID = GetCurrentProcessId();

        if (initializeWinSock()) {
            appExit(1);
            return; /* For clarity. */
        }

        if (setWorkingDir()) {
            appExit(1);
            return; /* For clarity. */
        }

        if (collectUserInfo()) {
            appExit(1);
            return; /* For clarity. */
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
            appExit(1);
            return; /* For clarity. */
        }
        wrapperLoadHostName();

        /* At this point, we have a command, confFile, and possibly additional arguments. */
        if (!strcmpIgnoreCase(wrapperData->argCommand, TEXT("?")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-help"))) {
            /* User asked for the usage. */
            setSimpleLogLevels();
            wrapperUsage(argv[0]);
            appExit(0);
            return; /* For clarity. */
        } else if (!strcmpIgnoreCase(wrapperData->argCommand, TEXT("v")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-version"))) {
            /* User asked for version. */
            setSimpleLogLevels();
            wrapperVersionBanner();
            appExit(0);
            return; /* For clarity. */
        } else if (!strcmpIgnoreCase(wrapperData->argCommand, TEXT("h")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-hostid"))) {
            /* Print out a banner containing the host id. */
            setSimpleLogLevels();
            wrapperVersionBanner();
            showHostIds(LEVEL_STATUS);
            appExit(0);
            return; /* For clarity. */
        }
        /* All 4 valid commands use the configuration file.  It is loaded here to
         *  reduce duplicate code.  But before loading the parameters, in the case
         *  of an NT service. the environment variables must first be loaded from
         *  the registry.
         * This is not necessary for versions of Windows XP and above. */
        if ((!strcmpIgnoreCase(wrapperData->argCommand, TEXT("s")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-service"))) && (isWinXP() == FALSE)) {
            if (wrapperLoadEnvFromRegistry())
            {
                appExit(1);
                return; /* For clarity. */
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
            appExit(1);
            return; /* For clarity. */
        }

        /* Set the default umask of the Wrapper process. */
        _umask(wrapperData->umask);

        /* Perform the specified command */
        if(!strcmpIgnoreCase(wrapperData->argCommand, TEXT("i")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-install"))) {
            /* Install an NT service */
            /* Always auto close the log file to keep the output in synch. */
            setLogfileAutoClose(TRUE);
            wrapperCheckForMappedDrives();
            /* are we elevated ? */
            if (!isElevated()) {
                appExit(elevateThis(argc, argv));
            } else {
                /* are we launched secondary? */
                if (getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL) != NULL && duplicateSTD() == FALSE) {
                    appExit(1);
                    return;
                }
                appExit(wrapperInstall());
            }
            return; /* For clarity. */
        } else if(!strcmpIgnoreCase(wrapperData->argCommand, TEXT("it")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-installstart"))) {
            /* Install and Start an NT service */
            /* Always auto close the log file to keep the output in synch. */
            setLogfileAutoClose(TRUE);
            wrapperCheckForMappedDrives();
            if (!isElevated()) {
                appExit(elevateThis(argc, argv));
            } else {
                /* are we launched secondary? */
                if (getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL) != NULL && duplicateSTD() == FALSE) {
                    appExit(1);
                    return;
                }
                result = wrapperInstall();
                if (!result) {
                    result = wrapperStartService();
                }
                appExit(result);
            }
            return; /* For clarity. */
        } else if (!strcmpIgnoreCase(wrapperData->argCommand, TEXT("r")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-remove"))) {
            /* Remove an NT service */
            /* Always auto close the log file to keep the output in synch. */
            setLogfileAutoClose(TRUE);
            if (!isElevated()) {
                appExit(elevateThis(argc, argv));
            } else {
                /* are we launched secondary? */
                if (getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL) != NULL && duplicateSTD() == FALSE) {
                    appExit(1);
                    return;
                }
                appExit(wrapperRemove());
            }
            return; /* For clarity. */
        } else if(!strcmpIgnoreCase(wrapperData->argCommand, TEXT("t")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-start"))) {
            /* Start an NT service */
            /* Always auto close the log file to keep the output in synch. */
            setLogfileAutoClose(TRUE);
            wrapperCheckForMappedDrives();
            if (!isElevated()) {
                appExit(elevateThis(argc, argv));
            } else {
                /* are we launched secondary? */
                if (getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL) != NULL && duplicateSTD() == FALSE) {
                    appExit(1);
                    return;
                }
                appExit(wrapperStartService());
            }
            return; /* For clarity. */
        } else if(!strcmpIgnoreCase(wrapperData->argCommand, TEXT("a")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-pause"))) {
            /* Pause a started NT service */
            /* Always auto close the log file to keep the output in synch. */
            setLogfileAutoClose(TRUE);
            if (!isElevated()) {
                appExit(elevateThis(argc, argv));
            } else {
                /* are we launched secondary? */
                if (getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL) != NULL && duplicateSTD() == FALSE) {
                    appExit(1);
                    return;
                }
                appExit(wrapperPauseService());
            }
            return; /* For clarity. */
        } else if(!strcmpIgnoreCase(wrapperData->argCommand, TEXT("e")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-resume"))) {
            /* Resume a paused NT service */
            /* Always auto close the log file to keep the output in synch. */
            setLogfileAutoClose(TRUE);
            if (!isElevated()) {
                appExit(elevateThis(argc, argv));
            } else {
                /* are we launched secondary? */
                if (getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL) != NULL && duplicateSTD() == FALSE) {
                    appExit(1);
                    return;
                }
                appExit(wrapperResumeService());
            }
            return; /* For clarity. */
        } else if(!strcmpIgnoreCase(wrapperData->argCommand, TEXT("p")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-stop"))) {
            /* Stop an NT service */
            /* Always auto close the log file to keep the output in synch. */
            setLogfileAutoClose(TRUE);
            if (!isElevated()) {
                appExit(elevateThis(argc, argv));
            } else {
                /* are we launched secondary? */
                if (getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL) != NULL && duplicateSTD() == FALSE) {
                    appExit(1);
                    return;
                }
                appExit(wrapperStopService(TRUE));
            }
            return; /* For clarity. */
        } else if(!strcmpIgnoreCase(wrapperData->argCommand, TEXT("l")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-controlcode"))) {
            /* Send a control code to an NT service */
            /* Always auto close the log file to keep the output in synch. */
            setLogfileAutoClose(TRUE);
            if (!isElevated()) {
                appExit(elevateThis(argc, argv));
            } else {
                /* are we launched secondary? */
                if (getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL) != NULL && duplicateSTD() == FALSE) {
                    appExit(1);
                    return;
                }
                appExit(wrapperSendServiceControlCode(argv, wrapperData->argCommandArg));
            }
            return; /* For clarity. */
        } else if(!strcmpIgnoreCase(wrapperData->argCommand, TEXT("d")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-dump"))) {
            /* Request a thread dump */
            /* Always auto close the log file to keep the output in synch. */
            setLogfileAutoClose(TRUE);
            if (!isElevated()) {
                appExit(elevateThis(argc, argv));
            } else {
                /* are we launched secondary? */
                if (getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL) != NULL && duplicateSTD() == FALSE) {
                    appExit(1);
                    return;
                }
                appExit(wrapperRequestThreadDump(argv));
            }
            return; /* For clarity. */
        } else if(!strcmpIgnoreCase(wrapperData->argCommand, TEXT("q")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-query"))) {
            /* Return service status with console output. */
            /* Always auto close the log file to keep the output in synch. */
            setLogfileAutoClose(TRUE);
            if (!isElevated()) {
                appExit(elevateThis(argc, argv));
            } else {
                /* are we launched secondary? */
                if (getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL) != NULL && duplicateSTD() == FALSE) {
                    appExit(1);
                    return;
                }
                appExit(wrapperServiceStatus(TRUE));
            }
            return; /* For clarity. */
        } else if(!strcmpIgnoreCase(wrapperData->argCommand, TEXT("qs")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-querysilent"))) {
            /* Return service status without console output. */
            /* Always auto close the log file to keep the output in synch. */
            setLogfileAutoClose(TRUE);
            if (!isElevated()) {
                appExit(elevateThis(argc, argv));
            } else {
                /* are we launched secondary? */
                if (getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL) != NULL && duplicateSTD() == FALSE) {
                    appExit(1);
                    return;
                }
                appExit(wrapperServiceStatus(FALSE));
            }
            appExit(wrapperServiceStatus(FALSE));
            return; /* For clarity. */
        } else if(!strcmpIgnoreCase(wrapperData->argCommand, TEXT("c")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-console"))) {
            /* Run as a console application */
            /* Load any dynamic functions. */
            loadDLLProcs();

            /* Initialize the invocation mutex as necessary, exit if it already exists. */
            if (initInvocationMutex()) {
                appExit(1);
                return; /* For clarity. */
            }

            /* See if the logs should be rolled on Wrapper startup. */
            if ((getLogfileRollMode() & ROLL_MODE_WRAPPER) ||
                (getLogfileRollMode() & ROLL_MODE_JVM)) {
                rollLogs();
            }

            /* Write pid and anchor files as requested.  If they are the same file the file is
             *  simply overwritten. */
            cleanUpPIDFilesOnExit = TRUE;
            if (wrapperData->anchorFilename) {
                if (writePidFile(wrapperData->anchorFilename, wrapperData->wrapperPID, wrapperData->anchorFileUmask)) {
                    log_printf
                        (WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                         TEXT("ERROR: Could not write anchor file %s: %s"),
                         wrapperData->anchorFilename, getLastErrorText());
                    appExit(1);
                    return; /* For clarity. */
                }
            }
            if (wrapperData->pidFilename) {
                if (writePidFile(wrapperData->pidFilename, wrapperData->wrapperPID, wrapperData->pidFileUmask)) {
                    log_printf
                        (WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                         TEXT("ERROR: Could not write pid file %s: %s"),
                         wrapperData->pidFilename, getLastErrorText());
                    appExit(1);
                    return; /* For clarity. */
                }
            }
            if (wrapperData->lockFilename) {
                if (writePidFile(wrapperData->lockFilename, wrapperData->wrapperPID, wrapperData->lockFileUmask)) {
                    log_printf
                        (WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                         TEXT("ERROR: Could not write lock file %s: %s"),
                         wrapperData->lockFilename, getLastErrorText());
                    appExit(1);
                    return; /* For clarity. */
                }
            }

            appExit(wrapperRunConsole());
            return; /* For clarity. */
        } else if(!strcmpIgnoreCase(wrapperData->argCommand, TEXT("s")) || !strcmpIgnoreCase(wrapperData->argCommand, TEXT("-service"))) {
            /* Run as a service */
            wrapperCheckForMappedDrives();
            /* Load any dynamic functions. */
            loadDLLProcs();

            /* Initialize the invocation mutex as necessary, exit if it already exists. */
            if (initInvocationMutex()) {
                appExit(1);
                return; /* For clarity. */
            }

            /* Get the current process. */
            wrapperData->wrapperProcess = GetCurrentProcess();
            wrapperData->wrapperPID = GetCurrentProcessId();

            /* See if the logs should be rolled on Wrapper startup. */
            if ((getLogfileRollMode() & ROLL_MODE_WRAPPER) ||
                (getLogfileRollMode() & ROLL_MODE_JVM)) {
                rollLogs();
            }

            /* Write pid and anchor files as requested.  If they are the same file the file is
             *  simply overwritten. */
            cleanUpPIDFilesOnExit = TRUE;
            if (wrapperData->anchorFilename) {
                if (writePidFile(wrapperData->anchorFilename, wrapperData->wrapperPID, wrapperData->anchorFileUmask)) {
                    log_printf
                        (WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                         TEXT("ERROR: Could not write anchor file %s: %s"),
                         wrapperData->anchorFilename, getLastErrorText());
                    appExit(1);
                    return; /* For clarity. */
                }
            }
            if (wrapperData->pidFilename) {
                if (writePidFile(wrapperData->pidFilename, wrapperData->wrapperPID, wrapperData->pidFileUmask)) {
                    log_printf
                        (WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                         TEXT("ERROR: Could not write pid file %s: %s"),
                         wrapperData->pidFilename, getLastErrorText());
                    appExit(1);
                    return; /* For clarity. */
                }
            }

            /* Prepare the service table */
            serviceTable[0].lpServiceName = wrapperData->serviceName;
            serviceTable[0].lpServiceProc = (LPSERVICE_MAIN_FUNCTION)wrapperServiceMain;
            serviceTable[1].lpServiceName = NULL;
            serviceTable[1].lpServiceProc = NULL;

            _tprintf(TEXT("Attempting to start %s as an NT service.\n"), wrapperData->serviceDisplayName);
            _tprintf(TEXT("\nCalling StartServiceCtrlDispatcher...please wait.\n"));

            /* Start the service control dispatcher. 
             *  The ServiceControlDispatcher will call the wrapperServiceMain method. */
            if (!StartServiceCtrlDispatcher(serviceTable)) {
                _tprintf(TEXT("\n"));
                _tprintf(TEXT("StartServiceControlDispatcher failed!\n"));
                _tprintf(TEXT("\n"));
                _tprintf(TEXT("The -s and --service commands should only be called by the Windows\n"));
                _tprintf(TEXT("ServiceManager to control the Wrapper as a service, and is not\n"));
                _tprintf(TEXT("designed to be run manually by the user.\n"));
                _tprintf(TEXT("\n"));
                _tprintf(TEXT("For help, type\n"));
                _tprintf(TEXT("%s -?\n"), argv[0]);
                _tprintf(TEXT("\n"));
                appExit(1);
                return; /* For clarity. */
            }

            /* We will get here when the service starts to stop */
            /* As wrapperServiceMain should take care of shutdown, wait 10 sec to give some time for its shutdown 
             * but the process should exit before the sleep completes. */
            wrapperSleep(10000);
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Timed out waiting for wrapperServiceMain"));
            appExit(1);
            return; /* For clarity. */
        } else {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT(""));
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Unrecognized option: -%s"), wrapperData->argCommand);
            wrapperUsage(argv[0]);
            appExit(1);
            return; /* For clarity. */
        }
    } __except (exceptionFilterFunction(GetExceptionInformation())) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("<-- Wrapper Stopping due to error"));
        appExit(1);
        return; /* For clarity. */
    }
}
#endif

/*
 * This function will connect to the secondary/elevated wrapper process and
 * read all output from stdout and stderr. Furthermore it will handle input
 * being sent to stdin. This function won't return until the client closed all
 * named pipes connected.
 * @param in  - the File HANDLE for the outbound channel to write to the 2nd process
 * @param out, err - the File HANDLEs for the inbound channel to read from the 2nd process
 *
 * @return TRUE if everything worked well. FALSE otherwise.
 */
BOOL readAndWriteNamedPipes(HANDLE in, HANDLE out, HANDLE err) {
    TCHAR inbuf[1024], outbuf[512], errbuf[512], *secret;
    DWORD currentBlockAvail, outRead, errRead, inWritten, ret, writeOut;
    BOOL fConnected, outClosed = FALSE, errClosed = FALSE;

    /* the named pipes are nonblocking, so loop until an connection could
     * have been established with the secondary process (or an error occured) */
    do {
        /* ConnectNamedPipe does rather wait until a connection was established
           However, the inbound pipes are non-blocking, so ConnectNamedPipe immediately
           returns. So call it looped...*/
        fConnected = ConnectNamedPipe(out, NULL);
    } while((fConnected == 0) && (GetLastError() == ERROR_PIPE_LISTENING));
    /* check for error */
    /* if ERROR_PIPE_CONNECTED it just means that while ConnectNamedPipe(..) was
     * called again, in the meantime the client connected. So in fact that's what we want.
     * WIN-7: if ERROR_NO_DATA, it means that the process has already been gone, probably an error in the start (but not in the pipe)
     * it might even be very likely there is data in stderr to retrieve */
    if ((fConnected == 0) && (GetLastError() != ERROR_PIPE_CONNECTED) && (GetLastError() != ERROR_NO_DATA)) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("The connect to stdout of the elevated process failed: %s"), getLastErrorText());
        return FALSE;
    }
    /* Same as above */
    do {
        fConnected = ConnectNamedPipe(err, NULL);
    } while((fConnected == 0) && (GetLastError() == ERROR_PIPE_LISTENING));
    if ((fConnected == 0) && (GetLastError() != ERROR_PIPE_CONNECTED) && (GetLastError() != ERROR_NO_DATA)) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("The connect to stderr of the elevated process failed: %s"), getLastErrorText());
        return FALSE;
    }

    do {
        writeOut = 0;
        /* out */
        if (!outClosed) {
            currentBlockAvail = 0;
            /* Check how much data is available for reading... */
            ret = PeekNamedPipe(out, NULL, 0, NULL, &currentBlockAvail, NULL);
            if ((ret == 0) && (GetLastError() == ERROR_BROKEN_PIPE)) {
                /* ERROR_BROKEN_PIPE - the client has closed the pipe. So most likely it just exited */
                outClosed = TRUE;
            }
            /* currentBlockAvail is already in bytes! */
            if (ret && (currentBlockAvail > 0) && (currentBlockAvail < 512 * sizeof(TCHAR))) {
                /* Clean the buffer before each read, as we don't want old stuff */
                memset(outbuf,0, sizeof(outbuf));
                if (ReadFile(out, outbuf, currentBlockAvail, &outRead, NULL) == TRUE) {
                    /* if the message we just read in, doesn't have a new line, it means, that we most likely
                       got the secondary process prompting sth. */
                    if (outbuf[_tcslen(outbuf) - 1] != TEXT('\n')) {
                        /* To make sure, check if in is indeed waiting for input */
                        if (WaitForSingleObject(in, 1000) == WAIT_OBJECT_0) {
                            /* Clean the input buffer before each read */
                            memset(inbuf, 0, sizeof(inbuf));
                            /* A prompt can have an "n" - normal at the end. So this means, that
                               we prompt with echo */
                            if (outbuf[_tcslen(outbuf) - 1] == TEXT('n')) {
                                /* clean the mark */
                                outbuf[_tcslen(outbuf) - 1] = TEXT('\0');
                                /* show the prompt */
                                _tprintf(TEXT("%s"), outbuf);
                                /* and prompt */
                                _fgetts(inbuf, 1024, stdin);
                                if (WriteFile(in, inbuf, (DWORD)(_tcslen(inbuf)) * sizeof(TCHAR), &inWritten, NULL) == FALSE) {
                                    /* something happened with the named pipe, get out */
                                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Writing to the elevated process failed (%d): %s"), GetLastError(), getLastErrorText());
                                    return FALSE;
                                }
                            } else if (outbuf[_tcslen(outbuf) - 1] == TEXT('p')) {
                              /* A prompt can have an "p" - password at the end. So this means, that
                               we prompt without showing the input on the screen */
                                outbuf[_tcslen(outbuf) - 1] = TEXT('\0');
                                /* show the prompt */
                                _tprintf(TEXT("%s"), outbuf);
                                /* now read secret, readPassword already works with allocating a buffer (max 64(+1) character supported) */
                                secret = readPassword();
                                _tcsncpy(inbuf, secret, 1024);
                                free(secret);
                                /* "secret" does not have any delimiter we could use, so send the whole inbuf buffer */
                                /* this is the downside of using MESSAGE pipes */
                                if (WriteFile(in, inbuf, (DWORD)sizeof(inbuf), &inWritten, NULL) == FALSE) {
                                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Writing to the elevated process failed (%d): %s"), GetLastError(), getLastErrorText());
                                    return FALSE;
                                }
                            } else {
                                /* no additional for the prompt was provided, but WaitForSingleObject indicates, that
                                   stdin is expecting input. Handle this as if "n" - normal was specified (without clearing the mark)
                                /* show the prompt */
                                _tprintf(TEXT("%s"), outbuf);
                                /* and prompt */
                                _fgetts(inbuf, 1024, stdin);
                                if (WriteFile(in, inbuf, (DWORD)(_tcslen(inbuf)) * sizeof(TCHAR), &inWritten, NULL) == FALSE) {
                                    /* something happened with the named pipe, get out */
                                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Writing to the elevated process failed (%d): %s"), GetLastError(), getLastErrorText());
                                    return FALSE;
                                }
                            }
                            /* this is important! for transparency writing to the elevated process works as without this layer,
                               however, not flushing the buffer will make _getts just keep blocking until the buffer over there
                               is full (what we don't want) */
                            FlushFileBuffers(in);
                        } else {
                            /* A timeout occured! probably a print without a newline. */
                            _tprintf(TEXT("%s\n"), outbuf);
                        }
                    } else {
                       /* This is the normal case - just output */
                       /* print the message on the screen */
                       _tprintf(TEXT("%s"), outbuf);
                    }
                } else {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Reading stdout of the elevated process failed (%d): %s"), GetLastError(), getLastErrorText());
                    return FALSE;
                }
            }
        }
        /* err */
        /* it works almost exactly as reading stdout, except no input will be checked */
        if (!errClosed) {
            currentBlockAvail = 0;
            ret = PeekNamedPipe(err, NULL, 0, NULL, &currentBlockAvail, NULL);
            if ((ret == 0) && (GetLastError() == ERROR_BROKEN_PIPE)) {
                errClosed = TRUE;
            }
            if (ret && (currentBlockAvail > 0) && (currentBlockAvail < 512 * sizeof(TCHAR))) {
                memset(errbuf,0, sizeof(errbuf));
                if (ReadFile(err, errbuf, currentBlockAvail, &errRead, NULL) == TRUE) {
                    _tprintf(TEXT("%s"), errbuf);
                } else {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Reading stderr of the elevated process failed (%d): %s"), GetLastError(), getLastErrorText());
                    return FALSE;
                }
            }
        }
    } while(!errClosed || !outClosed);
    return TRUE;
}

/* This function needs to get called from the elevated/secondary process.
 * It will open the named pipes, the caller has establishes and redirects stdin, stdout, stderr
 * to this named pipes.
 * All this call is doing should be transparent and (except the stdin prompting) not interfere
 * the secondary process (i.e. log files will still work and logging to logfile actually will be done here)
 *
 * @return If successful this function will return TRUE, FALSE otherwise
 */
BOOL duplicateSTD() {
    TCHAR* strNamedPipeNameIn, *strNamedPipeNameOut, *strNamedPipeNameErr;
    const TCHAR *pipeBaseName;
    HANDLE pipeIn, pipeOut, pipeErr;
    int ret, fdOut, fdIn, fdErr;
    size_t len;

    /* get the base name for the named pipe, each channel will append an additional extension */
    pipeBaseName = getStringProperty(properties, TEXT("wrapper.internal.namedpipe"), NULL);
    len = _tcslen(pipeBaseName) + 13;
    strNamedPipeNameIn = malloc(sizeof(TCHAR) * len);
    if (!strNamedPipeNameIn) {
        outOfMemory(TEXT("MSE"), 1);
        return FALSE;
    }
    _sntprintf(strNamedPipeNameIn, len, TEXT("\\\\.\\pipe\\%sINN"), pipeBaseName);

    strNamedPipeNameOut = malloc(sizeof(TCHAR) * len);
    if (!strNamedPipeNameOut) {
        free(strNamedPipeNameIn);
        outOfMemory(TEXT("MSE"), 2);
        return FALSE;
    }
    _sntprintf(strNamedPipeNameOut, len, TEXT("\\\\.\\pipe\\%sOUT"), pipeBaseName);

    strNamedPipeNameErr = malloc(sizeof(TCHAR) * len);
    if (!strNamedPipeNameErr) {
        free(strNamedPipeNameIn);
        free(strNamedPipeNameOut);
        outOfMemory(TEXT("MSE"), 3);
        return FALSE;
    }
    _sntprintf(strNamedPipeNameErr, len, TEXT("\\\\.\\pipe\\%sERR"), pipeBaseName);

#ifdef _DEBUG
    _tprintf(TEXT("going to open %s, %s and %s\n"), strNamedPipeNameIn, strNamedPipeNameOut, strNamedPipeNameErr);
#endif
    /* Use CreateFile to connect to the Named Pipes. */
    if ((pipeIn = CreateFile(strNamedPipeNameIn, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Connect to stdin pipe failed (%d): %s"), GetLastError(), getLastErrorText());
        ret = FALSE;
    } else {
        if ((pipeOut = CreateFile(strNamedPipeNameOut, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Connect to stdout pipe failed (%d): %s"), GetLastError(), getLastErrorText());
            ret = FALSE;
        } else {
            if ((pipeErr = CreateFile(strNamedPipeNameErr, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, 0, NULL)) == INVALID_HANDLE_VALUE) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Connect to stderr pipe failed (%d): %s"), GetLastError(), getLastErrorText());
                ret = FALSE;
            } else {
                /* This is magic */
                if (((fdIn = _open_osfhandle((long)pipeIn, 0)) != -1) &&
                   ((fdErr = _open_osfhandle((long)pipeErr, 0)) != -1) &&
                   ((fdOut = _open_osfhandle((long)pipeOut, 0)) != -1)) {

                    if ((_dup2(fdIn, 0) == 0) && (_dup2(fdOut, 1) == 0) && (_dup2(fdErr, 2) == 0)) {
                        ret = TRUE;
#ifdef _DEBUG
                        _ftprintf(stderr, TEXT("12345\n"));fflush(NULL);
                        _ftprintf(stderr, TEXT("1234567890\n"));fflush(NULL);
                        _ftprintf(stdout, TEXT("12345\n"));fflush(NULL);
                        _ftprintf(stdout, TEXT("1234567890\n"));fflush(NULL);
#endif
                    } else {
                        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                            TEXT("ERROR: Could not redirect the file descriptors to the client sided named pipes."));
                    }
                } else {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL,
                        TEXT("ERROR: Could not acquire the file descriptors for the client sided named pipes."));
                }
            }
        }
    }
    free(strNamedPipeNameErr);
    free(strNamedPipeNameOut);
    free(strNamedPipeNameIn);
    return ret;
}


/* This function first creates 3 named pipes (2 inbound & 1 outbound) for establishing the connection.
 * Then it will ask the user to allow elevation for a secondary process.
 * And finally (if elevation granted) wait and call readAndWriteNamedPipes until the elevated process finishes.
 *
 * @param hwnd - The current window handle.
 * @param pszVerb - the verb defining the action ShellExecuteEx will perform
 * @param pszPath - the path to the executable going to be called
 * @param pszParameters - the parameters for the executable
 * @param pszDirectory - the working directory the process will have (if NULL the working direcory context will be inherited)
 * @param namedPipeName - the base name for the named pipes for the IPC between us and the new process.
 * @return TRUE if successfull, FALSE otherwise
 */
BOOL myShellExec(HWND hwnd, LPCTSTR pszVerb, LPCTSTR pszPath, LPCTSTR pszParameters, LPCTSTR pszDirectory, TCHAR* namedPipeName) {
    DWORD returnValue;
    SHELLEXECUTEINFO shex;
    HANDLE hNamedPipeIn, hNamedPipeOut, hNamedPipeErr;
    TCHAR* strNamedPipeNameIn, *strNamedPipeNameOut, *strNamedPipeNameErr;
    int ret = FALSE;
    size_t len;

    /* first we generate the filenames for the named pipes based on namedPipeName */
    len = _tcslen(namedPipeName) + 4 + 9;
    strNamedPipeNameIn = malloc(sizeof(TCHAR) * len);
    if (!strNamedPipeNameIn) {
        outOfMemory(TEXT("MSE"), 1);
        return FALSE;
    }
    _sntprintf(strNamedPipeNameIn, len, TEXT("\\\\.\\pipe\\%sINN"), namedPipeName);

    strNamedPipeNameOut = malloc(sizeof(TCHAR) * len);
    if (!strNamedPipeNameOut) {
        free(strNamedPipeNameIn);
        outOfMemory(TEXT("MSE"), 2);
        return FALSE;
    }
    _sntprintf(strNamedPipeNameOut, len, TEXT("\\\\.\\pipe\\%sOUT"), namedPipeName);

    strNamedPipeNameErr = malloc(sizeof(TCHAR) * len);
    if (!strNamedPipeNameErr) {
        free(strNamedPipeNameIn);
        free(strNamedPipeNameOut);
        outOfMemory(TEXT("MSE"), 3);
        return FALSE;
    }
    _sntprintf(strNamedPipeNameErr, len, TEXT("\\\\.\\pipe\\%sERR"), namedPipeName);
    /* create the process information */
    memset(&shex, 0, sizeof(shex));
    shex.cbSize         = sizeof(SHELLEXECUTEINFO);
    shex.fMask          = SEE_MASK_NO_CONSOLE | SEE_MASK_NOCLOSEPROCESS;
    shex.hwnd           = hwnd;
    shex.lpVerb         = pszVerb;
    shex.lpFile         = pszPath;
    shex.lpParameters   = pszParameters;
    shex.lpDirectory    = pszDirectory;
#ifdef _DEBUG
    shex.nShow          = SW_SHOWNORMAL;
#else
    shex.nShow          = SW_HIDE;
#endif

    hNamedPipeIn = CreateNamedPipe(strNamedPipeNameIn, PIPE_ACCESS_OUTBOUND ,
                            PIPE_TYPE_BYTE |       // message type pipe
                            PIPE_READMODE_BYTE |   // message-read mode
                            PIPE_WAIT,             // blocking mode
                            1,                     // max. instances
                            1024 * sizeof(TCHAR),    // output buffer size
                            1024*sizeof(TCHAR),    // input buffer size
                            0,                     // client time-out
                            NULL);                 // default security attribute

    if (hNamedPipeIn == INVALID_HANDLE_VALUE) {
        log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Stdin CreateNamedPipe failed (%d): %s"), GetLastError(), getLastErrorText());
        ret = FALSE;
    } else {
            hNamedPipeOut = CreateNamedPipe(strNamedPipeNameOut, PIPE_ACCESS_INBOUND ,
                                PIPE_TYPE_MESSAGE |       // message type pipe
                                PIPE_READMODE_MESSAGE |   // message-read mode
                                PIPE_NOWAIT,              // nonblocking mode
                                1,                        // max. instances
                                512 * sizeof(TCHAR),      // output buffer size
                                512 * sizeof(TCHAR),      // input buffer size
                                0,                        // client time-out
                                NULL);                    // default security attribute

        if (hNamedPipeOut == INVALID_HANDLE_VALUE) {
            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Stdout CreateNamedPipe failed (%d): %s"), GetLastError(), getLastErrorText());
            ret = FALSE;
        } else {
            hNamedPipeErr = CreateNamedPipe(strNamedPipeNameErr, PIPE_ACCESS_INBOUND ,
                                    PIPE_TYPE_MESSAGE |       // message type pipe
                                    PIPE_READMODE_MESSAGE |   // message-read mode
                                    PIPE_NOWAIT,              // nonblocking mode
                                    1,                        // max. instances
                                    512 * sizeof(TCHAR),      // output buffer size
                                    512 * sizeof(TCHAR),      // input buffer size
                                    0,                        // client time-out
                                    NULL);                    // default security attribute

            if (hNamedPipeErr == INVALID_HANDLE_VALUE) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Stderr CreateNamedPipe failed (%d): %s"), GetLastError(), getLastErrorText());
                ret = FALSE;
            } else {
                /* Now launch the process */
                if (ShellExecuteEx(&shex) == TRUE) {
                    if (shex.hProcess != NULL) {
                        /* now read and write the pipes */
                        if (readAndWriteNamedPipes(hNamedPipeIn, hNamedPipeOut, hNamedPipeErr) != TRUE) {
                            // the error should have already been reported.
                        }
                        /* Wait up to 1 sec to check if the elevated process really exited */
                        returnValue = WaitForSingleObject(shex.hProcess, 1000);
                        if (returnValue == WAIT_OBJECT_0) {

                            ret = TRUE;
                        } else {
                            log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("The elevated process is still alive. Trying to kill it."), GetLastError(), getLastErrorText());
                            if (TerminateProcess(shex.hProcess, 1) == 0) {
                                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Couldn't kill it."), GetLastError(), getLastErrorText());
                            }
                            ret = FALSE;
                        }
                    }

                } else {
                    log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_FATAL, TEXT("Elevation failed. Wrapper will exit."), GetLastError(), getLastErrorText());
                    ret = FALSE;
                }
                CloseHandle(hNamedPipeErr);
            }
            CloseHandle(hNamedPipeOut);
        }
        CloseHandle(hNamedPipeIn);
    }


    free(strNamedPipeNameIn);
    free(strNamedPipeNameOut);
    free(strNamedPipeNameErr);

    return ret;
}


/*
 * This is just a wrapper function between elevateThis and myShellExec filling in the verb
 * For more information please refer to myShellExec
 */
BOOL runElevated(__in LPCTSTR pszPath, __in_opt LPCTSTR pszParameters, __in_opt LPCTSTR pszDirectory, TCHAR* namedPipeName) {
    return myShellExec(NULL, TEXT("runas"), pszPath, pszParameters, pszDirectory, namedPipeName);
}

/*
 * This is the entry point on the user side for creating an elevated process.
 * UAC does not allow to give a running process elevated privileges, so the
 * wrapper has to create a copy of the current process, arm it with elevated
 * privileges and take care of IPC.
 *
 * @return TRUE if OK, FALSE otherwise.
 */
BOOL elevateThis(int argc, TCHAR **argv) {
    int i, namedPipeInserted = 0, ret = FALSE;
    size_t len = 0;
    TCHAR szPath[_MAX_PATH];
    TCHAR *parameter;
    TCHAR* strNamedPipeName;

    /* get the file name of the binary, we can't trust argv[0] as the working
     * directory might have been changed.
     */
    if (GetModuleFileName(NULL, szPath, _MAX_PATH)) {
        /* seed the pseudo-random generator */
        srand((unsigned)time(NULL));
        strNamedPipeName = malloc(sizeof(TCHAR) * 11);
        if (!strNamedPipeName) {
            outOfMemory(TEXT("MSE"), 1);
            return FALSE;
        }
        /* create a pseudo-random 10 digit string */
        _sntprintf(strNamedPipeName, 11, TEXT("%05d%05d"), rand() % 100000, rand() % 100000);
        /* ShellExecuteEx is expecting the parameter in a single string */
        for (i = 1; i < argc; i++) {
            /* if '--' was specified, wrapperParseArguments has replaced this parameter with NULL */
            if (argv[i] == NULL) {
                len += 3;
            } else {
                /* insert a space and qoutes */
                len += _tcslen(argv[i]) + 3;
            }
        }
        /* add space for the namedpipe and console flush property */
        len += _tcslen(strNamedPipeName) + 28 + 27;
        parameter = calloc(len, sizeof(TCHAR));
        if (!parameter) {
            outOfMemory(TEXT("ET"), 1);
            return FALSE;
        }
        /* now fill the parameter */
        for (i = 1; i < argc; i++) {
            if (i != 1) {
                _tcsncat(parameter, TEXT(" "), len);
            }
            if (argv[i] == NULL) {
                /* the additional parameters have to be inserted before the '--'. */
                _tcsncat(parameter, TEXT("wrapper.console.flush=true wrapper.internal.namedpipe="), len);
                _tcsncat(parameter, strNamedPipeName, len);
                namedPipeInserted = 1;
                _tcsncat(parameter, TEXT(" --"), len);
            } else {
                _tcsncat(parameter, TEXT("\""), len);
                _tcsncat(parameter, argv[i], len);
                _tcsncat(parameter, TEXT("\""), len);
            }
        }
        if (!namedPipeInserted) {
            _tcsncat(parameter, TEXT(" wrapper.console.flush=true wrapper.internal.namedpipe="), len);
            _tcsncat(parameter, strNamedPipeName, len);
        }
        ret = runElevated(szPath, parameter, NULL, strNamedPipeName);
        free(strNamedPipeName);
        free(parameter);
        return ret;
    }
    return FALSE;

}
#endif /* ifdef WIN32 */
