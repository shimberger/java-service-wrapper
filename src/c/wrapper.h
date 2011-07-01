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

#ifndef _WRAPPER_H
#define _WRAPPER_H

#ifdef WIN32
 #include <tchar.h>
#endif

#include <locale.h>

#ifdef WIN32
 #include <winsock.h>

#else /* UNIX */
 #include <sys/types.h>
 #include <time.h>
 #include <unistd.h>
 #ifndef MACOSX
  #define u_short unsigned short
 #endif /* MACOSX */
#endif

#ifndef DWORD
#define DWORD unsigned long
#endif

#include <sys/timeb.h>

#include "property.h"

/* The following define will enable debug output of the code to parse the JVM output. */
/*#define DEBUG_CHILD_OUTPUT*/

/* Initialize the timerTicks to a very high value.  This means that we will
 *  always encounter the first rollover (512 * WRAPPER_MS / 1000) seconds
 *  after the Wrapper the starts, which means the rollover will be well
 *  tested. */
#define WRAPPER_TICK_INITIAL 0xfffffe00

#define WRAPPER_TICK_MS 100 /* The number of ms that are represented by a single
                             *  tick.  Ticks are used as an alternative time
                             *  keeping method. See the wrapperGetTicks() and
                             *  wrapperGetTickAgeSeconds() functions for more information.
                             * Some code assumes that this number can be evenly
                             *  divided into 1000. */

#define WRAPPER_MAX_UPTIME_SECONDS 365 * 24 * 3600 /* Maximum uptime count. 1 year. */
#define WRAPPER_MAX_UPTIME_TICKS (WRAPPER_MAX_UPTIME_SECONDS * (1000 / WRAPPER_TICK_MS)) /* The paranthesis are important to avoid overflow. */

#define WRAPPER_TIMER_FAST_THRESHOLD (2 * 24 * 3600 * 1000 / WRAPPER_TICK_MS) /* Default to 2 days. */
#define WRAPPER_TIMER_SLOW_THRESHOLD (2 * 24 * 3600 * 1000 / WRAPPER_TICK_MS) /* Default to 2 days. */

#define WRAPPER_BACKEND_TYPE_UNKNOWN 0 /* Unknown type. */
#define WRAPPER_BACKEND_TYPE_SOCKET  1 /* Use a loopback socket to communicate. */
#define WRAPPER_BACKEND_TYPE_PIPE    2 /* Use a pair of pipes to communicate. */

#define WRAPPER_WSTATE_STARTING  51 /* Wrapper is starting.  Remains in this state
                                     *  until the JVM enters the STARTED state or
                                     *  the wrapper jumps into the STOPPING state
                                     *  in response to the JVM application asking
                                     *  to shut down. */
#define WRAPPER_WSTATE_STARTED   52 /* The JVM has entered the STARTED state.
                                     *  The wrapper will remain in this state
                                     *  until the wrapper decides to shut down.
                                     *  This is true even when the JVM process
                                     *  is being restarted. */
#define WRAPPER_WSTATE_PAUSING   53 /* The wrapper enters this state when asked to Pause. */
#define WRAPPER_WSTATE_PAUSED    54 /* The wrapper enters this state when the Wrapper
                                     *  has actually paused. */
#define WRAPPER_WSTATE_RESUMING  55 /* The wrapper enters this state when asked to Resume. */
#define WRAPPER_WSTATE_STOPPING  56 /* The wrapper is shutting down.  Will be in
                                     *  this state until the JVM enters the DOWN
                                     *  state. */
#define WRAPPER_WSTATE_STOPPED   57 /* The wrapper enters this state just before
                                     *  it exits. */

#define WRAPPER_JSTATE_DOWN_CHECK 70 /* JVM is confirmed to be down, but we still need
                                     *  to do our cleanup work.  This is the state after
                                     *  a JVM process has gone away. */
#define WRAPPER_JSTATE_DOWN_CLEAN 71 /* JVM is confirmed to be down and we have cleaned
                                     *  up.  This is the initial state and the state
                                     *  after the JVM process has gone away and and
                                     *  cleanup has been done. */
#define WRAPPER_JSTATE_LAUNCH_DELAY 72 /* Set from the DOWN state to launch a JVM.  The
                                     *  timeout will be the time to actually launch
                                     *  the JVM after any required delay. */
#define WRAPPER_JSTATE_RESTART   73 /* JVM is about to be restarted. No timeout. */
#define WRAPPER_JSTATE_LAUNCH    74 /* JVM is about to launch a JVM. No timeout. */
#define WRAPPER_JSTATE_LAUNCHING 75 /* JVM was launched, but has not yet responded.
                                     *  Must enter the LAUNCHED state before <t>
                                     *  or the JVM will be killed. */
#define WRAPPER_JSTATE_LAUNCHED  76 /* JVM was launched, and responed to a ping. */
#define WRAPPER_JSTATE_STARTING  77 /* JVM has been asked to start.  Must enter the
                                     *  STARTED state before <t> or the JVM will be
                                     *  killed. */
#define WRAPPER_JSTATE_STARTED   78 /* JVM has responded that it is running.  Must
                                     *  respond to a ping by <t> or the JVM will
                                     *  be killed. */
#define WRAPPER_JSTATE_STOP      79 /* JVM is about to be sent a stop command to shutdown
                                     *  cleanly. */
#define WRAPPER_JSTATE_STOPPING  80 /* JVM was sent a stop command, but has not yet
                                     *  responded.  Must enter the STOPPED state
                                     *  and exit before <t> or the JVM will be killed. */
