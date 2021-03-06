#include "pty.hpp"
#include "support.hpp"
#include "term.hpp"

#include "st.h"

extern char const* stty_args;
extern char const* termname;
extern char const* utmp;
extern char const* scroll;

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

void stty(char** args)
{
    char   cmd[_POSIX_ARG_MAX], **p, *q, *s;
    size_t n, siz;

    if ((n = strlen(stty_args)) > sizeof(cmd) - 1)
        die("incorrect stty parameters\n");
    memcpy(cmd, stty_args, n);
    q   = cmd + n;
    siz = sizeof(cmd) - n;
    for (p = args; p && (s = *p); ++p)
    {
        if ((n = strlen(s)) > siz - 1)
            die("stty parameter length too long\n");
        *q++ = ' ';
        memcpy(q, s, n);
        q += n;
        siz -= n + 1;
    }
    *q = '\0';
    if (system(cmd) != 0)
        perror("Couldn't call stty");
}

#define DEFAULT(a, b) (a) = (a) ? (a) : (b)
#endif

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

extern int twrite_aborted;

int pty::read_pending()
{
    return twrite_aborted;
}

size_t pty::read(Term& term)
{
    static char buf[BUFSIZ];
    static int  buflen = 0;
    int         ret, written;

    /* append read bytes to unprocessed bytes */
    ret = twrite_aborted ? 1 : ::read(output, buf + buflen, LEN(buf) - buflen);

    switch (ret)
    {
    case 0:
        exit(0);
    case -1:
        die("couldn't read from shell: %s\n", strerror(errno));
    default:
        buflen += twrite_aborted ? 0 : ret;
        written = term.write(buf, buflen, 0);
        buflen -= written;
        /* keep any incomplete UTF-8 byte sequence for the next call */
        if (buflen > 0)
            memmove(buf, buf + written, buflen);
        return ret;
    }
}

void pty::write(Term& term, std::string_view s, bool may_echo)
{
    char const* next;
    Arg         arg = Arg{.i = term.scr};

    kscrolldown(&arg);

    if (may_echo && term.IS_SET(MODE_ECHO))
        term.write(s, s.size(), 1);

    if (!term.IS_SET(MODE_CRLF))
    {
        write_raw(term, s);
        return;
    }

    /* This is similar to how the kernel handles ONLCR for ttys */
    while (!s.empty())
    {
        if (s[0] == '\r')
        {
            next = s.data() + 1;
            write_raw(term, "\r\n");
        }
        else
        {
            next = (char const*)memchr(s.data(), '\r', s.size());
            DEFAULT(next, s.data() + s.size());
            write_raw(term, s.substr(0, next - s.data()));
        }
        s = next;
    }
}

void pty::write_raw(Term& term, std::string_view s)
{
    fd_set  wfd, rfd;
    ssize_t r;
    size_t  lim = 256;

    /*
     * Remember that we are using a pty, which might be a modem line.
     * Writing too much will clog the line. That's why we are doing this
     * dance.
     * FIXME: Migrate the world to Plan 9.
     */
    while (!s.empty())
    {
        FD_ZERO(&wfd);
        FD_ZERO(&rfd);
        FD_SET(output, &wfd);
        FD_SET(output, &rfd);

        /* Check if we can write. */
        if (pselect(output + 1, &rfd, &wfd, NULL, NULL, NULL) < 0)
        {
            if (errno == EINTR)
                continue;
            die("select failed: %s\n", strerror(errno));
        }
        if (FD_ISSET(output, &wfd))
        {
            /*
             * Only write the bytes written by ttywrite() or the
             * default of 256. This seems to be a reasonable value
             * for a serial line. Bigger values might clog the I/O.
             */
            if ((r = ::write(output, s.data(), (s.size() < lim) ? s.size() : lim)) < 0)
                goto write_error;
            if (r < s.size())
            {
                /*
                 * We weren't able to write out everything.
                 * This means the buffer is getting full
                 * again. Empty it.
                 */
                if (s.size() < lim)
                    lim = read(term);
                s = s.substr(r);
            }
            else
            {
                /* All bytes have been written. */
                break;
            }
        }
        if (FD_ISSET(output, &rfd))
            lim = read(term);
    }
    return;

write_error:
    die("write error on tty: %s\n", strerror(errno));
}

void pty::hangup()
{
    /* Send SIGHUP to shell */
    kill(pid, SIGHUP);
}