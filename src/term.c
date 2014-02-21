/* Copyright (C) 1990, 1993 Free Software Foundation, Inc.

   This file is part of GNU ISPELL.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.  */

#include <stdio.h>
#include <signal.h>
#include <setjmp.h>

#ifdef OS2
#include <stdlib.h>
#include <process.h>
#define INCL_DOSPROCESS
#define INCL_SUB
#define INCL_NOPM
#include <os2.h>

#ifdef getchar
#undef getchar
#endif
#define getchar getchr

int
getchr()
{
  static int ext, scan;
  KBDKEYINFO ki;
  int c;

  if ( ext ) {
    ext = 0;
    c = scan;
  }
  else {
    KbdCharIn(&ki, IO_WAIT, 0);
    c = ki.chChar;

    if ( c == 0 || c == 0xE0 ) {
      c = 0xE0;
      ext = 1;
      scan = ki.chScan;
    }
  }
  return (c);
}

#ifdef _MSC_VER
unsigned 
sleep(unsigned sec)
{
  return DosSleep(sec * 1000L);
}
#endif

#else

#if defined (HAVE_TERMIO_H)
#include <termio.h>
#else
/* Assume BSD if all else fails */
#include <sgtty.h>
#endif /* not HAVE_TERMIO_H */

#endif /* OS2 */

#include <signal.h>
#include "ispell.h"


extern int reading_interactive_command;
extern jmp_buf command_loop;

int erasechar, killchar;

/* termcap variables */
static char *BC, *cd, *cl, *so, *se;
#ifndef VENIX
static char *cm, *ho, termcap[1024], termstr[1024];
#endif
static int li;

void
gettermcap ()
{
#ifdef NO_TERMCAP
  /* IBM pc ansi escape codes */
  BC = "\b";
  cl = "\033[0;0H\033[0J";
  cd = "\033[0J";
  so = "\033[47;30m";
  se = "\033[37;40m";
  li = 25;
#else
  char *termptr;
  char *tgetstr ();

  tgetent (termcap, getenv ("TERM"));
  termptr = termstr;
  BC = tgetstr ("bc", &termptr);/* backspace */
  if (BC == NULL)
    BC = "\b";
  cd = tgetstr ("cd", &termptr);/* clear to end of screen */
  cl = tgetstr ("cl", &termptr);/* clear screen */
  cm = tgetstr ("cm", &termptr);/* cursor motion */
  ho = tgetstr ("ho", &termptr);/* home */
  so = tgetstr ("so", &termptr);/* inverse video on */
  se = tgetstr ("se", &termptr);/* inverse video off */
  li = tgetnum ("li");		/* number of lines on screen */
#endif
}

#ifdef NO_TERMCAP
/* ARGSUSED */
void
tputs (str, lines, put)
     char *str;
     int (*put) ();
{
  while (*str)
    (*put) (*str++);
}

#endif

void
putch (c)
  int c;
{
  putchar (c);
}

void
termflush ()
{
  (void) fflush (stdout);
}

void
erase ()
{
  if (cl)
    tputs (cl, li, putch);
  else
    {
      move (0, 0);
      tputs (cd, li, putch);
    }
}

void
move (row, col)
  int row, col;
{
#ifdef NO_TERMCAP
  (void) printf ("\033[%d;%dH", row, col);
#else
  char *tgoto ();
  tputs (tgoto (cm, col, row), 100, putch);
#endif
}


void
inverse ()
{
  tputs (so, 10, putch);
}

void
normal ()
{
  tputs (se, 10, putch);
}

void
backup ()
{
  tputs (BC, 1, putch);
}


static int termchanged = 0;

#ifdef OS2

KBDINFO	initialKbdInfo;	/* keyboard info		*/
KBDINFO kbdInfo;

