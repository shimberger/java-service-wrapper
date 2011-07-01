/*
 * Copyright (c) 1999, 2011 Tanuki Software, Ltd.
 * http://www.tanukisoftware.com
 * All rights reserved.
 *
 * This software is the proprietary information of Tanuki Software.
 * You shall use it only in accordance with the terms of the
 * license agreement you entered into with Tanuki Software.
 * http://wrapper.tanukisoftware.com/doc/english/licenseOverview.html
 */

/**
 * Author:
 *   Leif Mortenson <leif@tanukisoftware.com>
 */

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef WIN32
#include <errno.h>
#include <tchar.h>
#include <io.h>
#else
#include <stdlib.h>
#include <string.h>
#include <glob.h>
#include <unistd.h>
#endif

#include "wrapper_file.h"
#include "logger.h"
#include "wrapper_i18n.h"
#include "wrapper.h"

#define FILES_CHUNK 5

#ifndef TRUE
#define TRUE -1
#endif

#ifndef FALSE
#define FALSE 0
#endif

/**
 * Returns a valid sort mode given a name: "TIMES", "NAMES_ASC", "NAMES_DEC".
 *  In the event of an invalid value, TIMES will be returned.
 */
int wrapperFileGetSortMode(const TCHAR *modeName) {
    if (strcmpIgnoreCase(modeName, TEXT("NAMES_ASC")) == 0) {
        return WRAPPER_FILE_SORT_MODE_NAMES_ASC;
    } else if (strcmpIgnoreCase(modeName, TEXT("NAMES_DEC")) == 0) {
        return WRAPPER_FILE_SORT_MODE_NAMES_DEC;
    } else {
        return WRAPPER_FILE_SORT_MODE_TIMES;
    }
}


