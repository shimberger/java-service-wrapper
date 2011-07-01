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
/* For some reason this is not defined sometimes when I build on MFVC 6.0 $%$%$@@!!
 * This causes a compiler error to let me know about the problem.  Anyone with any
 * ideas as to why this sometimes happens or how to fix it, please let me know. */
barf
#endif

#ifdef WIN32

#include <windows.h>
#include <io.h>
#include <time.h>
#include <tlhelp32.h>
#include <winnt.h>
#include <Sddl.h>
#include "wrapper_i18n.h"
#include "wrapperjni.h"

/* MS Visual Studio 8 went and deprecated the POXIX names for functions.
 *  Fixing them all would be a big headache for UNIX versions. */
#pragma warning(disable : 4996)

/* Reference to HINSTANCE of this DLL */
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

static DWORD javaProcessId = 0;

HANDLE controlEventQueueMutexHandle = NULL;

FARPROC OptionalProcess32First = NULL;
FARPROC OptionalProcess32Next = NULL;
FARPROC OptionalThread32First = NULL;
FARPROC OptionalThread32Next = NULL;
FARPROC OptionalCreateToolhelp32Snapshot = NULL;

int wrapperLockControlEventQueue() {
#ifdef _DEBUG
        _tprintf(TEXT(" wrapperLockControlEventQueue()\n"));
        fflush(NULL);
#endif
    if (!controlEventQueueMutexHandle) {
        /* Not initialized so fail quietly.  A message was shown on startup. */
        return -1;
    }

    /* Only wait for up to 30 seconds to make sure we don't get into a deadlock situation.
     *  This could happen if a signal is encountered while locked. */
    switch (WaitForSingleObject(controlEventQueueMutexHandle, 30000)) {
    case WAIT_ABANDONED:
        _tprintf(TEXT("WrapperJNI Error: Control Event mutex was abandoned.\n"));
        fflush(NULL);
        return -1;
    case WAIT_FAILED:
        _tprintf(TEXT("WrapperJNI Error: Control Event mutex wait failed.\n"));
        fflush(NULL);
        return -1;
    case WAIT_TIMEOUT:
        _tprintf(TEXT("WrapperJNI Error: Control Event mutex wait timed out.\n"));
        fflush(NULL);
        return -1;
    default:
        /* Ok */
        break;
    }
    return 0;
}

int wrapperReleaseControlEventQueue() {
    #ifdef _DEBUG
        _tprintf(TEXT(" wrapperReleaseControlEventQueue()\n"));
        fflush(NULL);
    #endif
    if (!ReleaseMutex(controlEventQueueMutexHandle)) {
        _tprintf(TEXT( "WrapperJNI Error: Failed to release Control Event mutex. %s\n"), getLastErrorText());
        fflush(NULL);
        return -1;
    }

    return 0;
}

/**
 * Handler to take care of the case where the user hits CTRL-C when the wrapper
 *  is being run as a console.  If this is not done, then the Java process
 *  would exit due to a CTRL_LOGOFF_EVENT when a user logs off even if the
 *  application is installed as a service.
 *
 * Handlers are called in the reverse order that they are registered until one
 *  returns TRUE.  So last registered is called first until the default handler
 *  is called.  This means that if we return FALSE, the JVM'S handler will then
 *  be called.
 */
int wrapperConsoleHandler(int key) {
    int event;

    /* Call the control callback in the java code */
    switch(key) {
    case CTRL_C_EVENT:
        event = org_tanukisoftware_wrapper_WrapperManager_WRAPPER_CTRL_C_EVENT;
        break;
    case CTRL_BREAK_EVENT:
        /* This is a request to do a thread dump. Let the JVM handle this. */
        return FALSE;
    case CTRL_CLOSE_EVENT:
        event = org_tanukisoftware_wrapper_WrapperManager_WRAPPER_CTRL_CLOSE_EVENT;
        break;
    case CTRL_LOGOFF_EVENT:
        event = org_tanukisoftware_wrapper_WrapperManager_WRAPPER_CTRL_LOGOFF_EVENT;
        break;
    case CTRL_SHUTDOWN_EVENT:
        event = org_tanukisoftware_wrapper_WrapperManager_WRAPPER_CTRL_SHUTDOWN_EVENT;
        break;
    default:
        event = key;
    }
    if (wrapperJNIDebugging) {
        _tprintf(TEXT("WrapperJNI Debug: Got Control Signal %d->%d\n"), key, event);
        flushall();
    }

    wrapperJNIHandleSignal(event);

    if (wrapperJNIDebugging) {
        _tprintf(TEXT("WrapperJNI Debug: Handled signal\n"));
        flushall();
    }

    return TRUE; /* We handled the event. */
}

/**
 * Looks up the name of the explorer.exe file in the registry.  It may change
 *  in a future version of windows, so this is the safe thing to do.
 */
TCHAR explorerExe[1024];
void
initExplorerExeName() {
    /* Location: "\\HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Winlogon\\Shell" */
    _sntprintf(explorerExe, 1024, TEXT("Explorer.exe"));
}

