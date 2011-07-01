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

#ifdef CUNIT

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "CUnit/Basic.h"
#include "logger.h"
#include "wrapper_i18n.h"
#include "wrapper.h"

/**
 * The suite initialization function.
 * Opens the temporary file used by the tests.
 * Returns zero on success, non-zero otherwise.
 */
int init_suite1(void) {
    return 0;
}

/**
 * The suite cleanup function.
 * Closes the temporary file used by the tests.
 * Returns zero on success, non-zero otherwise.
 */
int clean_suite1(void) {
    return 0;
}

void dummyLogFileChanged(const TCHAR *logFile) {
}
int init_wrapper(void) {
    initLogging(dummyLogFileChanged);
    logRegisterThread(WRAPPER_THREAD_MAIN);
    setLogfileLevelInt(LEVEL_NONE);
    setConsoleLogFormat(TEXT("LPM"));
    setConsoleLogLevelInt(LEVEL_DEBUG);
    setConsoleFlush(TRUE);
    setSyslogLevelInt(LEVEL_NONE);
    return 0;
}


/**
 * Simple test that passes.
 */
void testPass(void) {
    CU_ASSERT(0 == 0);
}

/**
 * Simple test that passes.
 */
void testFail(void) {
    CU_ASSERT(0 == 1);
}

/********************************************************************
 * Common Tools
 *******************************************************************/
TCHAR *randomChars = TEXT("0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_-");
#define WORK_BUFFER_LEN 4096
TCHAR workBuffer[WORK_BUFFER_LEN];

int getRandom(int min, int max) {
    int num;
    int rNum;
    
    num = max + 1 - min;
    if (num <= 0) {
        return min;
    }
    
    /* Some platforms use very large RAND_MAX values that cause overflow problems in our math */
    if (RAND_MAX > 0x10000) {
        rNum = (int)((rand() >> 8) * num / (RAND_MAX >> 8));
    } else {
        rNum = (int)(rand() * num / RAND_MAX);
    }
    
    return min + rNum;
}

/**
 * Creates a string of random characters that is within the specified range of lengths.
 * It is the responsibility of the caller to free up the string.
 *
 * @param minLen Minimum Length of the string.
 * @param maxLen Maximum Length of the string.
 *
 * @return the requested string, or NULL if out of memory.
 */
TCHAR *buildRandomString(int minLen, int maxLen) {
    int num;
    int len;
    TCHAR *str;
    int i;
    
    num = _tcslen(randomChars);
    
    len = getRandom(minLen, maxLen);
    
    str = malloc(sizeof(TCHAR) * (len + 1));
    if (!str) {
        return NULL;
    }
    
    for (i = 0; i < len; i++) {
        str[i] = randomChars[getRandom(0, num - 1)];
    }
    str[len] = TEXT('\0');
    
    return str;
}

/**
 * Creates a string of random characters that is within the specified range of lengths.
 * It is the responsibility of the caller to free up the string.
 *
 * @param minLen Minimum Length of the string.
 * @param maxLen Maximum Length of the string.
 *
 * @return the requested string, or NULL if out of memory.
 */
TCHAR *buildRandomStrinWithTail(int minLen, int maxLen, int tail) {
    int num;
    size_t len;
    size_t strLen;
    TCHAR *str;
    size_t i;
    TCHAR tailStr[32];
    
    _sntprintf(tailStr, 32, TEXT("-%d"), tail);
    
    num = _tcslen(randomChars);
    
    len = getRandom(minLen, maxLen);
    
    strLen = len + _tcslen(tailStr) + 1; 
    str = malloc(sizeof(TCHAR) * strLen);
    if (!str) {
        return NULL;
    }
    
    for (i = 0; i < len; i++) {
        str[i] = randomChars[getRandom(0, num - 1)];
    }
    str[len] = TEXT('\0');
    _tcsncat(str, tailStr, strLen);
    
    return str;
}

/**
 * Frees up an array and its contents.  Depends on the values being NULL if they are not allocated.
 *
 * @param array Array to be freed.
 */
void freeTCHARArray(TCHAR **array, int len) {
    int i;
    
    if (array) {
        for (i = 0; i < len; i++) {
            if (array[i]) {
                free(array[i]);
            }
        }
        
        free(array);
    }
}

/********************************************************************
 * Filter Tests
 *******************************************************************/
void subTestWrapperWildcardMatch(const TCHAR *pattern, const TCHAR *text, size_t expectedMinLen, int expectedMatch) {
    size_t minLen;
    int matched;
    
    minLen = wrapperGetMinimumTextLengthForPattern(pattern);
    if (minLen != expectedMinLen) {
        _sntprintf(workBuffer, WORK_BUFFER_LEN, TEXT("wrapperGetMinimumTextLengthForPattern(\"%s\") returned %d rather than expected %d."), pattern, minLen, expectedMinLen);
        _tprintf(TEXT("%s\n"), workBuffer);
        CU_FAIL(workBuffer);
    } else {
        _sntprintf(workBuffer, WORK_BUFFER_LEN, TEXT("wrapperGetMinimumTextLengthForPattern(\"%s\") returned %d."), pattern, minLen);
        CU_PASS(workBuffer);
    }
    
    matched = wrapperWildcardMatch(text, pattern, expectedMinLen);
    if (matched != expectedMatch) {
        _sntprintf(workBuffer, WORK_BUFFER_LEN, TEXT("wrapperWildcardMatch(\"%s\", \"%s\", %d) returned %s rather than expected %s."),
            text, pattern, expectedMinLen, (matched ? TEXT("TRUE") : TEXT("FALSE")), (expectedMatch ? TEXT("TRUE") : TEXT("FALSE")));
        _tprintf(TEXT("%s\n"), workBuffer);
        CU_FAIL(workBuffer);
    } else {
        _sntprintf(workBuffer, WORK_BUFFER_LEN, TEXT("wrapperWildcardMatch(\"%s\", \"%s\", %d) returned %s."),
            text, pattern, expectedMinLen, (matched ? TEXT("TRUE") : TEXT("FALSE")));
        CU_PASS(workBuffer);
    }
}