#ifdef WIN32
int sortFilesTimes(TCHAR **files, __time64_t *fileTimes, int cnt) {
#else
int sortFilesTimes(TCHAR **files, time_t *fileTimes, int cnt) {
#endif
    int i, j;
    TCHAR *temp;
#ifdef WIN32
    __time64_t tempTime;
#else
    time_t tempTime;
#endif

    for (i = 0; i < cnt; i++) {
        for (j = 0; j < cnt - 1; j++) {
            if (fileTimes[j] < fileTimes[j + 1]) {
                temp = files[j + 1];
                tempTime = fileTimes[j + 1];

                files[j + 1] = files[j];
                fileTimes[j + 1] = fileTimes[j];

                files[j] = temp;
                fileTimes[j] = tempTime;
            }
        }
    }

    return TRUE;
}

/**
 * Compares two strings.  Returns 0 if they are equal, -1 if file1 is bigger, 1 if file2 is bigger.
 */
int compareFileNames(const TCHAR *file1, const TCHAR *file2) {
    int pos1, pos2;
    TCHAR c1, c2;
    int numeric1, numeric2;
    long int num1, num2;
    int afterNumber = FALSE;

    pos1 = 0;
    pos2 = 0;

    while (TRUE) {
        c1 = file1[pos1];
        c2 = file2[pos2];
        /*printf("     file1[%d]=%d, file2[%d]=%d\n", pos1, c1, pos2, c2);*/

        /* Did we find the null. */
        if (c1 == 0) {
            if (c2 == 0) {
                return 0;
            } else {
                return 1;
            }
        } else {
            if (c2 == 0) {
                return -1;
            } else {
                /* Continue. */
            }
        }

        /* We have two characters. */
        numeric1 = (c1 >= TEXT('0') && c1 <= TEXT('9'));
        numeric2 = (c2 >= TEXT('0') && c2 <= TEXT('9'));

        /* See if one or both of the strings is numeric. */
        if (numeric1) {
            if (numeric2) {
                /* Both are numeric, we need to start comparing the two file names as integer values. */
                num1 = c1 - TEXT('0');
                c1 = file1[pos1 + 1];
                while (c1 >= TEXT('0') && c1 <= TEXT('9')) {
                    num1 = num1 * 10 + (c1 - TEXT('0'));
                    pos1++;
                    c1 = file1[pos1 + 1];
                }

                num2 = c2 - TEXT('0');
                c2 = file2[pos2 + 1];
                while (c2 >= TEXT('0') && c2 <= TEXT('9')) {
                    num2 = num2 * 10 + (c2 - TEXT('0'));
                    pos2++;
                    c2 = file2[pos2 + 1];
                }

                /*printf("     num1=%ld, num2=%ld\n", num1, num2);*/
                if (num1 > num2) {
                    return -1;
                } else if (num2 > num1) {
                    return 1;
                } else {
                    /* Equal, continue. */
                }
                afterNumber = TRUE;
            } else {
                /* 1 is numeric, 2 is not. */
                if (afterNumber) {
                    return -1;
                } else {
                    return 1;
                }
            }
        } else {
            if (numeric2) {
                /* 1 is not, 2 is numeric. */
                if (afterNumber) {
                    return 1;
                } else {
                    return -1;
                }
            } else {
                /* Neither is numeric. */
            }
        }

        /* Compare the characters as is. */
        if (c1 > c2) {
            return -1;
        } else if (c2 > c1) {
            return 1;
        } else {
            /* Equal, continue. */
            if (c1 == TEXT('.') || c1 == TEXT('-') || c1 == TEXT('_')) {
            } else {
                afterNumber = FALSE;
            }
        }

        pos1++;
        pos2++;
    }
}

int sortFilesNamesAsc(TCHAR **files, int cnt) {
    int i, j;
    TCHAR *temp;
    int cmp;

    for (i = 0; i < cnt; i++) {
        for (j = 0; j < cnt - 1; j++) {
            cmp = compareFileNames(files[j], files[j+1]);
            if (cmp < 0) {
                temp = files[j + 1];
                files[j + 1] = files[j];
                files[j] = temp;
            }
        }
    }

    return TRUE;
}

int sortFilesNamesDec(TCHAR **files, int cnt) {
    int i, j;
    TCHAR *temp;
    int cmp;

    for (i = 0; i < cnt; i++) {
        for (j = 0; j < cnt - 1; j++) {
            cmp = compareFileNames(files[j], files[j+1]);
            if (cmp > 0) {
                temp = files[j + 1];
                files[j + 1] = files[j];
                files[j] = temp;
            }
        }
    }

    return TRUE;
}

/**
 * Returns a NULL terminated list of file names within the specified pattern.
 *  The files will be sorted new to old for TIMES.  Then incremental ordering
 *  for NAMES.  The numeric components of the names will be treated as
 *  numbers and sorted accordingly.
 */
TCHAR** wrapperFileGetFiles(const TCHAR* pattern, int sortMode) {
    int cnt;
    int filesSize;
    TCHAR **files;
#ifdef WIN32
    int i;
    size_t dirLen;
    TCHAR *c;
    TCHAR *dirPart;
    intptr_t handle;
    struct _tfinddata64_t fblock;
    size_t fileLen;
    TCHAR **newFiles;
    __time64_t *fileTimes;
    __time64_t *newFileTimes;
#else
#ifdef WRAPPER_FILE_DEBUG
    int i;
#endif
    int result;
    glob_t g;
    int findex;
    time_t *fileTimes;
    struct stat fileStat;
#endif

#ifdef WRAPPER_FILE_DEBUG
    _tprintf(TEXT("wrapperFileGetFiles(%s, %d)\n"), pattern, sortMode);
#endif

#ifdef WIN32
    cnt = 0;
    /* Initialize the files array. */
    filesSize = FILES_CHUNK;
    files = malloc(sizeof(TCHAR *) * filesSize);
    if (!files) {
        outOfMemoryQueued(TEXT("WFGF"), 1);
        return NULL;
    }
    memset(files, 0, sizeof(TCHAR *) * filesSize);

    fileTimes = malloc(sizeof(__time64_t) * filesSize);
    if (!fileTimes) {
        outOfMemoryQueued(TEXT("WFGF"), 2);
        free(files);
        return NULL;
    }
    memset(fileTimes, 0, sizeof(__time64_t) * filesSize);

    /* Extract any path information from the beginning of the file */
    c = max(_tcsrchr(pattern, TEXT('\\')), _tcsrchr(pattern, TEXT('/')));
    if (c == NULL) {
        /* No directory component */
        dirPart = malloc(sizeof(TCHAR) * 1);
        if (!dirPart) {
            outOfMemoryQueued(TEXT("WFGF"), 3);
            return NULL;
        }
        dirPart[0] = TEXT('\0');
        dirLen = 0;
    } else {
        /* extract the directory. */
        dirLen = c - pattern + 1;
        dirPart = malloc(sizeof(TCHAR) * (dirLen + 1));
        if (!dirPart) {
            outOfMemoryQueued(TEXT("WFGF"), 4);
            return NULL;
        }
        _tcsncpy(dirPart, pattern, dirLen);
        dirPart[dirLen] = TEXT('\0');
    }

#ifdef WRAPPER_FILE_DEBUG
    _tprintf(TEXT("  dirPart=[%s]\n"), dirPart);
#endif

    /* Get the first file. */
    if ((handle = _tfindfirst64(pattern, &fblock)) > 0) {
        if ((_tcscmp(fblock.name, TEXT(".")) != 0) && (_tcscmp(fblock.name, TEXT("..")) != 0)) {
            fileLen = _tcslen(fblock.name);
            files[cnt] = malloc((_tcslen(dirPart) + _tcslen(fblock.name) + 1) * sizeof(TCHAR));
            if (!files[cnt]) {
                outOfMemoryQueued(TEXT("WFGF"), 5);
                free(fileTimes);
                wrapperFileFreeFiles(files);
                free(dirPart);
                return NULL;
            }
            _sntprintf(files[cnt], _tcslen(dirPart) + _tcslen(fblock.name) + 1, TEXT("%s%s"), dirPart, fblock.name);
            fileTimes[cnt] = fblock.time_write;
#ifdef WRAPPER_FILE_DEBUG
            _tprintf(TEXT("  files[%d]=%s, %ld\n"), cnt, files[cnt], fileTimes[cnt]);
#endif

            cnt++;
        }

        /* Look for additional files. */
        while (_tfindnext64(handle, &fblock) == 0) {
            if ((_tcscmp(fblock.name, TEXT(".")) != 0) && (_tcscmp(fblock.name, TEXT("..")) != 0)) {
                /* Make sure we have enough room in the files array. */
                if (cnt >= filesSize - 1) {
                    newFiles = malloc(sizeof(TCHAR *) * (filesSize + FILES_CHUNK));
                    if (!newFiles) {
                        outOfMemoryQueued(TEXT("WFGF"), 6);
                        free(fileTimes);
                        wrapperFileFreeFiles(files);
                        free(dirPart);
                        return NULL;
                    }
                    memset(newFiles, 0, sizeof(TCHAR *) * (filesSize + FILES_CHUNK));
                    newFileTimes = malloc(sizeof(__time64_t) * (filesSize + FILES_CHUNK));
                    if (!newFileTimes) {
                        outOfMemoryQueued(TEXT("WFGF"), 7);
                        free(newFiles);
                        free(fileTimes);
                        wrapperFileFreeFiles(files);
                        free(dirPart);
                        return NULL;
                    }
                    memset(newFileTimes, 0, sizeof(__time64_t) * (filesSize + FILES_CHUNK));
                    
                    for (i = 0; i < filesSize; i++) {
                        newFiles[i] = files[i];
                        newFileTimes[i] = fileTimes[i];
                    }
                    free(files);
                    free(fileTimes);
                    files = newFiles;
                    fileTimes = newFileTimes;
                    filesSize += FILES_CHUNK;
#ifdef WRAPPER_FILE_DEBUG
                    _tprintf(TEXT("  increased files to %d\n"), filesSize);
#endif
                }

                fileLen = _tcslen(fblock.name);
                files[cnt] = malloc((_tcslen(dirPart) + _tcslen(fblock.name) + 1) * sizeof(TCHAR));
                if (!files[cnt]) {
                    outOfMemoryQueued(TEXT("WFGF"), 8);
                    free(fileTimes);
                    wrapperFileFreeFiles(files);
                    free(dirPart);
                    return NULL;
                }
                _sntprintf(files[cnt], _tcslen(dirPart) + _tcslen(fblock.name) + 1, TEXT("%s%s"), dirPart, fblock.name);
                fileTimes[cnt] = fblock.time_write;

#ifdef WRAPPER_FILE_DEBUG
                _tprintf(TEXT("  files[%d]=%s, %ld\n"), cnt, files[cnt], fileTimes[cnt]);
#endif
                cnt++;
            }
        }

        /* Close the file search */
        _findclose(handle);
    }

    if (cnt <= 0) {
        if (errno == ENOENT) {
            /* No files matched. */
#ifdef WRAPPER_FILE_DEBUG
            _tprintf(TEXT("  No files matched.\n"));
#endif
        } else {
            /* Encountered an error of some kind. */
            log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Error listing files, %s: %s"), pattern, getLastErrorText());
            free(fileTimes);
            wrapperFileFreeFiles(files);
            return NULL;
        }
    }
#else

#ifdef UNICODE
    char* cPattern;
    size_t req;

    req = wcstombs(NULL, pattern, 0) + 1;
    cPattern = malloc(req);
    if(!cPattern) {
        outOfMemoryQueued(TEXT("WFGF"), 8);
        return NULL;
    }
    wcstombs(cPattern, pattern, req);

    result = glob(cPattern, GLOB_MARK | GLOB_NOSORT, NULL, &g);
    free(cPattern);
#else
    result = glob(pattern, GLOB_MARK | GLOB_NOSORT, NULL, &g);
#endif
    cnt = 0;
    if (!result) {
        if (g.gl_pathc > 0) {
            filesSize = g.gl_pathc + 1;
            files = malloc(sizeof(TCHAR *) * filesSize);
            if (!files) {
                outOfMemoryQueued(TEXT("WFGF"), 9);
                return NULL;
            }
            memset(files, 0, sizeof(TCHAR *) * filesSize);
            
            fileTimes = malloc(sizeof(time_t) * filesSize);
            if (!fileTimes) {
                outOfMemoryQueued(TEXT("WFGF"), 10);
                wrapperFileFreeFiles(files);
                return NULL;
            }
            memset(fileTimes, 0, sizeof(time_t) * filesSize);

            for (findex = 0; findex < g.gl_pathc; findex++) {
#ifdef UNICODE
                req = mbstowcs(NULL, g.gl_pathv[findex], 0);
                if (req < 0) {
                    invalidMultiByteSequence(TEXT("GLET"), 1);
                }
                files[cnt] = malloc((req + 1) * sizeof(TCHAR));
                if (!files[cnt]) {
                    outOfMemoryQueued(TEXT("WFGF"), 11);
                    free(fileTimes);
                    wrapperFileFreeFiles(files);
                    return NULL;
                }
                mbstowcs(files[cnt], g.gl_pathv[findex], req + 1);

#else
                files[cnt] = malloc((strlen(g.gl_pathv[findex]) + 1));
                if (!files[cnt]) {
                    outOfMemoryQueued(TEXT("WFGF"), 11);
                    free(fileTimes);
                    wrapperFileFreeFiles(files);
                    return NULL;
                }

                strncpy(files[cnt], g.gl_pathv[findex], strlen(g.gl_pathv[findex]) + 1);
#endif

                /* Only try to get the modified time if it is really necessary. */
                if (sortMode == WRAPPER_FILE_SORT_MODE_TIMES) {
                    if (!_tstat(files[cnt], &fileStat)) {
                        fileTimes[cnt] = fileStat.st_mtime;
                    } else {
                        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_WARN, TEXT("Failed to stat %s: %s"), files[cnt], getLastErrorText());
                    }
                }
#ifdef WRAPPER_FILE_DEBUG
                printf("  files[%d]=%s, %ld\n", cnt, files[cnt], fileTimes[cnt]);
#endif
                cnt++;
            }
        } else {
#ifdef WRAPPER_FILE_DEBUG
            printf("  No files matched.\n");
#endif
            /* No files, but we still need the array. */
            filesSize = 1;
            files = malloc(sizeof(TCHAR *) * filesSize);
            if (!files) {
                outOfMemoryQueued(TEXT("WFGF"), 12);
                return NULL;
            }
            memset(files, 0, sizeof(TCHAR *) * filesSize);
            
            fileTimes = malloc(sizeof(time_t) * filesSize);
            if (!fileTimes) {
                free(files);
                outOfMemoryQueued(TEXT("WFGF"), 13);
                return NULL;
            }
            memset(fileTimes, 0, sizeof(time_t) * filesSize);
        }

        globfree(&g);
    } else if (result == GLOB_NOMATCH) {
#ifdef WRAPPER_FILE_DEBUG
        _tprintf(TEXT("  No files matched.\n"));
#endif
        /* No files, but we still need the array. */
        filesSize = 1;
        files = malloc(sizeof(TCHAR *) * filesSize);
        if (!files) {
            outOfMemoryQueued(TEXT("WFGF"), 14);
            return NULL;
        }
        memset(files, 0, sizeof(TCHAR *) * filesSize);

        fileTimes = malloc(sizeof(time_t) * filesSize);
        if (!fileTimes) {
            free(files);
            outOfMemoryQueued(TEXT("WFGF"), 15);
            return NULL;
        }
        memset(fileTimes, 0, sizeof(time_t) * filesSize);
    } else {
        /* Encountered an error of some kind. */
        log_printf_queue(TRUE, WRAPPER_SOURCE_WRAPPER, LEVEL_ERROR, TEXT("Error listing files, %s: %s"), pattern, getLastErrorText());
        return NULL;
    }
#endif
    
    if (sortMode == WRAPPER_FILE_SORT_MODE_TIMES) {
        if (!sortFilesTimes(files, fileTimes, cnt)) {
            /* Failed. Reported. */
            free(fileTimes);
            wrapperFileFreeFiles(files);
            return NULL;
        }
    } else if (sortMode == WRAPPER_FILE_SORT_MODE_NAMES_DEC) {
        if (!sortFilesNamesDec(files, cnt)) {
            /* Failed. Reported. */
            free(fileTimes);
            wrapperFileFreeFiles(files);
            return NULL;
        }
    } else { /* WRAPPER_FILE_SORT_MODE_NAMES_ASC */
        if (!sortFilesNamesAsc(files, cnt)) {
            /* Failed. Reported. */
            free(fileTimes);
            wrapperFileFreeFiles(files);
            return NULL;
        }
    }

#ifdef WRAPPER_FILE_DEBUG
    _tprintf(TEXT("  Sorted:\n"));
    for (i = 0; i < cnt; i++) {
        _tprintf(TEXT("  files[%d]=%s, %ld\n"), i, files[i], fileTimes[i]);
    }
    _tprintf(TEXT("wrapperFileGetFiles(%s, %d) END\n"), pattern, sortMode);
#endif

    free(fileTimes);

    return files;
}

