#include "con.hpp"
#include "../st.h"

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

void execsh(char const* cmd, char** args)
{
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
        sh = (pw->pw_shell[0]) ? pw->pw_shell : cmd;

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
    DEFAULT(args, ((char*[]){(char*)prog, (char*)arg, NULL}));

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

    execvp(prog, args);
    _exit(1);
}

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

int Con::ttynew(char* line, char const* cmd, char* out, char** args)
{
    int m, s;

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
        if ((pty.output = open(line, O_RDWR)) < 0)
            die("open line '%s' failed: %s\n", line, strerror(errno));
        dup2(pty.output, 0);
        stty(args);
        return pty.output;
    }

    /* seems to work fine on linux, openbsd and freebsd */
    if (openpty(&m, &s, NULL, NULL, NULL) < 0)
        die("openpty failed: %s\n", strerror(errno));

    switch (pty.process = fork())
    {
    case -1:
        die("fork failed: %s\n", strerror(errno));
        break;
    case 0:
        close(iofd);
        setsid(); /* create a new process group */
        dup2(s, 0);
        dup2(s, 1);
        dup2(s, 2);
        if (ioctl(s, TIOCSCTTY, NULL) < 0)
            die("ioctl TIOCSCTTY failed: %s\n", strerror(errno));
        close(s);
        close(m);
#ifdef __OpenBSD__
        if (pledge("stdio getpw proc exec", NULL) == -1)
            die("pledge\n");
#endif
        execsh(cmd, args);
        break;
    default:
#ifdef __OpenBSD__
        if (pledge("stdio rpath tty proc", NULL) == -1)
            die("pledge\n");
#endif
        close(s);
        pid = pty.process;
        pty.output = m;
        signal(SIGCHLD, sigchld);
        break;
    }
    return pty.output;
}

int Con::ttyread_pending()
{
    return twrite_aborted;
}

size_t Con::ttyread(void)
{
    static char buf[BUFSIZ];
    static int  buflen = 0;
    int         ret, written;

    /* append read bytes to unprocessed bytes */
    ret = twrite_aborted ? 1 : read(pty.output, buf + buflen, LEN(buf) - buflen);

    switch (ret)
    {
    case 0:
        exit(0);
    case -1:
        die("couldn't read from shell: %s\n", strerror(errno));
    default:
        buflen += twrite_aborted ? 0 : ret;
        written = twrite({buf, (size_t)buflen}, 0);
        buflen -= written;
        /* keep any incomplete UTF-8 byte sequence for the next call */
        if (buflen > 0)
            memmove(buf, buf + written, buflen);
        return ret;
    }
}

void Con::ttywrite(std::string_view s, int may_echo)
{
    size_t next;
    Arg    arg{.i = term.scr};

    kscrolldown(&arg);

    if (may_echo && IS_SET(MODE_ECHO))
        twrite(s, 1);

    if (!IS_SET(MODE_CRLF))
    {
        ttywriteraw(s);
        return;
    }

    /* This is similar to how the kernel handles ONLCR for ttys */
    while (s.size() > 0)
    {
        if (s[0] == '\r')
        {
            next = 1;
            ttywriteraw("\r\n");
        }
        else
        {
            next = s.find('\r');
            DEFAULT(next, s.size());
            ttywriteraw(s.substr(0, next));
        }
        s = s.substr(next);
    }
}

void Con::ttywriteraw(std::string_view s)
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
    while (s.size() > 0)
    {
        FD_ZERO(&wfd);
        FD_ZERO(&rfd);
        FD_SET(pty.output, &wfd);
        FD_SET(pty.output, &rfd);

        /* Check if we can write. */
        if (pselect(pty.output + 1, &rfd, &wfd, NULL, NULL, NULL) < 0)
        {
            if (errno == EINTR)
                continue;
            die("select failed: %s\n", strerror(errno));
        }
        if (FD_ISSET(pty.output, &wfd))
        {
            /*
             * Only write the bytes written by ttywrite() or the
             * default of 256. This seems to be a reasonable value
             * for a serial line. Bigger values might clog the I/O.
             */
            if ((r = write(pty.output, s.data(), (s.size() < lim) ? s.size() : lim)) < 0)
                goto write_error;
            if (r < s.size())
            {
                /*
                 * We weren't able to write out everything.
                 * This means the buffer is getting full
                 * again. Empty it.
                 */
                if (s.size() < lim)
                    lim = ttyread();
                s = s.substr(r);
            }
            else
            {
                /* All bytes have been written. */
                break;
            }
        }
        if (FD_ISSET(pty.output, &rfd))
            lim = ttyread();
    }
    return;

write_error:
    die("write error on tty: %s\n", strerror(errno));
}

void Con::ttyresize(int tw, int th)
{
    struct winsize w;

    w.ws_row    = term.row;
    w.ws_col    = term.col;
    w.ws_xpixel = tw;
    w.ws_ypixel = th;
    if (ioctl(pty.output, TIOCSWINSZ, &w) < 0)
        fprintf(stderr, "Couldn't set window size: %s\n", strerror(errno));
}

void Con::ttyhangup()
{
    /* Send SIGHUP to shell */
    kill(pty.process, SIGHUP);
}

#endif
