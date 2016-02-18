/*
 * Liftoff is a program like time but specifically written for
 * Linux. It prints process statistics with a higher granularity.
 *
 * Copyright 2016 NEC EUrope Ltd.
 * Authors:
 *    Simon Kuenzer <simon.kuenzer@neclab.eu>
 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <inttypes.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/resource.h>

#define likely(x)   __builtin_expect(!(!(x)),1)
#define unlikely(x) __builtin_expect(!(!(x)),0)

int main(int argc, char *argv[])
{
  pid_t pid, caught;
  sighandler_t irqsig, quitsig;
  struct timeval start, elapsed;
  struct rusage ru;
  int status;
  FILE *sout = stdout;

  if (argc < 1)
    exit(1);
  if (argc == 1) {
    fprintf(stdout, "Usage: %s [PROGRAM] [[ARGUMENTS]]..\n", argv[0]);
    goto err_exit;
  }

  /* disable signal handler */
  irqsig  = signal(SIGINT, SIG_IGN);
  quitsig = signal(SIGQUIT, SIG_IGN);

  gettimeofday(&start, NULL);

  pid = fork();
  if (unlikely(pid < 0)) {
    fprintf(stderr, "%s: failed to fork: %s\n",
	    argv[0], strerror(errno));
    goto err_exit;
  }
  if (pid == 0) {
    /* child */
    execvp(argv[1], &argv[1]); /* does not return on success */
    fprintf(stderr, "%s: failed to execute %s: %s\n",
	    argv[0], argv[1], strerror(errno));
    goto err_exit;
  }
  /* wait until child closed */
  while ((caught = wait3(&status, 0, &ru)) != pid) {
    if (unlikely(caught == -1)) {
      fprintf(stderr, "%s: failed to wait for child %d: %s\n",
	      argv[0], pid, strerror(errno));
      goto err_exit;
    }
  }

  gettimeofday(&elapsed, NULL);
  timersub(&elapsed, &start, &elapsed);

  /* restore signal handler  */
  signal(SIGINT,  irqsig);
  signal(SIGQUIT, quitsig);

  /* print stats */
  fprintf(sout, "real:                         %"PRIu64".%06"PRIu64" s\n",
	  (uint64_t) elapsed.tv_sec,
	  (uint64_t) elapsed.tv_usec);
  fprintf(sout, "user:                         %"PRIu64".%06"PRIu64" s\n",
	  (uint64_t) ru.ru_utime.tv_sec,
	  (uint64_t) ru.ru_utime.tv_usec);
  fprintf(sout, "sys:                          %"PRIu64".%06"PRIu64" s\n",
	  (uint64_t) ru.ru_stime.tv_sec,
	  (uint64_t) ru.ru_stime.tv_usec);
  fprintf(sout, "maximum resident set size:    %ld kb\n", ru.ru_maxrss);
  fprintf(sout, "soft page faults:             %ld\n", ru.ru_minflt);
  fprintf(sout, "hard page faults:             %ld\n", ru.ru_majflt);
  fprintf(sout, "fs input ops:                 %ld\n", ru.ru_inblock);
  fprintf(sout, "fs output ops:                %ld\n", ru.ru_oublock);
  fprintf(sout, "voluntary context switches:   %ld\n", ru.ru_nvcsw);
  fprintf(sout, "involuntary context switches: %ld\n", ru.ru_nivcsw);

  fflush(sout);
  exit(0);

 err_exit:
  fflush(stdout);
  fflush(stderr);
  exit(1);
}