void throwException(JNIEnv *env, const char *className, int jErrorCode, const TCHAR *message) {
    jclass exceptionClass;
    jmethodID constructor;
    jbyteArray jMessage;
    jobject exception;

    if (exceptionClass = (*env)->FindClass(env, className)) {
        /* Look for the constructor. Ignore failures. */
        if (constructor = (*env)->GetMethodID(env, exceptionClass, "<init>", "(I[B)V")) {
            jMessage = (*env)->NewByteArray(env, (jsize)_tcslen(message) * sizeof(TCHAR));
            /* The 1.3.1 jni.h file does not specify the message as const.  The cast is to
             *  avoid compiler warnings trying to pass a (const TCHAR *) as a (TCHAR *). */
            JNU_SetByteArrayRegion(env, &jMessage, 0, (jsize)_tcslen(message) * sizeof(TCHAR), message);

            exception = (*env)->NewObject(env, exceptionClass, constructor, jErrorCode, jMessage);

            if ((*env)->Throw(env, exception)) {
                _tprintf(TEXT("WrapperJNI Error: Unable to throw exception of class '%s' with message: %s"),
                    className, message);
                flushall();
            }

            (*env)->DeleteLocalRef(env, jMessage);
            (*env)->DeleteLocalRef(env, exception);
        }

        (*env)->DeleteLocalRef(env, exceptionClass);
    } else {
        _tprintf(TEXT("WrapperJNI Error: Unable to load class, '%s' to report exception: %s"),
            className, message);
        flushall();
    }
}

void throwServiceException(JNIEnv *env, int errorCode, const TCHAR *message) {
    throwException(env, "org/tanukisoftware/wrapper/WrapperServiceException", errorCode, message);
}

/**
 * Converts a FILETIME to a time_t structure.
 */
time_t fileTimeToTimeT(FILETIME *filetime) {
    SYSTEMTIME utc;
    SYSTEMTIME local;
    TIME_ZONE_INFORMATION timeZoneInfo;
    struct tm tm;

    FileTimeToSystemTime(filetime, &utc);
    GetTimeZoneInformation(&timeZoneInfo);
    SystemTimeToTzSpecificLocalTime(&timeZoneInfo, &utc, &local);

    tm.tm_sec = local.wSecond;
    tm.tm_min = local.wMinute;
    tm.tm_hour = local.wHour;
    tm.tm_mday = local.wDay;
    tm.tm_mon = local.wMonth - 1;
    tm.tm_year = local.wYear - 1900;
    tm.tm_wday = local.wDayOfWeek;
    tm.tm_yday = -1;
    tm.tm_isdst = -1;
    return mktime(&tm);
}

/**
 * Looks for the login time given a user SID.  The login time is found by looking
 *  up the SID in the registry.
 */
time_t getUserLoginTime(TCHAR *sidText) {
    LONG     result;
    LPSTR    pBuffer = NULL;
    HKEY     userKey;
    int      i;
    TCHAR    userKeyName[MAX_PATH];
    DWORD    userKeyNameSize;
    FILETIME lastTime;
    time_t   loginTime;

    loginTime = 0;

    /* Open a key to the HKRY_USERS registry. */
    result = RegOpenKey(HKEY_USERS, NULL, &userKey);
    if (result != ERROR_SUCCESS) {
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, result, 0, (LPTSTR)&pBuffer, 0, NULL);
        _tprintf(TEXT("WrapperJNI Error: Error opening registry for HKEY_USERS: %s\n"), getLastErrorText());
        flushall();
        LocalFree(pBuffer);
        return loginTime;
    }

    /* Loop over the users */
    i = 0;
    userKeyNameSize = sizeof(userKeyName);
    while ((result = RegEnumKeyEx(userKey, i, userKeyName, &userKeyNameSize, NULL, NULL, NULL, &lastTime)) == ERROR_SUCCESS) {
        if (_tcsicmp(sidText, userKeyName) == 0) {
            /* We found the SID! */
            /* Convert the FILETIME to UNIX time. */
            loginTime = fileTimeToTimeT(&lastTime);
            break;
        }

        userKeyNameSize = sizeof(userKeyName);
        i++;
    }
    if (result != ERROR_SUCCESS) {
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, result, 0, (LPTSTR)&pBuffer, 0, NULL);
        printf("WrapperJNI Error: Unable to enumerate the registry: %d : %s", result, pBuffer);
        flushall();
        LocalFree(pBuffer);
    }

    /* Always close the userKey. */
    result = RegCloseKey(userKey);
    if (result != ERROR_SUCCESS) {
        FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER, NULL, result, 0, (LPTSTR)&pBuffer, 0, NULL);
        printf("WrapperJNI Error: Unable to close the registry: %d : %s", result, pBuffer);
        flushall();
        LocalFree(pBuffer);
    }
    return loginTime;
}

/**
 * Sets group information in a user object.
 *
 * Returns TRUE if there were any problems.
 */