#define WRAPPER_JSTATE_STOPPED   81 /* JVM has responed that it is stopped. */
#define WRAPPER_JSTATE_KILLING   82 /* The Wrapper is about ready to kill the JVM
                                     *  process but it must wait a few moments before
                                     *  actually doing so.  After <t> has expired, the
                                     *  JVM will be killed and we will enter the STOPPED
                                     *  state. */
#define WRAPPER_JSTATE_KILL      83 /* The Wrapper is about ready to kill the JVM process. */

/* Defined Action types.  Registered actions are negative.  Custom types are positive. */
#define ACTION_LIST_END          0
#define ACTION_NONE              -1
#define ACTION_RESTART           -2
#define ACTION_SHUTDOWN          -3
#define ACTION_DUMP              -4
#define ACTION_DEBUG             -5
#define ACTION_PAUSE             -6
#define ACTION_RESUME            -7
#define ACTION_SUCCESS           -8
#define ACTION_GC                -9
#if defined(MACOSX)
#define TRIGGER_ADVICE_NIL_SERVER TEXT("****** Returning nil _server **********")
#define ACTION_ADVICE_NIL_SERVER -32
#endif

#define WRAPPER_ACTION_SOURCE_CODE_FILTER                  1  /* Action originated with a filter. */
#define WRAPPER_ACTION_SOURCE_CODE_COMMANDFILE             2  /* Action originated from a commandfile. */
#define WRAPPER_ACTION_SOURCE_CODE_WINDOWS_SERVICE_MANAGER 3  /* Action originated from the Windows Service Manager. */
#define WRAPPER_ACTION_SOURCE_CODE_ON_EXIT                 4  /* Action originated from an on_exit configuration. */

/* Because of the way time is converted to ticks, the largest possible timeout that
 *  can be specified without causing 32-bit overflows is (2^31 / 1000) - 5 = 2147478
 *  Which is a little over 24 days.  To make the interface nice, round this down to
 *  20 days.  Or 1728000. */
#define WRAPPER_TIMEOUT_MAX      1728000

#define WRAPPER_IGNORE_SIGNALS_WRAPPER 1
#define WRAPPER_IGNORE_SIGNALS_JAVA    2

#define WRAPPER_RESTART_REQUESTED_NO 0
#define WRAPPER_RESTART_REQUESTED_INITIAL 1
#define WRAPPER_RESTART_REQUESTED_AUTOMATIC 2
#define WRAPPER_RESTART_REQUESTED_CONFIGURED 4

#ifdef JSW64
typedef unsigned int TICKS;
#else
typedef unsigned long TICKS;
#endif

#define WRAPPER_ENV_SOURCE_PARENT      1
#define WRAPPER_ENV_SOURCE_WRAPPER     2
#ifdef WIN32
#define WRAPPER_ENV_SOURCE_REG_SYSTEM  4
#define WRAPPER_ENV_SOURCE_REG_ACCOUNT 8
#endif

#ifdef WIN32
/* Defines the maximum number of service manager control events that can be queued in a single loop. */
#define CTRL_CODE_QUEUE_SIZE 26 /* Can enqueue one less than this count at any time. */
#endif

/* Type definitions */
typedef struct WrapperConfig WrapperConfig;
struct WrapperConfig {
    TCHAR   *argCommand;            /* The command used to launch the wrapper. */
    TCHAR   *argCommandArg;         /* The argument to the command used to launch the wrapper. */
    TCHAR   *argConfFile;           /* The name of the config file from the command line. */
    TCHAR   *confDir;
    int     argConfFileDefault;     /* True if the config file was not specified. */
    int     argConfFileFound;       /* True if the config file was found. */
    int     argCount;               /* The total argument count. */
    TCHAR   **argValues;            /* Argument values. */
    TCHAR   **javaArgValues;        /* Arguments getting passed over to the java application */
    int     javaArgValueCount;      /* Number of the arguments getting passed over to the java application */

    TCHAR   *initialPath;           /* What the working directory was when the Wrapper process was first launched. */
    TCHAR   *language;              /* The language */
    int     backendType;            /* The type of the backend that the Wrapper and Java use to communicate. */
    int     configured;             /* TRUE if loadConfiguration has been called. */
    int     useSystemTime;          /* TRUE if the wrapper should use the system clock for timing, FALSE if a tick counter should be used. */
    int     timerFastThreshold;     /* If the difference between the system time based tick count and the timer tick count ever falls by more than this value then a warning will be displayed. */
    int     timerSlowThreshold;     /* If the difference between the system time based tick count and the timer tick count ever grows by more than this value then a warning will be displayed. */
    int     useTickMutex;           /* TRUE if access to the tick count should be protected by a mutex. */
    int     uptimeFlipped;          /* TRUE when the maximum uptime has been flipped. (Overflown) */

