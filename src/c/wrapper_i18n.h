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

#ifndef _LOCALIZE
 #define _LOCALIZE
 #include <stdio.h>


 #ifndef WIN32

  #ifdef UNICODE
   #include <wchar.h>
  #ifdef _sntprintf
   #undef _sntprintf
  #endif

  #include <stdarg.h>
  #include <stdlib.h>
  #include <unistd.h>
  #include <sys/types.h>
  #include <sys/stat.h>
  #include <locale.h>
  #include <syslog.h>
  #include <time.h>
  #include <wctype.h>
  
  #define __max(x,y) (((x) > (y)) ? (x) : (y))
  #define __min(x,y) (((x) < (y)) ? (x) : (y))

  #if defined(SOLARIS) || defined(HPUX)
   #define WRAPPER_USE_PUTENV
  #endif


  #if defined(MACOSX) || defined(HPUX) || defined(FREEBSD) || defined(SOLARIS)
   #ifndef wcscasecmp
extern int wcscasecmp(const wchar_t* s1, const wchar_t* s2);
    #define ECSCASECMP
   #endif
  #endif


  #define TEXT(x) L##x
typedef wchar_t TCHAR;
typedef wchar_t _TUCHAR;

extern int _tprintf(const wchar_t *fmt,...) ;
extern int multiByteToWideChar(const char *multiByteChars, const char *multiByteEncoding, char *interumEncoding, wchar_t **outputBuffer, int localizeErrorMessage);

#define _taccess      _waccess
#define _tstoi64      _wtoi64
#define _ttoi64       _wtoi64
#define cgetts        _cgetws
extern int _tchdir(const TCHAR *path);
extern int _texecvp(TCHAR* arg, TCHAR **cmd);
extern int _tmkfifo(TCHAR* arg, mode_t mode);
#define _tchmod       _wchmod
#define _tcprintf     _cwprintf
#define _cputts       _cputws
#define _tcreat       _wcreat
#define _tcscanf      _cwscanf
#define _tctime64     _wctime64
#define _texecl       _wexecl
#define _texecle      _wexecle
#define _texeclp      _wexeclp
#define _texeclpe     _wexeclpe
#define _texecv       _wexecv
extern int _texecve(TCHAR* arg, TCHAR **cmd, TCHAR** env);
#define _texecvpe     _wexecvpe
#define _tfdopen      _wfdopen
#define _fgettchar    _fgetwchar
#define _tfindfirst   _wfindfirst
#define _tfindnext64  _wfindnext64
#define _tfindnext    _wfindnext
#define _tfindnexti64 _wfindnexti64
#define _fputtchar    _fputwchar
#define _tfsopen      _wfsopen
#define _tfullpath    _wfullpath
#define _gettch       _getwch
#define _gettche      _getwche
extern TCHAR* _tgetcwd(TCHAR *buf, size_t size);
#define _tgetdcwd     _wgetdcwd
#define _ltot         _ltow
#define _tmakepath    _wmakepath
#define _tmkdir       _wmkdir
#define _tmktemp      _wmktemp
extern int _topen(const TCHAR *path, int oflag, mode_t mode);
#define _tpopen       _wpopen
#define _puttch       _putwch
#if defined(WRAPPER_USE_PUTENV)
extern int _tputenv(const TCHAR *string);
#else
extern int _tsetenv(const TCHAR *name, const TCHAR *value, int overwrite);
extern void _tunsetenv(const TCHAR *name);
#endif
#define _trmdir       _wrmdir
#define _sctprintf    _scwprintf
#define _tsearchenv   _wsearchenv

#define _sntscanf     _snwscanf
#define _tsopen       _wsopen
#define _tspawnl      _wspawnl
#define _tspawnle     _wspawnle
#define _tspawnlp     _wspawnlp
#define _tspawnlpe    _wspawnlpe
#define _tspawnv      _wspawnv
#define _tspawnve     _wspawnve
#define _tspawnvp     _wspawnvp
#define _tspawnvpe    _wspawnvpe
#define _tsplitpath   _wsplitpath
#define _tstat64      _wstat64
extern int _tstat(const wchar_t* filename, struct stat *buf);