void
terminit()
{
  initialKbdInfo.cb = sizeof(initialKbdInfo);
  KbdGetStatus(&initialKbdInfo, 0);
  kbdInfo = initialKbdInfo;
  kbdInfo.fsMask &= ~0x0001;		/* not echo on		*/
  kbdInfo.fsMask |= 0x0002;		/* echo off		*/
  kbdInfo.fsMask &= ~0x0008;		/* cooked mode off	*/
  kbdInfo.fsMask |= 0x0004;		/* raw mode		*/
  kbdInfo.fsMask &= ~0x0100;		/* shift report	off	*/
  KbdSetStatus(&kbdInfo, 0);

  termchanged = 1;
  erasechar = 8;
  gettermcap ();
  normal(); erase();
}

void
termuninit ()
{
  if (termchanged)
    {
      KbdSetStatus(&initialKbdInfo, 0); /* restore original state */
      termchanged = 0;
    }
}

void
termreinit ()
{
  if (termchanged == 0)
    {
      KbdSetStatus(&kbdInfo, 0);
      termchanged = 1;
    }
}

#else

#if !defined (HAVE_TERMIOS_H) && !defined (HAVE_TERMIO_H)
static struct sgttyb sbuf, osbuf;

void
terminit ()
{
  int tpgrp;
  int onstop ();

retry:
  sigsetmask (1 << SIGTSTP | 1 << SIGTTIN | 1 << SIGTTOU);
  /* apparently tpgrp was a short on 4.1, but is now a long -
	 * set the high bits to zero in case the ioctl doesn't write them.
	 */
  tpgrp = 0;
  if (ioctl (0, TIOCGPGRP, &tpgrp) != 0)
    {
      (void) fprintf (stderr, "must run from tty in interactive mode\n");
      exit (1);
    }
  if (tpgrp != getpgrp (0))
    {				/* not in foreground */
      (void) sigsetmask (1 << SIGTSTP | 1 << SIGTTIN);
      (void) signal (SIGTTOU, SIG_DFL);
      (void) kill (0, SIGTTOU);
      /* job stops here waiting for SIGCONT */
      goto retry;
    }

  (void) ioctl (0, TIOCGETP, &osbuf);
  termchanged = 1;

  sbuf = osbuf;
  sbuf.sg_flags &= ~ECHO;
  sbuf.sg_flags |= CBREAK;
  ioctl (0, TIOCSETP, &sbuf);

  erasechar = sbuf.sg_erase;
  killchar = sbuf.sg_kill;

  (void) signal (SIGTTIN, onstop);
  (void) signal (SIGTTOU, onstop);
  (void) signal (SIGTSTP, onstop);
  (void) sigsetmask (0);
  gettermcap ();
}

void
termuninit ()
{
  if (termchanged)
    {
      (void) ioctl (0, TIOCSETP, (char *) &osbuf);
      termchanged = 0;
    }
}

void
termreinit ()
{
  if (termchanged == 0)
    {
      (void) ioctl (0, TIOCSETP, (char *) &sbuf);
      termchanged = 1;
    }
}

#endif /* !HAVE_TERMIO_H && !HAVE_TERMIOS_H */

#endif

#if defined (HAVE_TERMIO_H) || defined (HAVE_TERMIOS_H)
struct termio termio, otermio;

void
terminit ()
{
  if (ioctl (0, TCGETA, (char *) &otermio) < 0)
    {
      (void) fprintf (stderr, "must run from tty in interactive mode\n");
      exit (1);
    }

  termchanged = 1;

  termio = otermio;
  termio.c_lflag &= ~(ICANON | ECHO);
  termio.c_cc[VMIN] = 1;
  termio.c_cc[VTIME] = 0;
  erasechar = termio.c_cc[VERASE];
  killchar = termio.c_cc[VKILL];

  (void) ioctl (0, TCSETA, (char *) &termio);

  gettermcap ();
}


void
termuninit ()
{
  if (termchanged)
    {
      (void) ioctl (0, TCSETA, (char *) &otermio);
      termchanged = 0;
    }
}

void
termreinit ()
{
  if (termchanged == 0)
    {
      (void) ioctl (0, TCSETA, (char *) &termio);
      termchanged = 1;
    }
}

#endif /* USG */

#ifdef MSDOS
void terminit () { ; }
void termuninit () { ; }
void termreinit () { ; }
#endif