    int     ignoreSequenceGaps;     /* TRUE if all sequence properties should be used. */
    int     port;                   /* Port number which the Wrapper is configured to be listening on */
    int     portMin;                /* Minimum port to use when looking for a port to bind to. */
    int     portMax;                /* Maximum port to use when looking for a port to bind to. */
    int     actualPort;             /* Port number which the Wrapper is actually listening on */
    int     jvmPort;                /* The port which the JVM should bind to when connecting back to the wrapper. */
    int     jvmPortMin;             /* Minimum port which the JVM should bind to when connecting back to the wrapper. */
    int     jvmPortMax;             /* Maximum port which the JVM should bind to when connecting back to the wrapper. */
    int     sock;                   /* Socket number. if open. */
    TCHAR   *originalWorkingDir;    /* Original Wrapper working directory. */
    TCHAR   *workingDir;            /* Configured working directory. */
    TCHAR   *configFile;            /* Name of the configuration file */
    int     commandLogLevel;        /* The log level to use when logging the java command. */
#ifdef WIN32
    TCHAR   *jvmCommand;            /* Command used to launch the JVM */
#else /* UNIX */
    TCHAR   **jvmCommand;           /* Command used to launch the JVM */
#endif
    int     detachStarted;          /* TRUE if the JVM process should be detached once it has started. */
    int     environmentClasspath;   /* TRUE if the classpath should be passed to the JVM in the environment. */
    TCHAR   *classpath;             /* Classpath to pass to the JVM. */
    int     debugJVM;               /* True if the JVM is being launched with a debugger enabled. */
    int     debugJVMTimeoutNotified;/* True if the JVM is being launched with a debugger enabled and the user has already been notified of a timeout. */
    TCHAR   key[17];                /* Key which the JVM uses to authorize connections. (16 digits + \0) */
    int     isConsole;              /* TRUE if the wrapper was launched as a console. */
    int     cpuTimeout;             /* Number of seconds without CPU before the JVM will issue a warning and extend timeouts */
    int     startupTimeout;         /* Number of seconds the wrapper will wait for a JVM to startup */
    int     pingTimeout;            /* Number of seconds the wrapper will wait for a JVM to reply to a ping */
    int     pingInterval;           /* Number of seconds between pinging the JVM */
    int     pingIntervalLogged;     /* Number of seconds between pings which can be logged to debug output. */
    int     shutdownTimeout;        /* Number of seconds the wrapper will wait for a JVM to shutdown */
    int     jvmExitTimeout;         /* Number of seconds the wrapper will wait for a JVM to process to terminate */
    int     jvmCleanupTimeout;      /* Number of seconds the wrapper will allow for its post JVM shudown cleanup. */
    int     useJavaIOThread;        /* If TRUE then a dedicated thread will be used to process console output form the JVM. */
    int     pauseThreadMain;        /* Number of seconds to pause the main thread on its next loop.  Only used for testing. */
    int     pauseThreadTimer;       /* Number of seconds to pause the timer thread on its next loop.  Only used for testing. */
    int     pauseThreadJavaIO;      /* Number of seconds to pause the javaio thread on its next loop.  Only used for testing. */

#ifdef WIN32
    int     ignoreUserLogoffs;      /* If TRUE, the Wrapper will ignore logoff events when run in the background as an in console mode. */
    TCHAR   *userName;              /* The username (account) of the Wrapper process. */
    TCHAR   *domainName;            /* The domain of the Wrapper process. */
    DWORD   wrapperPID;             /* PID of the Wrapper process. */
    DWORD   javaPID;                /* PID of the Java process. */
    HANDLE  wrapperProcess;         /* Handle of the Wrapper process. */
    HANDLE  javaProcess;            /* Handle of the Java process. */
#else
    pid_t   wrapperPID;             /* PID of the Wrapper process. */
    pid_t   javaPID;                /* PID of the Java process. */
#endif
    int     wState;                 /* The current state of the wrapper */
    int     jState;                 /* The current state of the jvm */
    TICKS   jStateTimeoutTicks;     /* Tick count until which the current jState is valid */
    int     jStateTimeoutTicksSet;  /* 1 if the current jStateTimeoutTicks is set. */
    TICKS   lastPingTicks;          /* Time that the last ping was sent */
    TICKS   lastLoggedPingTicks;    /* Time that the last logged ping was sent */