#define _tstati64     _wstati64
#define _tstrdate     _wstrdate
#define _tcsdec       _wcsdec
#define _tcsdup       _wcsdup
#define _tcsicmp      wcscasecmp
extern wchar_t* _trealpath(const wchar_t* file_name, wchar_t *resolved_name) ;
#define _tcsicoll     _wcsicoll
#define _tcsinc       _wcsinc
#define _tcslwr       _wcslwr
#define _tcsnbcnt     _wcsncnt
#define _tcsnccnt     _wcsncnt
#define _tcsnccnt     _wcsncnt
#define _tcsnccoll    _wcsncoll
#define _tcsnextc     _wcsnextc
#define _tcsncicmp    _wcsnicmp
#define _tcsnicmp     _wcsnicmp
#define _tcsncicoll   _wcsnicoll
#define _tcsnicoll    _wcsnicoll
#define _tcsninc      _wcsninc
#define _tcsncset     _wcsnset
#define _tcsnset      _wcsnset
#define _tcsrev       _wcsrev
#define _tcsset       _wcsset
#define _tcsspnp      _wcsspnp
#define _tstrtime     wcsftime
#define _tcstoi64     _wcstoi64
#define _tcstoui64    _wcstoui64
#define _tcsupr       _wcsupr
#define _ttempnam     _wtempnam
#define _ui64tot      _ui64tow
#define _ultot        _ultow
#define _ungettch     _ungetwch
extern int _tunlink(const wchar_t* address);
#define _tutime64     _wutime64
#define _tutime       _wutime
#define _vsctprintf   _vscwprintf
#if defined(HPUX)
extern int _vsntprintf(wchar_t *ws, size_t n, const wchar_t *format, va_list arg);
#else
#define _vsntprintf   vswprintf
#endif
#define _tasctime     _wasctime
#define _tstof        _wtof
#define _tstoi        _wtoi
#define _ttoi(x)      wcstol(x, NULL, 10)
#define _tstol        _wtol
#define _ttol         _wtol
#define _tctime       _wctime
#define _fgettc       fgetwc
#define _fgetts       fgetws
extern FILE * _tfopen(const wchar_t* file, const wchar_t* mode) ;
#define _fputtc       fputwc
#define _fputts       fputws
#define _tfreopen     _wfreopen
#define _ftscanf      fwscanf
#define _gettc        getwc
#define _gettchar     getwchar
extern TCHAR * _tgetenv ( const TCHAR * name );
#define _getts        getws
#define _istalnum     iswalnum
#define _istalpha     iswalpha
#define _istascii     iswascii
#define _istcntrl     iswcntrl
#define _istdigit     iswdigit
#define _istgraph     iswgraph
#define _istleadbyte  isleadbyte
#define _istlower     iswlower
#define _istprint     iswprint
#define _istpunct     iswpunct
#define _istspace     iswspace
#define _istupper     iswupper
#define _istxdigit    iswxdigit
#define _tmain        wmain
#define _tperror      _wperror
/*_tprintf  wprintf*/
#define _puttc        putwc
#define _puttchar     putwchar
#define _putts        _putws
extern int _tremove(const TCHAR *path);
extern int _trename(const TCHAR *path, const TCHAR *to);
extern void _topenlog(const TCHAR *ident, int logopt, int facility);
extern void _tsyslog(int priority, const TCHAR *message);
#define _tscanf       wscanf
extern TCHAR *_tsetlocale(int category, const TCHAR *locale) ;
extern int _sntprintf(TCHAR *str, size_t size, const TCHAR *format, ...);
#define _stprintf     _sntprintf
extern int _ftprintf(FILE *stream, const wchar_t *format, ...);
#define _stscanf      swscanf
#define _tcscat       wcscat
#define _tcschr       wcschr
#define _tcscmp       wcscmp
#define _tcscoll      wcscoll
#define _tcscpy       wcscpy
#define _tcscspn      wcscspn
#define _tcserror     _wcserror
#define _tcsftime     wcsftime
#define _tcsclen      wcslen
#define _tcslen       wcslen
#define _tcsncat      wcsncat
#define _tcsnccat     wcsncat
#define _tcsnccmp     wcsncmp
#define _tcsncmp      wcsncmp
#define _tcsnccpy     wcsncpy
#define _tcsncpy      wcsncpy
#define _tcspbrk      wcspbrk
#define _tcsrchr      wcsrchr
#define _tcsspn       wcsspn
#define _tcsstr       wcsstr
#define _tcstod       wcstod
#define _tcstok       wcstok
#define _tcstol       wcstol
#define _tcstoul      wcstoul
#define _tcsxfrm      wcsxfrm
#define _tsystem      _wsystem
#define _ttmpnam      _wtmpnam
#define _totlower     towlower
#define _totupper     towupper
#define _ungettc      ungetwc
#define _vftprintf    vfwprintf
#define _vtprintf     vwprintf
#define _vstprintf    vswprintf
extern size_t _treadlink(TCHAR* exe, TCHAR* fullpath, size_t size);