int
setUserGroups(JNIEnv *env, jclass wrapperUserClass, jobject wrapperUser, HANDLE hProcessToken) {
    jmethodID addGroup;

    TOKEN_GROUPS *tokenGroups;
    DWORD tokenGroupsSize;
    DWORD i;

    TCHAR *sidText;
    TCHAR *groupName;
    DWORD groupNameSize;
    TCHAR *domainName;
    DWORD domainNameSize;
    SID_NAME_USE sidType;

    jstring jstringSID;
    jstring jstringGroupName;
    jstring jstringDomainName;

    int result = FALSE;

    /* Look for the method used to add groups to the user. */
    if (addGroup = (*env)->GetMethodID(env, wrapperUserClass, "addGroup", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V")) {
        /* Get the TokenGroups info from the token. */
        GetTokenInformation(hProcessToken, TokenGroups, NULL, 0, &tokenGroupsSize);
        tokenGroups = (TOKEN_GROUPS *)malloc(tokenGroupsSize);
        if (!tokenGroups) {
            throwOutOfMemoryError(env, TEXT("SUG1"));
            result = TRUE;
        } else {
            if (GetTokenInformation(hProcessToken, TokenGroups, tokenGroups, tokenGroupsSize, &tokenGroupsSize)) {
                /* Loop over each of the groups and add each one to the user. */
                for (i = 0; i < tokenGroups->GroupCount; i++) {
                    /* Get the text representation of the sid. */
                    if (ConvertSidToStringSid(tokenGroups->Groups[i].Sid, &sidText) == 0) {
                        _tprintf(TEXT("WrapperJNI Error: Failed to Convert SId to String: %s\n"), getLastErrorText());
                        result = TRUE;
                    } else {
                        /* We now have an SID, use it to lookup the account. */
                        groupNameSize = 0;
                        domainNameSize = 0;
                        LookupAccountSid(NULL, tokenGroups->Groups[i].Sid, NULL, &groupNameSize, NULL, &domainNameSize, &sidType);
                        groupName = (TCHAR*)malloc(sizeof(TCHAR) * groupNameSize);
                        if (!groupName) {
                            throwOutOfMemoryError(env, TEXT("SUG3"));
                            result = TRUE;
                        } else {
                            domainName = (TCHAR*)malloc(sizeof(TCHAR) * domainNameSize);
                            if (!domainName) {
                                throwOutOfMemoryError(env, TEXT("SUG4"));
                                result = TRUE;
                            } else {
                                if (LookupAccountSid(NULL, tokenGroups->Groups[i].Sid, groupName, &groupNameSize, domainName, &domainNameSize, &sidType)) {
                                    /* Create the arguments to the constructor as java objects */
                                    /* SID byte array */
                                    jstringSID = JNU_NewStringNative(env, sidText);
                                    if (jstringSID) {
                                        /* GroupName byte array */
                                        jstringGroupName = JNU_NewStringNative(env, groupName);
                                        if (jstringGroupName) {
                                            /* DomainName byte array */
                                            jstringDomainName = JNU_NewStringNative(env, domainName);
                                            if (jstringDomainName) {
                                                /* Now actually add the group to the user. */
                                                (*env)->CallVoidMethod(env, wrapperUser, addGroup, jstringSID, jstringGroupName, jstringDomainName);

                                                (*env)->DeleteLocalRef(env, jstringDomainName);
                                            } else {
                                                /* Exception Thrown */
                                                break;
                                            }

                                            (*env)->DeleteLocalRef(env, jstringGroupName);
                                        } else {
                                            /* Exception Thrown */
                                            break;
                                        }

                                        (*env)->DeleteLocalRef(env, jstringSID);
                                    } else {
                                        /* Exception Thrown */
                                        break;
                                    }
                                } else {
                                    /* This is normal as some accounts do not seem to be mappable. */
                                    /*
                                    _tprintf(TEXT("WrapperJNI Debug: Unable to locate account for Sid, %s: %s\n"), sidText, getLastErrorText());
                                    flushall();
                                    */
                                }
                                free(domainName);
                            }

                            free(groupName);
                        }

                        LocalFree(sidText);
                    }
                }
            } else {
                _tprintf(TEXT("WrapperJNI Error: Unable to get token information: %s\n"), getLastErrorText());
                flushall();
            }

            free(tokenGroups);
        }
    } else {
        /* Exception Thrown */
    }

    return result;
}

/**
 * Creates and returns a WrapperUser instance to represent the user who owns
 *  the specified process Id.
 */
jobject
createWrapperUserForProcess(JNIEnv *env, DWORD processId, jboolean groups) {
    HANDLE hProcess;
    HANDLE hProcessToken;
    TOKEN_USER *tokenUser;
    DWORD tokenUserSize;

    TCHAR *sidText;
    TCHAR *userName;
    DWORD userNameSize;
    TCHAR *domainName;
    DWORD domainNameSize;
    SID_NAME_USE sidType;
    time_t loginTime;

    jclass wrapperUserClass;
    jmethodID constructor;
    jstring jstringSID;
    jstring jstringUserName;
    jstring jstringDomainName;
    jobject wrapperUser = NULL;

    if (hProcess = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, processId)) {
        if (OpenProcessToken(hProcess, TOKEN_QUERY, &hProcessToken)) {
            GetTokenInformation(hProcessToken, TokenUser, NULL, 0, &tokenUserSize);
            tokenUser = (TOKEN_USER *)malloc(tokenUserSize);
            if (!tokenUser) {
                throwOutOfMemoryError(env, TEXT("CWUFP1"));
            } else {
                if (GetTokenInformation(hProcessToken, TokenUser, tokenUser, tokenUserSize, &tokenUserSize)) {
                    /* Get the text representation of the sid. */
                    if (ConvertSidToStringSid(tokenUser->User.Sid, &sidText) == 0) {
                        _tprintf(TEXT("Failed to Convert SId to String: %s\n"), getLastErrorText());
                    } else {
                        /* We now have an SID, use it to lookup the account. */
                        userNameSize = 0;
                        domainNameSize = 0;
                        LookupAccountSid(NULL, tokenUser->User.Sid, NULL, &userNameSize, NULL, &domainNameSize, &sidType);
                        userName = (TCHAR*)malloc(sizeof(TCHAR) * userNameSize);
                        if (!userName) {
                            throwOutOfMemoryError(env, TEXT("CWUFP3"));
                        } else {
                            domainName = (TCHAR*)malloc(sizeof(TCHAR) * domainNameSize);
                            if (!domainName) {
                                throwOutOfMemoryError(env, TEXT("CWUFP4"));
                            } else {
                                if (LookupAccountSid(NULL, tokenUser->User.Sid, userName, &userNameSize, domainName, &domainNameSize, &sidType)) {
                                    /* Get the time that this user logged in. */
                                    loginTime = getUserLoginTime(sidText);

                                    /* Look for the WrapperUser class. Ignore failures as JNI throws an exception. */
                                    if (wrapperUserClass = (*env)->FindClass(env, "org/tanukisoftware/wrapper/WrapperWin32User")) {

                                        /* Look for the constructor. Ignore failures. */
                                        if (constructor = (*env)->GetMethodID(env, wrapperUserClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;I)V")) {

                                            /* Create the arguments to the constructor as java objects */
                                            /* SID */
                                            jstringSID = JNU_NewStringNative(env, sidText);
                                            if (jstringSID) {
                                                /* UserName */
                                                jstringUserName = JNU_NewStringNative(env, userName);
                                                if (jstringUserName) {
                                                    /* DomainName */
                                                    jstringDomainName = JNU_NewStringNative(env, domainName);
                                                    if (jstringDomainName) {
                                                        /* Now create the new wrapperUser using the constructor arguments collected above. */
                                                        wrapperUser = (*env)->NewObject(env, wrapperUserClass, constructor, jstringSID, jstringUserName, jstringDomainName, loginTime);

                                                        /* If the caller requested the user's groups then look them up. */
                                                        if (groups) {
                                                            if (setUserGroups(env, wrapperUserClass, wrapperUser, hProcessToken)) {
                                                                /* Failed. Just continue without groups. */
                                                            }
                                                        }

                                                        (*env)->DeleteLocalRef(env, jstringDomainName);
                                                    } else {
                                                        /* Exception Thrown */
                                                    }

                                                    (*env)->DeleteLocalRef(env, jstringUserName);
                                                } else {
                                                    /* Exception Thrown */
                                                }

                                                (*env)->DeleteLocalRef(env, jstringSID);
                                            } else {
                                                /* Exception Thrown */
                                            }
                                        } else {
                                            /* Exception Thrown */
                                        }

                                        (*env)->DeleteLocalRef(env, wrapperUserClass);
                                    } else {
                                        /* Exception Thrown */
                                    }
                                } else {
                                    /* This is normal as some accounts do not seem to be mappable. */
                                    /*
                                    printf(TEXT("WrapperJNI Debug: Unable to locate account for Sid, %s: %s\n"), sidText, getLastErrorText());
                                    flushall();
                                    */
                                }
                                free(domainName);
                            }

                            free(userName);
                        }

                        LocalFree(sidText);
                    }
                } else {
                    _tprintf(TEXT("WrapperJNI Error: Unable to get token information: %s\n"), getLastErrorText());
                    flushall();
                }

                free(tokenUser);
            }

            CloseHandle(hProcessToken);
        } else {
            _tprintf(TEXT("WrapperJNI Error: Unable to open process token: %s\n"), getLastErrorText());
            flushall();
        }

        CloseHandle(hProcess);
    } else {
        _tprintf(TEXT("WrapperJNI Error: Unable to open process: %s\n"), getLastErrorText());
        flushall();
    }

    return wrapperUser;
}