    int     isDebugging;            /* TRUE if set in the configuration file */
    int     isAdviserEnabled;       /* TRUE if advice messages should be output. */
    const TCHAR *nativeLibrary;     /* The base name of the native library loaded by the WrapperManager. */
    int     libraryPathAppendPath;  /* TRUE if the PATH environment variable should be appended to the java library path. */
    int     isStateOutputEnabled;   /* TRUE if set in the configuration file.  Shows output on the state of the state engine. */
    int     isJavaIOOutputEnabled;  /* TRUE if detailed javaIO output should be included in debug output. */
    int     isTickOutputEnabled;    /* TRUE if detailed tick timer output should be included in debug output. */
    int     isLoopOutputEnabled;    /* TRUE if very detailed output from the main loop should be output. */
    int     isSleepOutputEnabled;   /* TRUE if detailed sleep output should be included in debug output. */
    int     isMemoryOutputEnabled;  /* TRUE if detailed memory output should be included in status output. */
    int     memoryOutputInterval;   /* Interval in seconds at which memory usage is logged. */
    TICKS   memoryOutputTimeoutTicks; /* Tick count at which memory will next be logged. */
    int     isCPUOutputEnabled;     /* TRUE if detailed CPU output should be included in status output. */
    int     cpuOutputInterval;      /* Interval in seconds at which CPU usage is logged. */
    TICKS   cpuOutputTimeoutTicks;  /* Tick count at which CPU will next be logged. */
    int     isPageFaultOutputEnabled;/* TRUE if detailed PageFault output should be included in status output. */
    int     pageFaultOutputInterval;/* Interval in seconds at which PageFault usage is logged. */
    TICKS   pageFaultOutputTimeoutTicks; /* Tick count at which PageFault will next be logged. */
    int     logfileInactivityTimeout; /* The number of seconds of inactivity before the logfile will be closed. */
    TICKS   logfileInactivityTimeoutTicks; /* Tick count at which the logfile will be considered inactive and closed. */
    int     isShutdownHookDisabled; /* TRUE if set in the configuration file */
    int     startupDelayConsole;    /* Delay in seconds before starting the first JVM in console mode. */
    int     startupDelayService;    /* Delay in seconds before starting the first JVM in service mode. */
    int     exitCode;               /* Code which the wrapper will exit with */
    int     exitRequested;          /* TRUE if the current JVM should be shutdown. */
    int     restartRequested;       /* WRAPPER_RESTART_REQUESTED_NO, WRAPPER_RESTART_REQUESTED_AUTOMATIC, or WRAPPER_RESTART_REQUESTED_CONFIGURED if the another JVM should be launched after the current JVM is shutdown. Only set if exitRequested is set. */
    int     jvmRestarts;            /* Number of times that a JVM has been launched since the wrapper was started. */
    int     restartDelay;           /* Delay in seconds before restarting a new JVM. */
    int     restartReloadConf;      /* TRUE if the configuration should be reloaded before a JVM restart. */
    int     isRestartDisabled;      /* TRUE if restarts should be disabled. */
    int     isAutoRestartDisabled;  /* TRUE if automatic restarts should be disabled. */
    int     requestThreadDumpOnFailedJVMExit; /* TRUE if the JVM should be asked to dump its state when it fails to halt on request. */
    int     requestThreadDumpOnFailedJVMExitDelay; /* Number of seconds to wait after the thread dump before killing the JVM. */
    TICKS   jvmLaunchTicks;         /* The tick count at which the previous or current JVM was launched. */
    int     failedInvocationCount;  /* The number of times that the JVM exited in less than successfulInvocationTime in a row. */
    int     successfulInvocationTime;/* Amount of time that a new JVM must be running so that the invocation will be considered to have been a success, leading to a reset of the restart count. */
    int     maxFailedInvocations;   /* Maximum number of failed invocations in a row before the Wrapper will give up and exit. */
    int     outputFilterCount;      /* Number of registered output filters. */
    TCHAR   **outputFilters;        /* Array of output filters. */
    int     **outputFilterActionLists;/* Array of output filter action lists. */
    TCHAR   **outputFilterMessages; /* Array of output filter messages. */
    int     *outputFilterAllowWildFlags; /* Array of output filter flags that say whether or not wild cards in the filter can be processed. */
    size_t  *outputFilterMinLens;   /* Array of the minimum text lengths that could possibly match the specified filter.  Only used if it contains wildcards. */
    TCHAR   *pidFilename;           /* Name of file to store wrapper pid in */
    TCHAR   *lockFilename;          /* Name of file to store wrapper lock in */
    TCHAR   *javaPidFilename;       /* Name of file to store jvm pid in */
    TCHAR   *javaIdFilename;        /* Name of file to store jvm id in */
    TCHAR   *statusFilename;        /* Name of file to store wrapper status in */
    TCHAR   *javaStatusFilename;    /* Name of file to store jvm status in */
    TCHAR   *commandFilename;       /* Name of a command file used to send commands to the Wrapper. */
    int     commandFileTests;       /* True if test commands will be accepted via the command file. */
    int     commandPollInterval;    /* Interval in seconds at which the existence of the command file is polled. */
    TICKS   commandTimeoutTicks;    /* Tick count at which the command file will be checked next. */
    TCHAR   *anchorFilename;        /* Name of an anchor file used to control when the Wrapper should quit. */
    int     anchorPollInterval;     /* Interval in seconds at which the existence of the anchor file is polled. */
    TICKS   anchorTimeoutTicks;     /* Tick count at which the anchor file will be checked next. */
    int     umask;                  /* Default umask for all files. */
    int     javaUmask;              /* Default umask for the java process. */
    int     pidFileUmask;           /* Umask to use when creating the pid file. */
    int     lockFileUmask;          /* Umask to use when creating the lock file. */
    int     javaPidFileUmask;       /* Umask to use when creating the java pid file. */
    int     javaIdFileUmask;        /* Umask to use when creating the java id file. */
    int     statusFileUmask;        /* Umask to use when creating the status file. */
    int     javaStatusFileUmask;    /* Umask to use when creating the java status file. */
    int     anchorFileUmask;        /* Umask to use when creating the anchor file. */
    int     ignoreSignals;          /* Mask that determines where the Wrapper should ignore any catchable system signals.  Can be ingored in the Wrapper and/or JVM. */
    TCHAR   *consoleTitle;          /* Text to set the console title to. */
    TCHAR   *serviceName;           /* Name of the service. */
    TCHAR   *serviceDisplayName;    /* Display name of the service. */
    TCHAR   *serviceDescription;    /* Description for service. */
    TCHAR   *hostName;              /* The name of the current host. */
    int     pausable;               /* Should the service be allowed to be paused? */
    int     pausableStopJVM;        /* Should the JVM be stopped when the service is paused? */
    int     initiallyPaused;        /* Should the Wrapper come up initially in a paused state? */

#ifdef WIN32
    int     isSingleInvocation;     /* TRUE if only a single invocation of an application should be allowed to launch. */
    TCHAR   *ntServiceLoadOrderGroup; /* Load order group name. */
    TCHAR   *ntServiceDependencies; /* List of Dependencies */
    int     ntServiceStartType;     /* Mode in which the Service is installed.
                                     * {SERVICE_AUTO_START | SERVICE_DEMAND_START} */
    DWORD   ntServicePriorityClass; /* Priority at which the Wrapper and its JVMS will run.
                                     * {HIGH_PRIORITY_CLASS | IDLE_PRIORITY_CLASS | NORMAL_PRIORITY_CLASS | REALTIME_PRIORITY_CLASS} */
    TCHAR   *ntServiceAccount;      /* Account name to use when running as a service.  NULL to use the LocalSystem account. */
    TCHAR   *ntServicePassword;     /* Password to use when running as a service.  NULL means no password. */
    int     ntServicePrompt;        /* If true then the user will be prompted for a account name, domain,  password when installing as a service. */
    int     ntServicePasswordPrompt; /* If true then the user will be prompted for a password when installing as a service. */
    int     ntServicePasswordPromptMask; /* If true then the password will be masked as it is input. */
    int     ntServiceInteractive;   /* Should the service be allowed to interact with the desktop? */
    int     ntHideJVMConsole;       /* Should the JVMs Console window be hidden when run as a service.  True by default but GUIs will not be visible for JVMs prior to 1.4.0. */
    int     ntHideWrapperConsole;   /* Should the Wrapper Console window be hidden when run as a service. */
    int     wrapperConsoleHide;     /* True if the Wrapper Console window should be hidden. */
    HWND    wrapperConsoleHandle;   /* Pointer to the Wrapper Console handle if it exists.  This will only be set if the console was allocated then hidden. */
    int     wrapperConsoleVisible;  /* True if the Wrapper Console window is visible. */
    HWND    jvmConsoleHandle;       /* Pointer to the JVM Console handle if it exists. */
    int     jvmConsoleVisible;      /* True if the JVM Console window is visible. */
    int     ntAllocConsole;         /* True if a console should be allocated for the Service. */
    int     generateConsole;        /* Make sure that a console is always generated to support thread dumps */
    int     threadDumpControlCode;  /* Control code which can be used to trigger a thread dump. */
#else /* UNIX */
    int     daemonize;              /* TRUE if the process  should be spawned as a daemon process on launch. */
    int     signalHUPMode;          /* Controls what happens when the Wrapper receives a HUP signal. */
    int     signalUSR1Mode;         /* Controls what happens when the Wrapper receives a USR1 signal. */
    int     signalUSR2Mode;         /* Controls what happens when the Wrapper receives a USR2 signal. */
    int     jvmStopped;             /* Flag which remembers the the stopped state of the JVM process. */
#endif

#ifdef WIN32
    int     ctrlEventCTRLCTrapped;  /* CTRL_C_EVENT trapped. */
    int     ctrlEventCloseTrapped;  /* CTRL_CLOSE_EVENT trapped. */
    int     ctrlEventLogoffTrapped; /* CTRL_LOGOFF_EVENT trapped. */
    int     ctrlEventShutdownTrapped;/* CTRL_SHUTDOWN_EVENT trapped. */
    int     *ctrlCodeQueue;         /* Queue of control code ids trapped. */
    int     ctrlCodeQueueWriteIndex;
    int     ctrlCodeQueueReadIndex;
    int     ctrlCodeQueueWrapped;
    int     ctrlCodePauseTrapped;   /* SERVICE_CONTROL_PAUSE was trapped. */
    int     ctrlCodeContinueTrapped;/* SERVICE_CONTROL_CONTINUE was trapped. */
    int     ctrlCodeStopTrapped;    /* SERVICE_CONTROL_STOP was trapped. */
    int     ctrlCodeShutdownTrapped;/* SERVICE_CONTROL_SHUTDOWN was trapped. */
    int     ctrlCodeDumpTrapped;    /* The configured thread dump control code was trapped. */
#else
    int     signalInterruptTrapped; /* SIGINT was trapped. */
    int     signalQuitTrapped;      /* SIGQUIT was trapped. */
    int     signalChildTrapped;     /* SIGCHLD was trapped. */
    int     signalTermTrapped;      /* SIGTERM was trapped. */
    int     signalHUPTrapped;       /* SIGHUP was trapped. */
    int     signalUSR1Trapped;      /* SIGUSR1 was trapped. */
    int     signalUSR2Trapped;      /* SIGUSR2 was trapped. */
#endif
};