void testWrapperWildcardMatch() {
    subTestWrapperWildcardMatch(TEXT("a"), TEXT("a"), 1, TRUE);
    subTestWrapperWildcardMatch(TEXT("a"), TEXT("b"), 1, FALSE);
    subTestWrapperWildcardMatch(TEXT("a"), TEXT(""), 1, FALSE);
    
    subTestWrapperWildcardMatch(TEXT("a"), TEXT("abc"), 1, TRUE);
    subTestWrapperWildcardMatch(TEXT("b"), TEXT("abc"), 1, TRUE);
    subTestWrapperWildcardMatch(TEXT("c"), TEXT("abc"), 1, TRUE);
    subTestWrapperWildcardMatch(TEXT("d"), TEXT("abc"), 1, FALSE);
    
    subTestWrapperWildcardMatch(TEXT("?"), TEXT("a"), 1, TRUE);
    subTestWrapperWildcardMatch(TEXT("?"), TEXT(""), 1, FALSE);
    
    subTestWrapperWildcardMatch(TEXT("*"), TEXT(""), 0, TRUE);
    subTestWrapperWildcardMatch(TEXT("*"), TEXT("a"), 0, TRUE);
    subTestWrapperWildcardMatch(TEXT("*"), TEXT("abc"), 0, TRUE);
    
    subTestWrapperWildcardMatch(TEXT("*a"), TEXT("a"), 1, TRUE);
    subTestWrapperWildcardMatch(TEXT("*a"), TEXT("abc"), 1, TRUE);
    subTestWrapperWildcardMatch(TEXT("*b"), TEXT("abc"), 1, TRUE);
    subTestWrapperWildcardMatch(TEXT("a*"), TEXT("a"), 1, TRUE);
    subTestWrapperWildcardMatch(TEXT("a*"), TEXT("abc"), 1, TRUE);
    subTestWrapperWildcardMatch(TEXT("b*"), TEXT("abc"), 1, TRUE);
    subTestWrapperWildcardMatch(TEXT("*a*"), TEXT("a"), 1, TRUE);
    subTestWrapperWildcardMatch(TEXT("*a*"), TEXT("abc"), 1, TRUE);
    subTestWrapperWildcardMatch(TEXT("*b*"), TEXT("abc"), 1, TRUE);
    
    subTestWrapperWildcardMatch(TEXT("HEAD*TAIL"), TEXT("This is the HEAD and this is the TAIL....."), 8, TRUE);
    subTestWrapperWildcardMatch(TEXT("HEAD**TAIL"), TEXT("This is the HEAD and this is the TAIL....."), 8, TRUE);
    subTestWrapperWildcardMatch(TEXT("*HEAD*TAIL*"), TEXT("This is the HEAD and this is the TAIL....."), 8, TRUE);
    subTestWrapperWildcardMatch(TEXT("HEAD*TAIL"), TEXT("This is the HEAD and this is the TaIL....."), 8, FALSE);
    subTestWrapperWildcardMatch(TEXT("HEAD**TAIL"), TEXT("This is the HEAD and this is the TaIL....."), 8, FALSE);
    subTestWrapperWildcardMatch(TEXT("*HEAD*TAIL*"), TEXT("This is the HEAD and this is the TaIL....."), 8, FALSE);
    subTestWrapperWildcardMatch(TEXT("HEAD*TA?L"), TEXT("This is the HEAD and this is the TAIL....."), 8, TRUE);
    subTestWrapperWildcardMatch(TEXT("HEAD**TA?L"), TEXT("This is the HEAD and this is the TAIL....."), 8, TRUE);
    subTestWrapperWildcardMatch(TEXT("*HEAD*TA?L*"), TEXT("This is the HEAD and this is the TAIL....."), 8, TRUE);
}

/* The main() function for setting up and running the tests.
 * Returns a CUE_SUCCESS on successful running, another
 * CUnit error code on failure.
 */
int main()
{
    CU_pSuite pSuite = NULL;
    CU_pSuite filterSuite = NULL;
    
    /* Initialize the random seed. */
    srand((unsigned)time(NULL));
    
    /* initialize the CUnit test registry */
    if (CUE_SUCCESS != CU_initialize_registry())
    return CU_get_error();
    
    /* add a suite to the registry */
    pSuite = CU_add_suite("Suite_1", init_suite1, clean_suite1);
    if (NULL == pSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    filterSuite = CU_add_suite("Filter Suite", init_wrapper, NULL);
    if (NULL == filterSuite) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    
    /* add the tests to the suite */
    /*
    if ((NULL == CU_add_test(pSuite, "test of pass", testPass)) ||
        (NULL == CU_add_test(pSuite, "test of fail", testFail)) {
        CU_cleanup_registry();
        return CU_get_error();
    }
    */
    
    CU_add_test(filterSuite, "wrapperWildcardMatch", testWrapperWildcardMatch);
    
    /* Run all tests using the CUnit Basic interface */
    CU_basic_set_mode(CU_BRM_VERBOSE);
    CU_basic_run_tests();
    CU_cleanup_registry();
    return CU_get_error();
}

#endif
