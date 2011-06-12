/*
    SDL_main.c, placed in the public domain by Sam Lantinga  4/13/98

    The WinMain function -- calls your program's main() function
*/

#include <stdio.h>
#include <stdlib.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#ifdef _WIN32_WCE

# define DIR_SEPERATOR TEXT("\\")
# define setbuf(f,b)
# define setvbuf(w,x,y,z)
# define fopen		_wfopen
# define freopen	_wfreopen
# define remove(x)	DeleteFile(x)
# define strcat		wcscat

TCHAR fileUnc[MAX_PATH+1];
static TCHAR *_getcwd(TCHAR *buffer, int maxlen)
{
	TCHAR *plast;

	GetModuleFileName(NULL, fileUnc, MAX_PATH);
	plast = wcsrchr(fileUnc, TEXT('\\'));
	if(plast) *plast = 0;
	/* Special trick to keep start menu clean... */
	if(_wcsicmp(fileUnc, TEXT("\\windows\\start menu")) == 0)
		wcscpy(fileUnc, TEXT("\\Apps"));

	if(buffer) wcsncpy(buffer, fileUnc, maxlen);
	return fileUnc;
}

# if defined(_WIN32_WCE) && _WIN32_WCE < 300
/* seems to be undefined in Win CE although in online help */
# define isspace(a) (((CHAR)a == ' ') || ((CHAR)a == '\t'))

/* seems to be undefined in Win CE although in online help */
char *strrchr(char *str, int c)
{
	char *p;

	/* Skip to the end of the string */
	p=str;
	while (*p)
		p++;

	/* Look for the given character */
	while ( (p >= str) && (*p != (CHAR)c) )
		p--;

	/* Return NULL if character not found */
	if ( p < str ) {
		p = NULL;
	}
	return p;
}
# endif /* _WIN32_WCE < 300 */

#else
# define DIR_SEPERATOR TEXT("/")
# include <direct.h>

#endif

/* The standard output files */
# define STDOUT_FILE	TEXT("stdout.txt")
# define STDERR_FILE	TEXT("stderr.txt")

/* Include the SDL main definition header */
#include "SDL.h"
#include "SDL_main.h"

#ifdef main
# ifndef _WIN32_WCE_EMULATION
#  undef main
# endif /* _WIN32_WCE_EMULATION */
#endif /* main */

#ifndef NO_STDIO_REDIRECT
# ifndef _WIN32_WCE
  static char stdoutPath[MAX_PATH];
  static char stderrPath[MAX_PATH];
# else
  static wchar_t stdoutPath[MAX_PATH];
  static wchar_t stderrPath[MAX_PATH];
# endif
#endif


/* Parse a command line buffer into arguments */
static int ParseCommandLine(char *cmdline, char **argv)
{
	char *bufp;
	int argc;

	argc = 0;
	for ( bufp = cmdline; *bufp; ) {
		/* Skip leading whitespace */
		while ( isspace(*bufp) ) {
			++bufp;
		}
		/* Skip over argument */
		if ( *bufp == '"' ) {
			++bufp;
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			/* Skip over word */
			while ( *bufp && (*bufp != '"') ) {
				++bufp;
			}
		} else {
			if ( *bufp ) {
				if ( argv ) {
					argv[argc] = bufp;
				}
				++argc;
			}
			/* Skip over word */
			while ( *bufp && ! isspace(*bufp) ) {
				++bufp;
			}
		}
		if ( *bufp ) {
			if ( argv ) {
				*bufp = '\0';
			}
			++bufp;
		}
	}
	if ( argv ) {
		argv[argc] = NULL;
	}
	return(argc);
}

/* Show an error message */
static void ShowError(const char *title, const char *message)
{
/* If USE_MESSAGEBOX is defined, you need to link with user32.lib */
#ifdef USE_MESSAGEBOX
	MessageBox(NULL, message, title, MB_ICONEXCLAMATION|MB_OK);
#else
	fprintf(stderr, "%s: %s\n", title, message);
#endif
}

/* Pop up an out of memory message, returns to Windows */
static BOOL OutOfMemory(void)
{
	ShowError("Fatal Error", "Out of memory - aborting");
	return FALSE;
}