#define WRAPPER_SIGNAL_MODE_IGNORE   (char)100
#define WRAPPER_SIGNAL_MODE_RESTART  (char)101
#define WRAPPER_SIGNAL_MODE_SHUTDOWN (char)102
#define WRAPPER_SIGNAL_MODE_FORWARD  (char)103

#define WRAPPER_MSG_START         (char)100
#define WRAPPER_MSG_STOP          (char)101
#define WRAPPER_MSG_RESTART       (char)102
#define WRAPPER_MSG_PING          (char)103
#define WRAPPER_MSG_STOP_PENDING  (char)104
#define WRAPPER_MSG_START_PENDING (char)105
#define WRAPPER_MSG_STARTED       (char)106
#define WRAPPER_MSG_STOPPED       (char)107
#define WRAPPER_MSG_KEY           (char)110
#define WRAPPER_MSG_BADKEY        (char)111
#define WRAPPER_MSG_LOW_LOG_LEVEL (char)112
#define WRAPPER_MSG_PING_TIMEOUT  (char)113 /* No longer used. */
#define WRAPPER_MSG_SERVICE_CONTROL_CODE (char)114
#define WRAPPER_MSG_PROPERTIES    (char)115
/** Log commands are actually 116 + the LOG LEVEL (LEVEL_UNKNOWN ~ LEVEL_NONE), (116 ~ 124). */
#define WRAPPER_MSG_LOG           (char)116
#define WRAPPER_MSG_LOGFILE       (char)134
#define WRAPPER_MSG_APPEAR_ORPHAN (char)137 /* No longer used. */
#define WRAPPER_MSG_PAUSE         (char)138
#define WRAPPER_MSG_RESUME        (char)139
#define WRAPPER_MSG_GC            (char)140
#define WRAPPER_PROCESS_DOWN      200
#define WRAPPER_PROCESS_UP        201

