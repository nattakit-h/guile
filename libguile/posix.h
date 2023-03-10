#ifndef SCM_POSIX_H
#define SCM_POSIX_H

/* Copyright 1995-1998, 2000-2001, 2003, 2006, 2008-2011, 2018, 2021-2023
     Free Software Foundation, Inc.

   This file is part of Guile.

   Guile is free software: you can redistribute it and/or modify it
   under the terms of the GNU Lesser General Public License as published
   by the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Guile is distributed in the hope that it will be useful, but WITHOUT
   ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
   FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public
   License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with Guile.  If not, see
   <https://www.gnu.org/licenses/>.  */



#include "libguile/threads.h"




SCM_API SCM scm_tcsetpgrp (SCM port, SCM pgid);
SCM_API SCM scm_tcgetpgrp (SCM port);
SCM_API SCM scm_ctermid (void);
SCM_API SCM scm_setsid (void);
SCM_API SCM scm_getsid (SCM pid);
SCM_API SCM scm_setpgid (SCM pid, SCM pgid);
SCM_API SCM scm_pipe (void);
SCM_INTERNAL SCM scm_pipe2 (SCM flags);
SCM_API SCM scm_getgroups (void);
SCM_API SCM scm_setgroups (SCM groups);
SCM_API SCM scm_getpgrp (void);
SCM_API SCM scm_getpwuid (SCM user);
SCM_API SCM scm_setpwent (SCM arg);
SCM_API SCM scm_getgrgid (SCM name);
SCM_API SCM scm_setgrent (SCM arg);
SCM_API SCM scm_getrlimit (SCM resource);
SCM_API SCM scm_setrlimit (SCM resource, SCM soft, SCM hard);
SCM_API SCM scm_kill (SCM pid, SCM sig);
SCM_API SCM scm_waitpid (SCM pid, SCM options);
SCM_API SCM scm_status_exit_val (SCM status);
SCM_API SCM scm_status_term_sig (SCM status);
SCM_API SCM scm_status_stop_sig (SCM status);
SCM_API SCM scm_getppid (void);
SCM_API SCM scm_getuid (void);
SCM_API SCM scm_getgid (void);
SCM_API SCM scm_geteuid (void);
SCM_API SCM scm_getegid (void);
SCM_API SCM scm_setuid (SCM uid);
SCM_API SCM scm_setgid (SCM gid);
SCM_API SCM scm_seteuid (SCM euid);
SCM_API SCM scm_setegid (SCM egid);
SCM_API SCM scm_ttyname (SCM port);
SCM_API SCM scm_execl (SCM filename, SCM args);
SCM_API SCM scm_execlp (SCM filename, SCM args);
SCM_API SCM scm_execle (SCM filename, SCM env, SCM args);
SCM_API SCM scm_fork (void);
SCM_API SCM scm_uname (void);
SCM_API SCM scm_environ (SCM env);
SCM_API SCM scm_tmpnam (void);
SCM_API SCM scm_tmpfile (void);
SCM_API SCM scm_open_pipe (SCM pipestr, SCM modes);
SCM_API SCM scm_close_pipe (SCM port);
SCM_INTERNAL SCM scm_spawn_process (SCM prog, SCM arguments, SCM keyword_args);
SCM_API SCM scm_system_star (SCM cmds);
SCM_API SCM scm_utime (SCM object, SCM actime, SCM modtime,
                       SCM actimens, SCM modtimens, SCM flags);
SCM_API SCM scm_access (SCM path, SCM how);
SCM_API SCM scm_getpid (void);
SCM_API SCM scm_putenv (SCM str);
SCM_API SCM scm_setlocale (SCM category, SCM locale);
SCM_API SCM scm_mknod (SCM path, SCM type, SCM perms, SCM dev);
SCM_API SCM scm_nice (SCM incr);
SCM_API SCM scm_sync (void);
SCM_API SCM scm_crypt (SCM key, SCM salt);
SCM_API SCM scm_chroot (SCM path);
SCM_API SCM scm_getlogin (void);
SCM_API SCM scm_getpriority (SCM which, SCM who);
SCM_API SCM scm_setpriority (SCM which, SCM who, SCM prio);
SCM_API SCM scm_getpass (SCM prompt);
SCM_API SCM scm_flock (SCM file, SCM operation);
SCM_API SCM scm_sethostname (SCM name);
SCM_API SCM scm_gethostname (void);
SCM_API SCM scm_getaffinity (SCM pid);
SCM_API SCM scm_setaffinity (SCM pid, SCM cpu_set);
SCM_INTERNAL void scm_init_posix (void);

SCM_INTERNAL scm_i_pthread_mutex_t scm_i_locale_mutex;

#endif  /* SCM_POSIX_H */