extern long _tpathconf(const TCHAR *path, int name);

#else /* ASCII */

#define TEXT(x) x
typedef char TCHAR;
typedef unsigned char _TUCHAR;
#define _tpathconf    pathconf
#define _taccess      _access
#define _treadlink    readlink
#define _tstoi64      _atoi64
#define _ttoi64       _atoi64
#define cgetts        _cgets
#define _tchdir       chdir
#define _tchmod       chmod
#define _tcprintf     _cprintf
#define _cputts       _cputs
#define _tcreat       _creat
#define _tcscanf      _cscanf
#define _tctime64     _ctime64
#define _tmkfifo      mkfifo
#define _texecl       execl
#define _texecle      execle
#define _texeclp      execlp
#define _texeclpe     execlpe
#define _texecv       execv
#define _texecve      execve
#define _texecvp      execvp
#define _texecvpe     execvpe
#define _tfdopen      _fdopen
#define _fgettchar    _fgetchar
#define _tfindfirst   _findfirst
#define _tfindnext64  _findnext64
#define _tfindnext    _findnext
#define _tfindnexti64 _findnexti64
#define _fputtchar    _fputchar
#define _tfsopen      _fsopen
#define _tfullpath    _fullpath
#define _gettch       _getch
#define _gettche      _getche
#define _tgetcwd      getcwd
#define _tgetdcwd     getdcwd
#define _ltot         _ltoa
#define _tmakepath    _makepath
#define _tmkdir       _mkdir
#define _tmktemp      _mktemp
#define _topen        open
#define _tpopen       _popen
#define _puttch       _putch
/*#define _tputenv      putenv*/
#define _tsetenv      setenv
#define _tunsetenv      unsetenv
#define _trmdir       _rmdir
#define _sctprintf    _scprintf
#define _tsearchenv   _searchenv
#define _sntprintf    _snprintf
#define _sntscanf     _snscanf
#define _tsopen       _sopen
#define _tspawnl      _spawnl
#define _tspawnle     _spawnle
#define _tspawnlp     _spawnlp
#define _tspawnlpe    _spawnlpe
#define _tspawnv      _spawnv
#define _tspawnve     _spawnve
#define _tspawnvp     _spawnvp
#define _tspawnvpe    _spawnvpe
#define _tsplitpath   _splitpath
#define _tstat64      _stat64
#define _tstat        stat
#define _tstati64     _stati64
#define _tstrdate     _strdate
#define _tcsdec       _strdec
#define _tcsdup       _strdup
#define _tcsicmp      strcasecmp
#define _tcsicoll     _stricoll
#define _tcsinc       _strinc
#define _trealpath    realpath
#define _tcslwr       _strlwr
#define _tcsnbcnt     _strncnt
#define _tcsnccnt     _strncnt
#define _tcsnccnt     _strncnt
#define _tcsnccoll    _strncoll
#define _tcsnextc     _strnextc
#define _tcsncicmp    _strnicmp
#define _tcsnicmp     _strnicmp
#define _tcsncicoll   _strnicoll
#define _tcsnicoll    _strnicoll
#define _tcsninc      _strninc
#define _tcsncset     _strnset
#define _tcsnset      _strnset
#define _tcsrev       _strrev
#define _tcsset       _strset
#define _tcsspnp      _strspnp
#define _tstrtime     strftime
#define _tcstoi64     _strtoi64
#define _tcstoui64    _strtoui64
#define _tcsupr       _strupr
#define _ttempnam     _tempnam
#define _ui64tot      _ui64toa
#define _ultot        _ultoa
#define _ungettch     _ungetch
#define _tunlink      unlink
#define _tutime64     _utime64
#define _tutime       _utime
#define _vsctprintf   _vscprintf
#define _vsntprintf   vsnprintf
#define _tasctime     asctime
#define _tstof        atof
#define _tstoi        atoi
#define _ttoi         atoi
#define _tstol        atol
#define _ttol         atol
#define _tctime       ctime
#define _fgettc       fgetc
#define _fgetts       fgets
#define _tfopen       fopen
#define _ftprintf     fprintf
#define _fputtc       fputc
#define _fputts       fputs
#define _tfreopen     freopen
#define _ftscanf      fscanf
#define _gettc        getc
#define _gettchar     getchar
#define _tgetenv      getenv
#define _getts        gets
#define _istalnum     isalnum
#define _istalpha     isalpha
#define _istascii     isascii
#define _istcntrl     iscntrl
#define _istdigit     isdigit
#define _istgraph     isgraph
#define _istlead      islead
#define _istleadbyte  isleadbyte
#define _istlegal     islegal
#define _istlower     islower
#define _istprint     isprint
#define _istpunct     ispunct
#define _istspace     isspace
#define _istupper     isupper
#define _istxdigit    isxdigit
#define _tmain        main
#define _tperror      perror
#define _tprintf      printf
#define _puttc        putc
#define _puttchar     putchar
#define _putts        puts
#define _tremove      remove
#define _trename      rename
#define _tscanf       scanf
#define _tsetlocale   setlocale
#define _sntprintf    snprintf
#define _stscanf      sscanf
#define _tcscat       strcat
#define _tcschr       strchr
#define _tcscmp       strcmp
#define _tcscoll      strcoll
#define _tcscpy       strcpy
#define _tcscspn      strcspn
#define _tcserror     strerror
#define _tcsftime     strftime
#define _tcsclen      strlen
#define _tcslen       strlen
#define _tcsncat      strncat
#define _tcsnccat     strncat
#define _tcsnccmp     strncmp
#define _tcsncmp      strncmp
#define _tcsnccpy     strncpy
#define _tcsncpy      strncpy
#define _tcspbrk      strpbrk
#define _tcsrchr      strrchr
#define _tcsspn       strspn
#define _tcsstr       strstr
#define _tcstod       strtod
#define _tcstok       strtok
#define _tcstol       strtol
#define _tcstoul      strtoul
#define _tcsxfrm      strxfrm
#define _tsystem      system
#define _ttmpnam      tmpnam
#define _totlower     tolower
#define _totupper     toupper
#define _ungettc      ungetc
#define _vftprintf    vfprintf
#define _vtprintf     vprintf
#define _vstprintf    vsprintf
#define _topenlog     openlog
#define _tsyslog      syslog
#endif
#else /* WIN32 */
#include <tchar.h>
#include <sys/types.h>
#include <sys/stat.h>
extern int multiByteToWideChar(const char *multiByteChars, int encoding, TCHAR **outputBufferW, int localizeErrorMessage);
#endif
#endif