extern WrapperConfig *wrapperData;
extern Properties    *properties;

extern TCHAR wrapperClasspathSeparator;

/* Protocol Functions */
/**
 * Close the backend socket.
 */
extern void wrapperProtocolClose();

/**
 * Sends a command to the JVM process.
 *
 * @param function The command to send.  (This is intentionally an 8-bit char.)
 * @param message Message to send along with the command.
 *
 * @return TRUE if there were any problems.
 */
extern int wrapperProtocolFunction(char function, const TCHAR *message);

/**
 * Checks the status of the server backend.
 *
 * The backend will be initialized if the JVM is in a state where it should
 *  be up, otherwise the backend will be left alone.
 *
 * If the forceOpen flag is set then an attempt will be made to initialize
 *  the backend regardless of the JVM state.
 *
 * Returns TRUE if the backend is open and ready on return, FALSE if not.
 */
extern int wrapperCheckServerBackend(int forceOpen);

/**
 * Read any data sent from the JVM.  This function will loop and read as many
 *  packets are available.  The loop will only be allowed to go for 250ms to
 *  ensure that other functions are handled correctly.
 *
 * Returns 0 if all available data has been read, 1 if more data is waiting.
 */
extern int wrapperProtocolRead();

/******************************************************************************
 * Utility Functions
 *****************************************************************************/
/**
 * Test function to pause the current thread for the specified amount of time.
 *  This is used to test how the rest of the Wrapper behaves when a particular
 *  thread blocks for any reason.
 *
 * @param pauseTime Number of seconds to pause for.  -1 will pause indefinitely.
 * @param threadName Name of the thread that will be logged prior to pausing.
 */
extern void wrapperPauseThread(int pauseTime, const TCHAR *threadName);

/**
 * Function that will recursively attempt to match two strings where the
 *  pattern can contain '?' or '*' wildcard characters.
 *
 * @param text Text to be searched.
 * @param pattern Pattern to search for.
 * @param patternLen Length of the pattern.
 * @param minTextLen Minimum number of characters that the text needs to possibly match the pattern.
 *
 * @return TRUE if found, FALSE otherwise.
 */
extern int wrapperWildcardMatch(const TCHAR *text, const TCHAR *pattern, size_t minTextLen);

/**
 * Calculates the minimum text length which could be matched by the specified pattern.
 *  Patterns can contain '*' or '?' wildcards.
 *  '*' matches 0 or more characters.
 *  '?' matches exactly one character.
 *
 * @param pattern Pattern to calculate.
 *
 * @return The minimum text length of the pattern.
 */
extern size_t wrapperGetMinimumTextLengthForPattern(const TCHAR *pattern);

/**
 * Returns a constant text representation of the specified Wrapper State.
 *
 * @param wState The Wrapper State whose name is being requested.
 *
 * @return Thre requested Wrapper State.
 */
extern const TCHAR *wrapperGetWState(int wState);

/**
 * Returns a constant text representation of the specified Java State.
 *
 * @param jState The Java State whose name is being requested.
 *
 * @return Thre requested Java State.
 */
extern const TCHAR *wrapperGetJState(int jState);

extern struct tm wrapperGetReleaseTime();
extern struct tm wrapperGetBuildTime();

extern void disposeJavaIO();
extern void disposeTimer();

extern int showHostIds(int logLevel);
extern void wrapperLoadHostName();

/**
 * Parses a list of actions for an action property.
 *
 * @param actionNameList A space separated list of action names.
 * @param propertyName The name of the property where the action name originated.
 *
 * @return an array of integer action ids, or NULL if there were any problems.
 */
extern int *wrapperGetActionListForNames(const TCHAR *actionNameList, const TCHAR *propertyName);

/**
 * Performs the specified action,
 *
 * @param actionList An array of action Ids ending with a value ACTION_LIST_END.
 *                   Negative values are standard actions, positive are user
 *                   custom events.
 * @param triggerMsg The reason the actions are being fired.
 * @param actionCode Tracks where the action originated.
 * @param logForActionNone Flag stating whether or not a message should be logged
 *                         for the NONE action.
 * @param exitCode Error code to use in case the action results in a shutdown.
 */
extern void wrapperProcessActionList(int *actionList, const TCHAR *triggerMsg, int actionCode, int logForActionNone, int exitCode);

extern void wrapperAddDefaultProperties();

extern int wrapperLoadConfigurationProperties();

extern void wrapperGetCurrentTime(struct timeb *timeBuffer);

#ifdef WIN32

extern void wrapperInitializeProfileCounters();
extern void wrapperDumpPageFaultUsage();

extern void updateStringValue(TCHAR **ptr, const TCHAR *value);

extern TCHAR** wrapperGetSystemPath();
extern int wrapperGetJavaHomeFromWindowsRegistry(TCHAR *javaHome);
#endif
extern int wrapperCheckRestartTimeOK();

extern int wrapperBuildJavaClasspath(TCHAR **classpath);