/* SDL_Quit() shouldn't be used with atexit() directly because
   calling conventions may differ... */
static void cleanup(void)
{
	SDL_Quit();
}

/* Remove the output files if there was no output written */
static void cleanup_output(void)
{
#ifndef NO_STDIO_REDIRECT
	FILE *file;
	int empty;
#endif

	/* Flush the output in case anything is queued */
	fclose(stdout);
	fclose(stderr);

#ifndef NO_STDIO_REDIRECT
	/* See if the files have any output in them */
	if ( stdoutPath[0] ) {
		file = (FILE *) fopen(stdoutPath, TEXT("rb"));
		if ( file ) {
			empty = (fgetc(file) == EOF) ? 1 : 0;
			fclose(file);
			if ( empty ) {
				remove(stdoutPath);
			}
		}
	}
	if ( stderrPath[0] ) {
		file = (FILE *) fopen(stderrPath, TEXT("rb"));
		if ( file ) {
			empty = (fgetc(file) == EOF) ? 1 : 0;
			fclose(file);
			if ( empty ) {
				remove(stderrPath);
			}
		}
	}
#endif
}

#if defined(_MSC_VER) && !defined(_WIN32_WCE)
/* The VC++ compiler needs main defined */
#define console_main main
#endif

/* This is where execution begins [console apps] */
int console_main(int argc, char *argv[])
{
	size_t n;
	char *bufp, *appname;
	int status;

	/* Get the class name from argv[0] */
	appname = argv[0];
	if ( (bufp=SDL_strrchr(argv[0], '\\')) != NULL ) {
		appname = bufp+1;
	} else
	if ( (bufp=SDL_strrchr(argv[0], '/')) != NULL ) {
		appname = bufp+1;
	}

	if ( (bufp=SDL_strrchr(appname, '.')) == NULL )
		n = SDL_strlen(appname);
	else
		n = (bufp-appname);

	bufp = SDL_stack_alloc(char, n+1);
	if ( bufp == NULL ) {
		return OutOfMemory();
	}
	SDL_strlcpy(bufp, appname, n+1);
	appname = bufp;

	/* Load SDL dynamic link library */
	if ( SDL_Init(SDL_INIT_NOPARACHUTE) < 0 ) {
		ShowError("WinMain() error", SDL_GetError());
		return(FALSE);
	}

#if defined(_WIN32_WCE) && (!defined(GCC_BUILD) && !defined(__GNUC__))
	atexit(cleanup_output);
	atexit(cleanup);
	/* Sam:
	   We still need to pass in the application handle so that
	   DirectInput will initialize properly when SDL_RegisterApp()
	   is called later in the video initialization.
	 */
	SDL_SetModuleHandle(GetModuleHandle(NULL));

	/* Run the application main() code */
	status = SDL_main(argc, argv);

	/* Exit cleanly, calling atexit() functions */
#if defined(_WIN32_WCE) && defined(__GNUC__)
	cleanup_output();
	SDL_Quit();
#endif
	exit(status);

	/* Hush little compiler, don't you cry... */
	return 0;
}

