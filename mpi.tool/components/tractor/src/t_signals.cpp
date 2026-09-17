/*
 * This file is part of PEAT (Performance Estimation & Analysing Toolkit).
 * Copyright (C) 2026 by Alex Knyazev
 *
 * PEAT is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * PEAT is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with PEAT.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "t_signals.h"
#include "t_defines.h"
#include "tractor.h"
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct sigaction old_handlers[SIGTERM + 1];
static int signals_installed = 0;
void handler_ignore(int i)
{
    printf("SIGNAL %d received...ignored\n", i);
}

void handler_do(int i)
{
    if (gt_info(t_use_signals)) {
#ifdef _TRACTOR_USE_PTPD_
        gt_info(ptpd_finalize());
#endif
        gt_info(pre_finalize(1));
        gt_info(abort());
        gt_info(t_use_signals) = 0;
        restore_signals(-1);
    }
}

void handler_explore(int i, siginfo_t *sinfo, void *context)
{
    printf("SIGNAL %d received... context %p\n", i, context);
    printf("Signal info:\n");
    printf("Number:%d\n", sinfo->si_signo);
    printf("Errno:%d\n", sinfo->si_errno);
    //    printf("Trap:%d\n",sinfo->si_trapno);
    printf("PID:%d\n", sinfo->si_pid);
    printf("UID:%d\n", sinfo->si_uid);
    printf("Status:%d\n", sinfo->si_status);
    printf("Memory Addr:%p\n", sinfo->si_addr);
    printf("Lower:%p,%Upper:%p\n", sinfo->si_lower, sinfo->si_upper);
    printf("Call Instruction Addr:%p\n", sinfo->si_call_addr);
}

void install_signals(int rank)
{
    struct sigaction sa;
    int ret;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = handler_do; // handler_ignore;
    /* SIGUSR is being used by MPICH to terminate child processes in case one faults */
    ret = sigaction(SIGUSR1, &sa, &old_handlers[SIGUSR1]);
    // printf("%d:SIGUSR1=%d\n",rank,ret);
    ret = sigaction(SIGUSR2, &sa, &old_handlers[SIGUSR2]);
    // printf("%d:SIGUSR2=%d\n",rank,ret);
    /* Intercept SIGTERM as well just in case */
    ret = sigaction(SIGTERM, &sa, &old_handlers[SIGTERM]);
    // printf("%d:SIGTERM=%d\n",rank,ret);

    //    memset(&sa, 0, sizeof(sa));
    //    sa.sa_flags=SA_SIGINFO;
    //    sa.sa_sigaction = handler_explore;
    ret = sigaction(SIGBUS, &sa, &old_handlers[SIGBUS]);
    // printf("%d:SIGBUS=%d\n",rank,ret);
    ret = sigaction(SIGFPE, &sa, &old_handlers[SIGFPE]);
    // printf("%d:SIGFPE=%d\n",rank,ret);
    ret = sigaction(SIGSEGV, &sa, &old_handlers[SIGSEGV]);
    // printf("%d:SIGSEGV=%d\n",rank,ret);
    ret = sigaction(SIGABRT, &sa, &old_handlers[SIGABRT]);
    // printf("%d:SIGABRT=%d\n",rank,ret);
    ret = sigaction(SIGILL, &sa, &old_handlers[SIGILL]);
    // printf("%d:SIGILL=%d\n",rank,ret);
    ret = sigaction(SIGPIPE, &sa, &old_handlers[SIGPIPE]);
    // printf("%d:SIGPIPE=%d\n",rank,ret);
    ret = sigaction(SIGALRM, &sa, &old_handlers[SIGALRM]);
    // printf("%d:SIGALRM=%d\n",rank,ret);
    ret = sigaction(SIGQUIT, &sa, &old_handlers[SIGQUIT]);
    // printf("%d:SIGQUIT=%d\n",rank,ret);
    ret = sigaction(SIGINT, &sa, &old_handlers[SIGINT]);
    // printf("%d:SIGINT=%d\n",rank,ret);
    // printf("%d:SIGNALs installed...\n",rank);
    signals_installed = 1;
}

void restore_signals(int rank)
{
    if (signals_installed) {
        sigaction(SIGUSR1, &old_handlers[SIGUSR1], 0);
        sigaction(SIGUSR2, &old_handlers[SIGUSR2], 0);
        sigaction(SIGTERM, &old_handlers[SIGTERM], 0);

        sigaction(SIGBUS, &old_handlers[SIGBUS], 0);
        sigaction(SIGFPE, &old_handlers[SIGFPE], 0);
        sigaction(SIGSEGV, &old_handlers[SIGSEGV], 0);
        sigaction(SIGABRT, &old_handlers[SIGABRT], 0);
        sigaction(SIGILL, &old_handlers[SIGILL], 0);
        sigaction(SIGPIPE, &old_handlers[SIGPIPE], 0);
        sigaction(SIGALRM, &old_handlers[SIGALRM], 0);
        sigaction(SIGQUIT, &old_handlers[SIGQUIT], 0);
        sigaction(SIGINT, &old_handlers[SIGINT], 0);
        // printf("%d: SIGNALs restored...\n",rank);
    }
}