/**
 * command is a pointer to a pointer of an array of character strings.
 * length is the number of strings in the above array.
 */
extern int wrapperBuildJavaCommandArray(TCHAR ***strings, int *length, int addQuotes, const TCHAR *classpath);
extern void wrapperFreeJavaCommandArray(TCHAR **strings, int length);

extern int wrapperInitialize();
extern void wrapperDispose();

/**
 * Returns the file name base as a newly malloced TCHAR *.  The resulting
 *  base file name will have any path and extension stripped.
 *
 * baseName should be long enough to always contain the base name.
 *  (strlen(fileName) + 1) is safe.
 */
extern void wrapperGetFileBase(const TCHAR *fileName, TCHAR *baseName);

/**
 * Output the version.
 */
extern void wrapperVersionBanner();

/**
 * Output the application usage.
 */
extern void wrapperUsage(TCHAR *appName);

/**
 * Parse the main arguments.
 *
 * Returns FALSE if the application should exit with an error.  A message will
 *  already have been logged.
 */
extern int wrapperParseArguments(int argc, TCHAR **argv);

/**
 * Called when the Wrapper detects that the JVM process has exited.
 *  Contains code common to all platforms.
 */
extern void wrapperJVMProcessExited(TICKS nowTicks, int exitCode);

/**
 * Read and process any output from the child JVM Process.
 * Most output should be logged to the wrapper log file.
 *
 * This function will only be allowed to run for 250ms before returning.  This is to
 *  make sure that the main loop gets CPU.  If there is more data in the pipe, then
 *  the function returns TRUE, otherwise FALSE.  This is a hint to the mail loop not to
 *  sleep.
 */
extern int wrapperReadChildOutput();

/**
 * Changes the current Wrapper state.
 *
 * wState - The new Wrapper state.
 */
extern void wrapperSetWrapperState(int wState);

/**
 * Updates the current state time out.
 *
 * nowTicks - The current tick count at the time of the call, may be -1 if
 *            delay is negative.
 * delay - The delay in seconds, added to the nowTicks after which the state
 *         will time out, if negative will never time out.
 */
extern void wrapperUpdateJavaStateTimeout(TICKS nowTicks, int delay);


/**
 * Changes the current Java state.
 *
 * jState - The new Java state.
 * nowTicks - The current tick count at the time of the call, may be -1 if
 *            delay is negative.
 * delay - The delay in seconds, added to the nowTicks after which the state
 *         will time out, if negative will never time out.
 */
extern void wrapperSetJavaState(int jState, TICKS nowTicks, int delay);

/******************************************************************************
 * Platform specific methods
 *****************************************************************************/
#ifdef WIN32
extern void wrapperCheckConsoleWindows();
/**
 *   checks the digital Signature of the binary and reports the result.
 */
extern BOOL verifyEmbeddedSignature();

extern int exceptionFilterFunction(PEXCEPTION_POINTERS exceptionPointers);
BOOL extern elevateThis(int argc, TCHAR **argv);
BOOL extern duplicateSTD();
BOOL extern myShellExec(HWND hwnd, LPCTSTR pszVerb, LPCTSTR pszPath, LPCTSTR pszParameters, LPCTSTR pszDirectory, TCHAR* namedPipeName);
BOOL extern runElevated( __in LPCTSTR pszPath, __in_opt LPCTSTR pszParameters, __in_opt LPCTSTR pszDirectory, TCHAR* namedPipeName);
BOOL extern isElevated();
BOOL extern isVista();
extern void wrapperMaintainControlCodes();
#else
extern void wrapperMaintainSignals();
#endif

/**
 * Gets the error code for the last operation that failed.
 */
extern int wrapperGetLastError();

/**
 * Execute initialization code to get the wrapper set up.
 */
extern int wrapperInitializeRun();

/**
 * Cause the current thread to sleep for the specified number of milliseconds.
 *  Sleeps over one second are not allowed.
 *
 * @param ms Number of milliseconds to wait for.
 *
 * @return TRUE if the was interrupted, FALSE otherwise.  Neither is an error.
 */
extern int wrapperSleep(int ms);

/**
 * Reports the status of the wrapper to the service manager
 * Possible status values:
 *   WRAPPER_WSTATE_STARTING
 *   WRAPPER_WSTATE_STARTED
 *   WRAPPER_WSTATE_STOPPING
 *   WRAPPER_WSTATE_STOPPED
 */
extern void wrapperReportStatus(int useLoggerQueue, int status, int errorCode, int waitHint);

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
extern int wrapperReadChildOutputBlock(char *blockBuffer, int blockSize, int *readCount);

/**
 * Checks on the status of the JVM Process.
 * Returns WRAPPER_PROCESS_UP or WRAPPER_PROCESS_DOWN
 */
extern int wrapperGetProcessStatus(TICKS nowTicks, int sigChild);

/**
 * Pauses before launching a new JVM if necessary.
 */
extern void wrapperPauseBeforeExecute();

/**
 * Launches a JVM process and store it internally
 */
extern void wrapperExecute();

/**
 * Returns a tick count that can be used in combination with the
 *  wrapperGetTickAgeSeconds() function to perform time keeping.
 */
extern TICKS wrapperGetTicks();

/**
 * Runs some assertion checks on the tick timer logic.
 */
extern int wrapperTickAssertions();

/**
 * Outputs a a log entry describing what the memory dump columns are.
 */
extern void wrapperDumpMemoryBanner();

/**
 * Outputs a log entry at regular intervals to track the memory usage of the
 *  Wrapper and its JVM.
 */