void loadDLLProcs() {
    HMODULE kernel32Mod;

    if ((kernel32Mod = GetModuleHandle(TEXT("KERNEL32.DLL"))) == NULL) {
        _tprintf(TEXT("WrapperJNI Error: Unable to load KERNEL32.DLL: %s\n"), getLastErrorText());
        flushall();
        return;
    }
#ifdef UNICODE
    if ((OptionalProcess32First = GetProcAddress(kernel32Mod, "Process32FirstW")) == NULL) {
#else
    if ((OptionalProcess32First = GetProcAddress(kernel32Mod, "Process32First")) == NULL) {
#endif
        if (wrapperJNIDebugging) {
            _tprintf(TEXT("WrapperJNI Debug: The Process32First function is not available on this version of Windows.\n"));
            flushall();
        }
    }
#ifdef UNICODE
    if ((OptionalProcess32Next = GetProcAddress(kernel32Mod, "Process32NextW")) == NULL) {
#else
    if ((OptionalProcess32Next = GetProcAddress(kernel32Mod, "Process32Next")) == NULL) {
#endif
        if (wrapperJNIDebugging) {
            _tprintf(TEXT("WrapperJNI Debug: The Process32Next function is not available on this version of Windows.\n"));
            flushall();
        }
    }
    if ((OptionalThread32First = GetProcAddress(kernel32Mod, "Thread32First")) == NULL) {
        if (wrapperJNIDebugging) {
            _tprintf(TEXT("WrapperJNI Debug: The Thread32First function is not available on this version of Windows.\n"));
            flushall();
        }
    }
    if ((OptionalThread32Next = GetProcAddress(kernel32Mod, "Thread32Next")) == NULL) {
        if (wrapperJNIDebugging) {
            _tprintf(TEXT("WrapperJNI Debug: The Thread32Next function is not available on this version of Windows.\n"));
            flushall();
        }
    }
    if ((OptionalCreateToolhelp32Snapshot = GetProcAddress(kernel32Mod, "CreateToolhelp32Snapshot")) == NULL) {
        if (wrapperJNIDebugging) {
            _tprintf(TEXT("WrapperJNI Debug: The CreateToolhelp32Snapshot function is not available on this version of Windows.\n"));
            flushall();
        }
    }
}


/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeInit
 * Signature: (Z)V
 */
JNIEXPORT void JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeInit(JNIEnv *env, jclass jClassWrapperManager, jboolean debugging) {
    TCHAR szPath[_MAX_PATH];
    DWORD usedLen;
    OSVERSIONINFO osVer;
    wrapperJNIDebugging = debugging;

    /* Set the locale so we can display MultiByte characters. */
    _tsetlocale(LC_ALL, TEXT(""));

    if (wrapperJNIDebugging) {
        /* This is useful for making sure that the JNI call is working. */
        _tprintf(TEXT("WrapperJNI Debug: Initializing WrapperManager native library.\n"));
        flushall();

        usedLen = GetModuleFileName(NULL, szPath, _MAX_PATH);
        if (usedLen == 0) {
            _tprintf(TEXT("WrapperJNI Debug: Unable to retrieve the Java process file name. %s\n"), getLastErrorText());
            flushall();
        } else if ((usedLen == _MAX_PATH) || (getLastError() == ERROR_INSUFFICIENT_BUFFER)) {
            _tprintf(TEXT("WrapperJNI Debug: Unable to retrieve the Java process file name. %s\n"), TEXT("Path too long."));
            flushall();
        } else {
            _tprintf(TEXT("WrapperJNI Debug: Java Executable: %s\n"), szPath);
            flushall();
        }

        usedLen = GetModuleFileName((HINSTANCE)&__ImageBase, szPath, _MAX_PATH);
        if (usedLen == 0) {
            _tprintf(TEXT("WrapperJNI Debug: Unable to retrieve the native library file name. %s\n"), getLastErrorText());
            flushall();
        } else if ((usedLen == _MAX_PATH) || (getLastError() == ERROR_INSUFFICIENT_BUFFER)) {
            _tprintf(TEXT("WrapperJNI Debug: Unable to retrieve the native library file name. %s\n"), TEXT("Path too long."));
            flushall();
        } else {
            _tprintf(TEXT("WrapperJNI Debug: Native Library: %s\n"), szPath);
            flushall();
        }
    }
    initCommon(env, jClassWrapperManager);

    osVer.dwOSVersionInfoSize = sizeof(osVer);
    if (GetVersionEx(&osVer)) {
        if (wrapperJNIDebugging) {
            _tprintf(TEXT("WrapperJNI Debug: Windows version: %ld.%ld.%ld\n"),
                osVer.dwMajorVersion, osVer.dwMinorVersion, osVer.dwBuildNumber);
            flushall();
        }
    } else {
        _tprintf(TEXT("WrapperJNI Error: Unable to retrieve the Windows version information.\n"));
        flushall();
    }
    loadDLLProcs();
    if (!(controlEventQueueMutexHandle = CreateMutex(NULL, FALSE, NULL))) {
        _tprintf(TEXT("WrapperJNI Error: Failed to create control event queue mutex. Signals will be ignored. %s\n"), getLastErrorText());
        flushall();
        controlEventQueueMutexHandle = NULL;
    }

    /* Make sure that the handling of CTRL-C signals is enabled for this process. */
    if (!SetConsoleCtrlHandler(NULL, FALSE)) {
        _tprintf(TEXT("WrapperJNI Error: Attempt to reset control signal handlers failed. %s\n"), getLastErrorText());
        flushall();
    }

    /* Initialize the CTRL-C handler */
    if (!SetConsoleCtrlHandler((PHANDLER_ROUTINE)wrapperConsoleHandler, TRUE)) {
        _tprintf(TEXT("WrapperJNI Error: Attempt to register a control signal handler failed. %s\n"), getLastErrorText());
        flushall();
    }

    /* Store the current process Id */
    javaProcessId = GetCurrentProcessId();

    /* Initialize the explorer.exe name. */
    initExplorerExeName();
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeRedirectPipes
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeRedirectPipes(JNIEnv *evn, jclass clazz) {
    /* We don't need to do anything on Windows. */
    return 0;
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeGetJavaPID
 * Signature: ()I
 */
JNIEXPORT jint JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeGetJavaPID(JNIEnv *env, jclass clazz) {
    return GetCurrentProcessId();
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeRequestThreadDump
 * Signature: ()V
 */
JNIEXPORT void JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeRequestThreadDump(JNIEnv *env, jclass clazz) {
    if (wrapperJNIDebugging) {
        _tprintf(TEXT("WrapperJNI Debug: Sending BREAK event to process group %ld.\n"), javaProcessId);
        flushall();
    }
    if (GenerateConsoleCtrlEvent(CTRL_BREAK_EVENT, javaProcessId) == 0) {
        if (getLastError() == 6) {
            _tprintf(TEXT("WrapperJNI Error: Unable to send BREAK event to JVM process because it does not have a console.\n"));
            flushall();
        } else {
            _tprintf(TEXT("WrapperJNI Error: Unable to send BREAK event to JVM process: %s\n"),
                getLastErrorText());
            flushall();
        }
    }
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeSetConsoleTitle
 * Signature: (Ljava/lang/String;)V
 */
JNIEXPORT void JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeSetConsoleTitle(JNIEnv *env, jclass clazz, jstring jstringTitle) {
    TCHAR *title;

    title = JNU_GetStringNativeChars(env, jstringTitle);
    if (!title) {
        throwOutOfMemoryError(env, TEXT("NSCT1"));
    } else {
        if (wrapperJNIDebugging) {
            _tprintf(TEXT("WrapperJNI Debug: Setting the console title to: %s\n"), title);
            flushall();
        }

        SetConsoleTitle(title);

        free(title);
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
    DWORD processId;

#ifdef UVERBOSE
    _tprintf(TEXT("WrapperJNI Debug: nativeGetUser()\n"));
    flushall();
#endif

    /* Get the current processId. */
    processId = GetCurrentProcessId();

    return createWrapperUserForProcess(env, processId, groups);
}


/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeGetInteractiveUser
 * Signature: (Z)Lorg/tanukisoftware/wrapper/WrapperUser;
 */
/*#define IUVERBOSE*/
JNIEXPORT jobject JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeGetInteractiveUser(JNIEnv *env, jclass clazz, jboolean groups) {
    HANDLE snapshot;
    PROCESSENTRY32 processEntry;
    THREADENTRY32 threadEntry;
    BOOL foundThread;
    HDESK desktop;
    jobject wrapperUser = NULL;

#ifdef IUVERBOSE
    _tprintf(TEXT("WrapperJNI Debug: nativeGetInteractiveUser()\n"));
    flushall();
#endif

    /* This function will only work if all required optional functions existed. */
    if ((OptionalProcess32First == NULL) || (OptionalProcess32Next == NULL) ||
        (OptionalThread32First == NULL) || (OptionalThread32Next == NULL) ||
        (OptionalCreateToolhelp32Snapshot == NULL)) {
        if (wrapperJNIDebugging) {
            _tprintf(TEXT("WrapperJNI Debug: getInteractiveUser not supported on this platform.\n"));
            flushall();
        }
        return NULL;
    }

    /* In order to be able to return the interactive user, we first need to locate the
     *  logged on user whose desktop we are able to open.  On XP systems, there will be
     *  more than one user with a desktop, but only the first one to log on will allow
     *  us to open its desktop.  On all NT systems, there will be additional logged on
     *  users if there are other services running. */
    if ((snapshot = (HANDLE)OptionalCreateToolhelp32Snapshot(TH32CS_SNAPPROCESS | TH32CS_SNAPTHREAD, 0)) >= 0) {
        processEntry.dwSize = sizeof(processEntry);
        if (OptionalProcess32First(snapshot, &processEntry)) {
            do {
                /* We are only interrested in the Explorer processes. */
                if (_tcsicmp(explorerExe, processEntry.szExeFile) == 0) {
#ifdef IUVERBOSE
                    _tprintf(TEXT("WrapperJNI Debug: Process size=%ld, cnt=%ld, id=%ld, parentId=%ld, moduleId=%ld, threads=%ld, exe=%s\n"),
                        processEntry.dwSize, processEntry.cntUsage, processEntry.th32ProcessID,
                        processEntry.th32ParentProcessID, processEntry.th32ModuleID, processEntry.cntThreads,
                        processEntry.szExeFile);
                    flushall();
#endif

                    /* Now look for a thread which is owned by the explorer process. */
                    threadEntry.dwSize = sizeof(threadEntry);
                    if (OptionalThread32First(snapshot, &threadEntry)) {
                        foundThread = FALSE;
                        do {
                            /* We are only interrested in threads that belong to the current Explorer process. */
                            if (threadEntry.th32OwnerProcessID == processEntry.th32ProcessID) {
#ifdef IUVERBOSE
                                _tprintf(TEXT("WrapperJNI Debug:   Thread Id=%ld\n"), threadEntry.th32ThreadID);
                                flushall();
#endif

                                /* We have a thread, now see if we can gain access to its desktop */
                                if (desktop = GetThreadDesktop(threadEntry.th32ThreadID)) {
                                    /* We got the desktop!   We now know that this is the thread and thus
                                     *  process that we have been looking for.   Unfortunately it does not
                                     *  appear that we can get the Sid of the account directly from this
                                     *  desktop.  I tried using GetUserObjectInformation, but the Sid
                                     *  returned does not seem to map to a valid account. */

                                    wrapperUser = createWrapperUserForProcess(env, processEntry.th32ProcessID, groups);
                                } else {
#ifdef IUVERBOSE
                                    _tprintf(TEXT("WrapperJNI Debug: GetThreadDesktop failed: %s\n"), getLastErrorText());
                                    flushall();
#endif
                                }

                                /* We only need the first thread, so break */
                                foundThread = TRUE;
                                break;
                            }
                        } while (OptionalThread32Next(snapshot, &threadEntry));

                        if (!foundThread && (GetLastError() != ERROR_NO_MORE_FILES)) {
#ifdef IUVERBOSE
                            _tprintf(TEXT("WrapperJNI Debug: Unable to get next thread entry: %s\n"), getLastErrorText());
                            flushall();
#endif
                        }
                    } else if (GetLastError() != ERROR_NO_MORE_FILES) {
                        _tprintf(TEXT("WrapperJNI Debug: Unable to get first thread entry: %s\n"), getLastErrorText());
                        flushall();
                    }
                }
            } while (OptionalProcess32Next(snapshot, &processEntry));

#ifdef IUVERBOSE
            if (GetLastError() != ERROR_NO_MORE_FILES) {
                _tprintf(TEXT("WrapperJNI Debug: Unable to get next process entry: %s\n"), getLastErrorText());
                flushall();
            }
#endif
        } else if (GetLastError() != ERROR_NO_MORE_FILES) {
            _tprintf(TEXT("WrapperJNI Error: Unable to get first process entry: %s\n"), getLastErrorText());
            flushall();
        }

        CloseHandle(snapshot);
    } else {
        _tprintf(TEXT("WrapperJNI Error: Toolhelp snapshot failed: %s\n"), getLastErrorText());
        flushall();
    }
    return wrapperUser;
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeListServices
 * Signature: ()[Lorg/tanukisoftware/wrapper/WrapperWin32Service;
 */
JNIEXPORT jobjectArray JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeListServices(JNIEnv *env, jclass clazz) {
    TCHAR buffer[512];
    SC_HANDLE hSCManager;
    DWORD size, sizeNeeded, servicesReturned, resumeHandle;
    DWORD err;
    ENUM_SERVICE_STATUS *services = NULL;
    BOOL threwError = FALSE;
    DWORD i;

    jobjectArray serviceArray = NULL;
    jclass serviceClass;
    jmethodID constructor;
    jstring jStringName;
    jstring jStringDisplayName;
    DWORD state;
    DWORD exitCode;
    jobject service;

    hSCManager = OpenSCManager(NULL, NULL, SC_MANAGER_ENUMERATE_SERVICE);
    if (hSCManager) {
        /* Before we can get the list of services, we need to know how much memory it will take. */
        resumeHandle = 0;
        if (!EnumServicesStatus(hSCManager, SERVICE_WIN32, SERVICE_STATE_ALL, NULL, 0, &sizeNeeded, &servicesReturned, &resumeHandle)) {
            err = GetLastError();
            if ((err == ERROR_MORE_DATA) || (err == ERROR_INSUFFICIENT_BUFFER)) {
                /* Allocate the needed memory and call again. */
                size = sizeNeeded;
                services = malloc(size);
                if (!services) {
                    throwOutOfMemoryError(env, TEXT("NLS1"));
                } else {
                    if (!EnumServicesStatus(hSCManager, SERVICE_WIN32, SERVICE_STATE_ALL, services, size, &sizeNeeded, &servicesReturned, &resumeHandle)) {
                        /* Failed to get the services. */
                        _sntprintf(buffer, 512, TEXT("Unable to enumerate the system services: %s"),
                            getLastErrorText());
                        throwServiceException(env, GetLastError(), buffer);
                        threwError = TRUE;
                    } else {
                        /* Success. */
                    }

                    /* free(services) is done below. */
                }
            } else {
                _sntprintf(buffer, 512, TEXT("Unable to enumerate the system services: %s"),
                    getLastErrorText());
                throwServiceException(env, GetLastError(), buffer);
                threwError = TRUE;
            }
        } else {
            /* Success which means that no services were found. */
        }

        if (!threwError) {
            if (serviceClass = (*env)->FindClass(env, "org/tanukisoftware/wrapper/WrapperWin32Service")) {
                /* Look for the constructor. Ignore failures. */
                if (constructor = (*env)->GetMethodID(env, serviceClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;II)V")) {
                    serviceArray = (*env)->NewObjectArray(env, servicesReturned, serviceClass, NULL);

                    for (i = 0; i < servicesReturned; i++) {
                        jStringName = JNU_NewStringNative(env, services[i].lpServiceName);
                        if (jStringName) {
                            jStringDisplayName = JNU_NewStringNative(env, services[i].lpDisplayName);
                            if (jStringDisplayName) {
                                state = services[i].ServiceStatus.dwCurrentState;

                                exitCode = services[i].ServiceStatus.dwWin32ExitCode;
                                if (exitCode == ERROR_SERVICE_SPECIFIC_ERROR) {
                                    exitCode = services[i].ServiceStatus.dwServiceSpecificExitCode;
                                }

                                service = (*env)->NewObject(env, serviceClass, constructor, jStringName, jStringDisplayName, state, exitCode);
                                (*env)->SetObjectArrayElement(env, serviceArray, i, service);
                                (*env)->DeleteLocalRef(env, service);

                                (*env)->DeleteLocalRef(env, jStringDisplayName);
                            } else {
                                /* Exception Thrown */
                                break;
                            }
                            (*env)->DeleteLocalRef(env, jStringName);
                        } else {
                            /* Exception Thrown */
                            break;
                        }
                    }
                }

                (*env)->DeleteLocalRef(env, serviceClass);
            } else {
                /* Unable to load the service class. */
                _sntprintf(buffer, 512, TEXT("Unable to locate class org.tanukisoftware.wrapper.WrapperWin32Service"));
                throwServiceException(env, 1, buffer);
            }
        }

        if (services != NULL) {
            free(services);
        }

        /* Close the handle to the service control manager database */
        CloseServiceHandle(hSCManager);
    } else {
        /* Unable to open the service manager. */
        _sntprintf(buffer, 512, TEXT("Unable to open the Windows service control manager database: %s"),
            getLastErrorText());
        throwServiceException(env, GetLastError(), buffer);
    }

    return serviceArray;
}

/*
 * Class:     org_tanukisoftware_wrapper_WrapperManager
 * Method:    nativeSendServiceControlCode
 * Signature: (Ljava/lang/String;I)Lorg/tanukisoftware/wrapper/WrapperWin32Service;
 */
JNIEXPORT jobject JNICALL
Java_org_tanukisoftware_wrapper_WrapperManager_nativeSendServiceControlCode(JNIEnv *env, jclass clazz, jstring jStringServiceName, jint controlCode) {
    jobject service = NULL;
    TCHAR *serviceName;
    size_t bufferSize = 2048;
    TCHAR buffer[2048];
    SC_HANDLE hSCManager;
    SC_HANDLE hService;
    int serviceAccess;
    DWORD wControlCode;
    BOOL threwError = FALSE;
    SERVICE_STATUS serviceStatus;
    jclass serviceClass;
    jmethodID constructor;
    DWORD displayNameSize;
    TCHAR *displayName;
    jstring jStringDisplayName;
    DWORD state;
    DWORD exitCode;

    if ((serviceName = JNU_GetStringNativeChars(env, jStringServiceName))) {
        hSCManager = OpenSCManager(NULL, NULL, GENERIC_READ);
        if (hSCManager) {
            /* Decide on the access needed when opening the service. */
            if (controlCode == org_tanukisoftware_wrapper_WrapperManager_SERVICE_CONTROL_CODE_START) {
                serviceAccess = SERVICE_START | SERVICE_INTERROGATE | SERVICE_QUERY_STATUS;
                wControlCode = SERVICE_CONTROL_INTERROGATE;
            } else if (controlCode == org_tanukisoftware_wrapper_WrapperManager_SERVICE_CONTROL_CODE_STOP) {
                serviceAccess = SERVICE_STOP | SERVICE_QUERY_STATUS;
                wControlCode = SERVICE_CONTROL_STOP;
            } else if (controlCode == org_tanukisoftware_wrapper_WrapperManager_SERVICE_CONTROL_CODE_INTERROGATE) {
                serviceAccess = SERVICE_INTERROGATE | SERVICE_QUERY_STATUS;
                wControlCode = SERVICE_CONTROL_INTERROGATE;
            } else if (controlCode == org_tanukisoftware_wrapper_WrapperManager_SERVICE_CONTROL_CODE_PAUSE) {
                serviceAccess = SERVICE_PAUSE_CONTINUE | SERVICE_QUERY_STATUS;
                wControlCode = SERVICE_CONTROL_PAUSE;
            } else if (controlCode == org_tanukisoftware_wrapper_WrapperManager_SERVICE_CONTROL_CODE_CONTINUE) {
                serviceAccess = SERVICE_PAUSE_CONTINUE | SERVICE_QUERY_STATUS;
                wControlCode = SERVICE_CONTROL_CONTINUE;
            } else if ((controlCode >= 128) || (controlCode <= 255)) {
                serviceAccess = SERVICE_USER_DEFINED_CONTROL | SERVICE_QUERY_STATUS;
                wControlCode = controlCode;
            } else {
                /* Illegal control code. */
                _sntprintf(buffer, 512, TEXT("Illegal Control code specified: %d"), controlCode);
                throwServiceException(env, 1, buffer);
                threwError = TRUE;
            }

            if (!threwError) {
                hService = OpenService(hSCManager, serviceName, serviceAccess);
                if (hService) {
                    /* If we are trying to start a service, it needs to be handled specially. */
                    if (controlCode == org_tanukisoftware_wrapper_WrapperManager_SERVICE_CONTROL_CODE_START) {
                        if (StartService(hService, 0, NULL)) {
                            /* Started the service. Continue on and interrogate the service. */
                        } else {
                           /* Failed. */
                            _sntprintf(buffer, bufferSize, TEXT("Unable to start service \"%s\": %s"), serviceName, getLastErrorText());
                            throwServiceException(env, GetLastError(), buffer);
                            threwError = TRUE;
                        }
                    }

                    if (!threwError) {
                        if (ControlService(hService, wControlCode, &serviceStatus)) {
                            /* Success.  fall through. */
                        } else {
                            /* Failed to send the control code.   See if the service is running. */
                            if (GetLastError() == ERROR_SERVICE_NOT_ACTIVE) {
                                /* Service is not running, so get its status information. */
                                if (QueryServiceStatus(hService, &serviceStatus)) {
                                    /* We got the status.  fall through. */
                                } else {
                                    /* Actual failure. */
                                    _sntprintf(buffer, bufferSize, TEXT("Unable to query status of service \"%s\": %s"), serviceName, getLastErrorText());
                                    throwServiceException(env, GetLastError(), buffer);
                                    threwError = TRUE;
                                }
                            } else {
                                /* Actual failure. */
                                _sntprintf(buffer, bufferSize, TEXT("Unable to query status of service \"%s\": %s"), serviceName, getLastErrorText());
                                throwServiceException(env, GetLastError(), buffer);
                                threwError = TRUE;
                            }
                        }

                        if (!threwError) {
                            /* Build up a service object to return. */
                            if (serviceClass = (*env)->FindClass(env, "org/tanukisoftware/wrapper/WrapperWin32Service")) {
                                /* Look for the constructor. Ignore failures. */
                                if (constructor = (*env)->GetMethodID(env, serviceClass, "<init>", "(Ljava/lang/String;Ljava/lang/String;II)V")) {
                                    /* Look up the display name of the service. First need to figure out how big it is. */
                                    displayNameSize = 0;
                                    GetServiceDisplayName(hSCManager, serviceName, NULL, &displayNameSize);
                                    if (GetLastError() != ERROR_INSUFFICIENT_BUFFER) {
                                        _sntprintf(buffer, bufferSize, TEXT("Unable to obtain the display name of service \"%s\": %s"), serviceName, getLastErrorText());
                                        throwServiceException(env, GetLastError(), buffer);
                                        threwError = TRUE;
                                    } else {
                                        displayNameSize++; /* Add room for the '\0' . */
                                        displayName = malloc(sizeof(TCHAR) * displayNameSize);
                                        if (!displayName) {
                                            throwOutOfMemoryError(env, TEXT("NSSCC1"));
                                            threwError = TRUE;
                                        } else {
                                            /* Now get the display name for real. */
                                            if ((GetServiceDisplayName(hSCManager, serviceName, displayName, &displayNameSize) == 0) && GetLastError()) {
                                                _sntprintf(buffer, bufferSize, TEXT("Unable to obtain the display name of service \"%s\": %s"), serviceName, getLastErrorText());
                                                throwServiceException(env, GetLastError(), buffer);
                                                threwError = TRUE;
                                            } else {
                                                /* Convert the display name to a jstring. */
                                                jStringDisplayName = JNU_NewStringNative(env, displayName);
                                                if (jStringDisplayName) {
                                                    state = serviceStatus.dwCurrentState;

                                                    exitCode = serviceStatus.dwWin32ExitCode;
                                                    if (exitCode == ERROR_SERVICE_SPECIFIC_ERROR) {
                                                        exitCode = serviceStatus.dwServiceSpecificExitCode;
                                                    }

                                                    service = (*env)->NewObject(env, serviceClass, constructor, jStringServiceName, jStringDisplayName, state, exitCode);

                                                    (*env)->DeleteLocalRef(env, jStringDisplayName);
                                                }
                                            }

                                            free(displayName);
                                        }
                                    }
                                } else {
                                    /* Exception Thrown */
                                    threwError = TRUE;
                                }

                                (*env)->DeleteLocalRef(env, serviceClass);
                            } else {
                                /* Exception Thrown */
                                threwError = TRUE;
                            }
                        }
                    }

                    CloseServiceHandle(hService);
                } else {
                    /* Unable to open service. */
                    _sntprintf(buffer, bufferSize, TEXT("Unable to open the service '%s': %s"), serviceName, getLastErrorText());
                    throwServiceException(env, GetLastError(), buffer);
                    threwError = TRUE;
                }
            }

            /* Close the handle to the service control manager database */
            CloseServiceHandle(hSCManager);
        } else {
            /* Unable to open the service manager. */
            _sntprintf(buffer, bufferSize, TEXT("Unable to open the Windows service control manager database: %s"), getLastErrorText());
            throwServiceException(env, GetLastError(), buffer);
            threwError = TRUE;
        }

        free(serviceName);
    } else {
        /* Exception Thrown */
    }

    return service;
}



#endif
