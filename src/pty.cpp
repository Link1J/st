#include "pty.hpp"
#include "support.hpp"

#if !defined(_WIN32)
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <wchar.h>

#include <pwd.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <termios.h>
#include <unistd.h>

int   iofd = 0;
pid_t pid;

void sigchld(int a)
{
    int   stat;
    pid_t p;

    if ((p = waitpid(pid, &stat, WNOHANG)) < 0)
        die("waiting for pid %hd failed: %s\n", pid, strerror(errno));

    if (pid != p)
        return;

    if (WIFEXITED(stat) && WEXITSTATUS(stat))
        die("child exited with status %d\n", WEXITSTATUS(stat));
    else if (WIFSIGNALED(stat))
        die("child terminated due to signal %d\n", WTERMSIG(stat));
    _exit(0);
}

#define DEFAULT(a, b) (a) = (a) ? (a) : (b)
#endif

extern char const* termname;
extern char const* utmp;
extern char const* scroll;

pty pty::create(std::string shell, char const** args)
{
    pty output;

#if defined(_WIN32)
    winrt::handle inputReadSide, outputWriteSide;

    winrt::check_bool(CreatePipe(inputReadSide.put(), output.output.put(), NULL, 0));
    winrt::check_bool(CreatePipe(output.input.put(), outputWriteSide.put(), NULL, 0));
    winrt::check_hresult(CreatePseudoConsole(COORD{80, 25}, inputReadSide.get(), outputWriteSide.get(), 0, output.pc.put()));

    STARTUPINFOEX si  = {0};
    si.StartupInfo.cb = sizeof(STARTUPINFOEX);

    size_t bytesRequired;
    InitializeProcThreadAttributeList(NULL, 1, 0, &bytesRequired);
    si.lpAttributeList = (PPROC_THREAD_ATTRIBUTE_LIST)malloc(bytesRequired);
    if (!si.lpAttributeList)
        winrt::throw_hresult(E_OUTOFMEMORY);

    // Initialize the list memory location
    if (!InitializeProcThreadAttributeList(si.lpAttributeList, 1, 0, &bytesRequired))
    {
        free(si.lpAttributeList);
        winrt::throw_last_error();
    }

    if (!UpdateProcThreadAttribute(si.lpAttributeList, 0, PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE, output.pc.get(), sizeof(output.pc), NULL, NULL))
    {
        free(si.lpAttributeList);
        winrt::throw_last_error();
    }

    PROCESS_INFORMATION pi = {0};
    winrt::check_bool(CreateProcess(NULL, shell.data(), NULL, NULL, FALSE, EXTENDED_STARTUPINFO_PRESENT, NULL, NULL, &si.StartupInfo, &pi));

    DeleteProcThreadAttributeList(si.lpAttributeList);
    free(si.lpAttributeList);

#else
#if 0
    if (out)
    {
        term.mode |= MODE_PRINT;
        iofd = (!strcmp(out, "-")) ? 1 : open(out, O_WRONLY | O_CREAT, 0666);
        if (iofd < 0)
        {
            fprintf(stderr, "Error opening %s:%s\n", out, strerror(errno));
        }
    }

    if (line)
    {
        if ((output.output = open(line, O_RDWR)) < 0)
            die("open line '%s' failed: %s\n", line, strerror(errno));
        dup2(output.output, 0);
        stty(args);
        return output;
    }
#endif

    int slave;
    if (openpty(&output.output, &slave, NULL, NULL, NULL) < 0)
        die("openpty failed: %s\n", strerror(errno));

    switch (output.process = fork())
    {
    case -1:
        die("fork failed: %s\n", strerror(errno));
        break;
    case 0:
        close(iofd);
        setsid(); /* create a new process group */
        dup2(slave, 0);
        dup2(slave, 1);
        dup2(slave, 2);
        if (ioctl(slave, TIOCSCTTY, NULL) < 0)
            die("ioctl TIOCSCTTY failed: %s\n", strerror(errno));
        close(slave);
        close(output.output);
#ifdef __OpenBSD__
        if (pledge("stdio getpw proc exec", NULL) == -1)
            die("pledge\n");
#endif
        char const *         sh, *prog, *arg;
        const struct passwd* pw;

        errno = 0;
        if ((pw = getpwuid(getuid())) == NULL)
        {
            if (errno)
                die("getpwuid: %s\n", strerror(errno));
            else
                die("who are you?\n");
        }

        if ((sh = getenv("SHELL")) == NULL)
            sh = (pw->pw_shell[0]) ? pw->pw_shell : shell.data();

        if (args)
        {
            prog = args[0];
            arg  = NULL;
        }
        else if (scroll)
        {
            prog = scroll;
            arg  = utmp ? utmp : sh;
        }
        else if (utmp)
        {
            prog = utmp;
            arg  = NULL;
        }
        else
        {
            prog = sh;
            arg  = NULL;
        }
        DEFAULT(args, ((char const*[]){prog, arg, NULL}));

        unsetenv("COLUMNS");
        unsetenv("LINES");
        unsetenv("TERMCAP");
        setenv("LOGNAME", pw->pw_name, 1);
        setenv("USER", pw->pw_name, 1);
        setenv("SHELL", sh, 1);
        setenv("HOME", pw->pw_dir, 1);
        setenv("TERM", termname, 1);

        signal(SIGCHLD, SIG_DFL);
        signal(SIGHUP, SIG_DFL);
        signal(SIGINT, SIG_DFL);
        signal(SIGQUIT, SIG_DFL);
        signal(SIGTERM, SIG_DFL);
        signal(SIGALRM, SIG_DFL);

        execvp(prog, (char* const*)args);
        _exit(1);
        break;
    default:
#ifdef __OpenBSD__
        if (pledge("stdio rpath tty proc", NULL) == -1)
            die("pledge\n");
#endif
        pid = output.process;
        close(slave);
        signal(SIGCHLD, sigchld);
        break;
    }
#endif

    return output;
}

void pty::resize(int row, int col, int tw, int th)
{
#if defined(_WIN32)
    winrt::check_hresult(ResizePseudoConsole(pc.get(), COORD{(SHORT)tw, (SHORT)th}));
#else
    struct winsize w;
    w.ws_row    = row;
    w.ws_col    = col;
    w.ws_xpixel = tw;
    w.ws_ypixel = th;
    if (ioctl(output, TIOCSWINSZ, &w) < 0)
        fprintf(stderr, "Couldn't set window size: %s\n", strerror(errno));
#endif
}