extern void wrapperDumpMemory();

/**
 * Outputs a log entry at regular intervals to track the CPU usage over each
 *  interval for the Wrapper and its JVM.
 */
extern void wrapperDumpCPUUsage();

/******************************************************************************
 * Wrapper inner methods.
 *****************************************************************************/
/**
 * Immediately kill the JVM process and set the JVM state to
 *  WRAPPER_JSTATE_DOWN.
 */
extern void wrapperKillProcessNow();

/**
 * Puts the Wrapper into a state where the JVM will be killed at the soonest
 *  possible opportunity.  It is necessary to wait a moment if a final thread
 *  dump is to be requested.  This call wll always set the JVM state to
 *  WRAPPER_JSTATE_KILLING.
 */
extern void wrapperKillProcess();

/**
 * Launch the wrapper as a console application.
 */
extern int wrapperRunConsole();

/**
 * Launch the wrapper as a service application.
 */
extern int wrapperRunService();

/**
 * Used to ask the state engine to pause the JVM and Wrapper
 *
 * @param actionCode Tracks where the action originated.
 */
extern void wrapperPauseProcess(int actionCode);

/**
 * Used to ask the state engine to resume the JVM and Wrapper
 *
 * @param actionCode Tracks where the action originated.
 */
extern void wrapperResumeProcess(int actionCode);

/**
 * Detaches the Java process so the Wrapper will if effect forget about it.
 */
extern void wrapperDetachJava();

/**
 * Used to ask the state engine to shut down the JVM and Wrapper.
 *
 * @param exitCode Exit code to use when shutting down.
 * @param force True to force the Wrapper to shutdown even if some configuration
 *              had previously asked that the JVM be restarted.  This will reset
 *              any existing restart requests, but it will still be possible for
 *              later actions to request a restart.
 */
extern void wrapperStopProcess(int exitCode, int force);

/**
 * Used to ask the state engine to shut down the JVM.
 */
extern void wrapperRestartProcess();

/**
 * Sends a command off to the JVM asking it to perform a garbage collection sweep.
 */
extern void wrapperRequestJVMGC();

/**
 * Loops over and strips all double quotes from prop and places the
 *  stripped version into propStripped.
 *
 * The exception is double quotes that are preceeded by a backslash
 *  in this case the backslash is stripped.
 *
 * If two backslashes are found in a row, then the first escapes the
 *  second and the second is removed.
 */
extern void wrapperStripQuotes(const TCHAR *prop, TCHAR *propStripped);

/**
 * Adds quotes around the specified string in such a way that everything is
 *  escaped correctly.  If the bufferSize is not large enough then the
 *  required size will be returned.  0 is returned if successful.
 */
extern size_t wrapperQuoteValue(const TCHAR* value, TCHAR *buffer, size_t bufferSize);

/**
 * Checks the quotes in the value and displays an error if there are any problems.
 * This can be useful to help users debug quote problems.
 */
extern int wrapperCheckQuotes(const TCHAR *value, const TCHAR *propName);

/**
 * The main event loop for the wrapper.  Handles all state changes and events.
 */
extern void wrapperEventLoop();

extern void wrapperBuildKey();

/**
 * Send a signal to the JVM process asking it to dump its JVM state.
 */
extern void wrapperRequestDumpJVMState();

/**
 * Build the java command line.
 *
 * @return TRUE if there were any problems.
 */
extern int wrapperBuildJavaCommand();

/**
 * Requests a lock on the tick mutex.
 */
extern int wrapperLockTickMutex();

/**
 * Releases a lock on the tick mutex.
 */
extern int wrapperReleaseTickMutex();

/**
 * Calculates a tick count using the system time.
 */
extern TICKS wrapperGetSystemTicks();

/**
 * Returns difference in seconds between the start and end ticks.  This function
 *  handles cases where the tick counter has wrapped between when the start
 *  and end tick counts were taken.  See the wrapperGetTicks() function.
 */
extern int wrapperGetTickAgeSeconds(TICKS start, TICKS end);

/**
 * Returns difference in ticks between the start and end ticks.  This function
 *  handles cases where the tick counter has wrapped between when the start
 *  and end tick counts were taken.  See the wrapperGetTicks() function.
 *
 * This can be done safely in 32 bits
 */
extern int wrapperGetTickAgeTicks(TICKS start, TICKS end);

/**
 * Returns TRUE if the specified tick timeout has expired relative to the
 *  specified tick count.
 */
extern int wrapperTickExpired(TICKS nowTicks, TICKS timeoutTicks);

/**
 * Returns a tick count that is the specified number of seconds later than
 *  the base tick count.
 */
extern TICKS wrapperAddToTicks(TICKS start, int seconds);

/**
 * Sets the working directory of the Wrapper to the specified directory.
 *  The directory can be relative or absolute.
 * If there are any problems then a non-zero value will be returned.
 */
extern int wrapperSetWorkingDir(const TCHAR* dir);

/******************************************************************************
 * Protocol callback functions
 *****************************************************************************/
extern void wrapperLogSignaled(int logLevel, TCHAR *msg);
extern void wrapperKeyRegistered(TCHAR *key);
extern void wrapperPingResponded();
extern void wrapperStopRequested(int exitCode);
extern void wrapperRestartRequested();
extern void wrapperStopPendingSignaled(int waitHint);
extern void wrapperStoppedSignaled();
extern void wrapperStartPendingSignaled(int waitHint);
extern void wrapperStartedSignaled();
#endif