#ifdef SIGTTIN
void
onstop (signo)
  int signo;
{
  (void) signal (signo, SIG_DFL);
  termuninit ();
  sigsetmask (0);
  (void) killpg (getpgrp (0), signo);
  /* stop here */
  signal (signo, onstop);
  termreinit ();
  if (reading_interactive_command)
    longjmp (command_loop, 1);
}

void
stop ()
{
  onstop (SIGTSTP);
}

#else
void
stop ()
{
  shellescape ((char *) NULL);
}

#endif

static char *shellcmd, *shellsh;;

#if defined(MSDOS) || defined(OS2)

typedef struct {
  char name[_MAX_PATH];
  char opt[3];
} shellbuf;

shellbuf shell;

char *getshell(shellbuf *shell)
{
  static char *sname[] = {"cmd", "command", "4os2", "4dos", NULL};
  char cmd[_MAX_PATH], *shl, *opt;
  int i;

  opt = "-c";
  if ((shl = getenv("SHELL")) == NULL)
    shl = getenv("COMSPEC");
  if (shl) {
    _splitpath(shl, NULL, NULL, cmd, NULL);
    for (i = 0; sname[i]; i++)
      if (stricmp(cmd, sname[i]) == 0) {
        opt = "/c"; break;
      }
  }
  else
    shl = "/bin/sh";
  if (shell) {
    strcpy(shell->name, shl);
    strcpy(shell->opt, opt);
  }
  return(shl);
}

void
shellfunc(char *buf)
{
  void shellfun();
  shellcmd = buf;
  shellfun();
}
  
#endif

void
shellfun ()
{
#if defined(MSDOS) || defined(OS2)
  char *p;
  if (shellsh == NULL) shellsh = getshell(&shell);
  p = (shellcmd && *shellcmd) ? shell.opt : NULL;
  if (-1 == spawnlp( P_WAIT , shellsh, shellsh, p, shellcmd, NULL))
    perror ( "shellfun" ) ;
#else
  if (shellcmd == (char *) 0)
    execlp (shellsh, shellsh, (char *) 0);
  else
    execlp (shellsh, shellsh, "-c", shellcmd, (char *) 0);
#endif
}


void
shellescape (buf)
     char *buf;
{
#if defined(MSDOS) || defined(OS2)
  shellsh = getshell(&shell);
#else
  shellsh = (char *) getenv ("SHELL");
  if (shellsh == NULL)
    shellsh = "/bin/sh";
#endif
  shellcmd = buf;
  (void) dochild (shellfun);
  (void) printf ("\n-- Type space to continue --");
  getchar ();
}

int
dochild (fun)
     void (*fun) (NOARGS);
{
  int pid, w;
  int status, val = 1;
  RETSIGTYPE (*oldf) (NOARGS);
  termuninit ();

  oldf = (RETSIGTYPE (*)()) signal (SIGINT, SIG_IGN);
#ifdef SIGQUIT
  (void) signal (SIGQUIT, SIG_IGN);
#endif
#ifdef SIGTTIN
  (void) signal (SIGTTIN, SIG_DFL);
  (void) signal (SIGTTOU, SIG_DFL);
  (void) signal (SIGTSTP, SIG_DFL);
#endif

#ifdef OS2
  (*fun) ();
  val = 0;
#else
  pid = fork ();
  if (pid < 0)
    {
      perror ("fork");
      goto ret;
    }
  if (pid == 0)
    {
      (void) signal (SIGINT, SIG_DFL);
      (void) signal (SIGQUIT, SIG_DFL);
      (*fun) ();
      _exit (1);
    }
  while ((w = wait (&status)) >= 0)
    if (w == pid)
      break;

  val = (status >> 8) & 0xff;
ret:
#ifdef SIGTTIN
  signal (SIGTTIN, onstop);
  signal (SIGTTOU, onstop);
  signal (SIGTSTP, onstop);
#endif

#endif

  signal (SIGINT, (RETSIGTYPE (*)()) oldf);
#ifdef SIGQUIT
  signal (SIGQUIT, SIG_DFL);
#endif

  termreinit ();

  return (val);
}

void
termbeep ()
{
  (void) putchar (7);
  (void) fflush (stdout);
}