/**
 * Frees the array of file names returned by wrapperFileGetFiles()
 */
void wrapperFileFreeFiles(TCHAR** files) {
    int i;

    i = 0;
    while (files[i]) {
        free(files[i]);
        i++;
    }
    free(files);
}


/**
 * @param path to check.
 * @param advice 0 if advice should be displayed.
 *
 * @return advice or advice + 1 if advice was logged.
 */
int wrapperGetUNCFilePath(const TCHAR *path, int advice) {
#ifdef WIN32
    TCHAR drive[4];
    DWORD result;

    /* See if the path starts with a drive.  Some users use forward slashes in the paths. */
    if ((path != NULL) && (_tcslen(path) >= 3) && (path[1] == TEXT(':')) && ((path[2] == TEXT('\\')) || (path[2] == TEXT('/')))) {
        _tcsncpy(drive, path, 2);
        drive[2] = TEXT('\\');
        drive[3] = TEXT('\0');
        result = GetDriveType(drive);
        if (result == DRIVE_REMOTE) {
            if (advice == 0) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE, TEXT("The following path in your Wrapper configuration file is to a mapped Network\n  Drive.  Using mapped network drives is not recommeded as they will fail to\n  be resolved correctly under certain circumstances.  Please consider using\n  UNC paths (\\\\<host>\\<share>\\path). Additional refrences will be ignored.\n  Path: %s"), path);
                advice++;
            }
        } else if (result == DRIVE_NO_ROOT_DIR) {
            if (advice == 0) {
                log_printf(WRAPPER_SOURCE_WRAPPER, LEVEL_ADVICE, TEXT("The following path in your Wrapper configuration file could not be resolved.\n  Please make sure the path exists.  If the path is a network share, it may be\n  that the current user is unable to resolve it.  Please consider using UNC\n  paths (\\\\<host>\\<share>\\path) or run the service as another user\n  (see wrapper.ntservice.account). Additional refrences will be ignored.\n  Path: %s"), path);
                advice++;
            }
        }
    }
#endif
    return advice;
}

#ifdef WRAPPER_FILE_DEBUG
void wrapperFileTests() {
    TCHAR** files;

    printf("Start wrapperFileTests\n");
    files = wrapperFileGetFiles((TEXT("../logs/*.log*"), WRAPPER_FILE_SORT_MODE_TIMES);
    if (files) {
        wrapperFileFreeFiles(files);
    }

    files = wrapperFileGetFiles(TEXT("../logs/*.log*"), WRAPPER_FILE_SORT_MODE_NAMES_ASC);
    if (files) {
        wrapperFileFreeFiles(files);
    }

    files = wrapperFileGetFiles(TEXT("../logs/*.log*"), WRAPPER_FILE_SORT_MODE_NAMES_DEC);
    if (files) {
        wrapperFileFreeFiles(files);
    }
    printf("End wrapperFileTests\n");
}
#endif