/* This is where execution begins [windowed apps] */
#ifdef _WIN32_WCE
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPWSTR szCmdLine, int sw)
#else
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE hPrev, LPSTR szCmdLine, int sw)
#endif
{
	HINSTANCE handle;
	char **argv;
	int argc;
	char *cmdline;
	DWORD pathlen;
#ifdef _WIN32_WCE
	wchar_t path[MAX_PATH];
#else
	char path[MAX_PATH];
#endif
#ifdef _WIN32_WCE
	wchar_t *bufp;
	int nLen;
#else
	char *bufp;
	size_t nLen;
#endif
#ifndef NO_STDIO_REDIRECT
	FILE *newfp;
#endif

	/* Start up DDHELP.EXE before opening any files, so DDHELP doesn't
	   keep them open.  This is a hack.. hopefully it will be fixed 
	   someday.  DDHELP.EXE starts up the first time DDRAW.DLL is loaded.
	 */
	handle = LoadLibrary(TEXT("DDRAW.DLL"));
	if ( handle != NULL ) {
		FreeLibrary(handle);
	}

#ifndef NO_STDIO_REDIRECT
	pathlen = GetModuleFileName(NULL, path, SDL_arraysize(path));
	while ( pathlen > 0 && path[pathlen] != '\\' ) {
		--pathlen;
	}
	path[pathlen] = '\0';

#ifdef _WIN32_WCE
	wcsncpy( stdoutPath, path, SDL_arraysize(stdoutPath) );
	wcsncat( stdoutPath, DIR_SEPERATOR STDOUT_FILE, SDL_arraysize(stdoutPath) );
#else
	SDL_strlcpy( stdoutPath, path, SDL_arraysize(stdoutPath) );
	SDL_strlcat( stdoutPath, DIR_SEPERATOR STDOUT_FILE, SDL_arraysize(stdoutPath) );
#endif
    
	/* Redirect standard input and standard output */
	newfp = (FILE *) freopen(stdoutPath, TEXT("w"), stdout);

	if ( newfp == NULL ) {	/* This happens on NT */
#if !defined(stdout)
		stdout = fopen(stdoutPath, TEXT("w"));
#else
		newfp = (FILE *) fopen(stdoutPath, TEXT("w"));
		if ( newfp ) {
			*stdout = *newfp;
		}
#endif
	}

#ifdef _WIN32_WCE
	wcsncpy( stderrPath, path, SDL_arraysize(stdoutPath) );
	wcsncat( stderrPath, DIR_SEPERATOR STDOUT_FILE, SDL_arraysize(stdoutPath) );
#else
	SDL_strlcpy( stderrPath, path, SDL_arraysize(stderrPath) );
	SDL_strlcat( stderrPath, DIR_SEPERATOR STDERR_FILE, SDL_arraysize(stderrPath) );
#endif

	newfp = (FILE *) freopen(stderrPath, TEXT("w"), stderr);
	if ( newfp == NULL ) {	/* This happens on NT */
#if !defined(stderr)
		stderr = fopen(stderrPath, TEXT("w"));
#else
		newfp = (FILE *) fopen(stderrPath, TEXT("w"));
		if ( newfp ) {
			*stderr = *newfp;
		}
#endif
	}

	setvbuf(stdout, NULL, _IOLBF, BUFSIZ);	/* Line buffered */
	setbuf(stderr, NULL);			/* No buffering */
#endif /* !NO_STDIO_REDIRECT */

#ifdef _WIN32_WCE
	nLen = wcslen(szCmdLine)+128+1;
	bufp = SDL_stack_alloc(wchar_t, nLen*2);
	wcscpy (bufp, TEXT("\""));
	GetModuleFileName(NULL, bufp+1, 128-3);
	wcscpy (bufp+wcslen(bufp), TEXT("\" "));
	wcsncpy(bufp+wcslen(bufp), szCmdLine,nLen-wcslen(bufp));
	nLen = wcslen(bufp)+1;
	cmdline = SDL_stack_alloc(char, nLen);
	if ( cmdline == NULL ) {
		return OutOfMemory();
	}
	WideCharToMultiByte(CP_ACP, 0, bufp, -1, cmdline, nLen, NULL, NULL);
#else
	/* Grab the command line */
	bufp = GetCommandLine();
	nLen = SDL_strlen(bufp)+1;
	cmdline = SDL_stack_alloc(char, nLen);
	if ( cmdline == NULL ) {
		return OutOfMemory();
	}
	SDL_strlcpy(cmdline, bufp, nLen);
#endif

	/* Parse it into argv and argc */
	argc = ParseCommandLine(cmdline, NULL);
	argv = SDL_stack_alloc(char*, argc+1);
	if ( argv == NULL ) {
		return OutOfMemory();
	}
	ParseCommandLine(cmdline, argv);

	/* fix gdb/emulator combo */
	while (argc > 1 && !strstr(argv[0], ".exe")) {
		OutputDebugString(TEXT("SDL: gdb argv[0] fixup\n"));
		*(argv[1]-1) = ' ';
		int i;
		for (i=1; i<argc; i++)
			argv[i] = argv[i+1];
		argc--;
	}

	/* Run the main program (after a little SDL initialization) */
	console_main(argc, argv);

	/* Hush little compiler, don't you cry... */
	return 0;
}
