/*
 * Liftoff - Retrieve and print process statistics
 *
 * Authors: Simon Kuenzer <simon.kuenzer@neclab.eu>
 *
 *
 * Copyright (c) 2017, NEC Europe Ltd., NEC Corporation. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived from
 *    this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 * THIS HEADER MAY NOT BE EXTRACTED OR MODIFIED IN ANY WAY.
 */

#define PROGNAME "liftoff"

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <inttypes.h>
#include <getopt.h>
#include <unistd.h>
#include <signal.h>
#include <sched.h>
#include <time.h>
#include <sys/types.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>

#define MAX_NUMCPUS 2048

#define likely(x)   __builtin_expect(!(!(x)),1)
#define unlikely(x) __builtin_expect(!(!(x)),0)
#define sync()      __sync_synchronize()

struct opts {
  int exitcode;
  const char *sout_fpath;
  int sout_append;
  int pin_cpu;
  int seconds;
};

static const char *sopts = "h?Veo:asc:";
static struct option lopts[] = {
  {"help",	no_argument,		NULL,	'h'},
  {"version",	no_argument,		NULL,	'V'},
  {"exitcode",	no_argument,		NULL,	'e'},
  {"out",	required_argument,	NULL,	'o'},
  {"append",	no_argument,		NULL,	'a'},
  {"seconds",	no_argument,		NULL,	's'},
  {"cpu",	required_argument,	NULL,	'c'},
  {NULL, 0, NULL, 0}
};

static void version()
{
  printf("%s (built: %s %s)\n", PROGNAME, __DATE__, __TIME__);
}

static void usage(const char *progname)
{
  printf("Usage: %s [[ARGUMENT]].. -- [PROGRAM] [[PROGRAM ARGUMENT]]..\n", progname);
  printf("\n");
  printf("Mandatory arguments to long options are mandatory for short options too.\n");
  printf("  -h, --help                 display this help and exit\n");
  printf("  -V, --version              display program version and exit\n");
  printf("  -e, --exitcode             return with exit code of PROGRAM\n");
  printf("  -o, --out [FILE]           write statistics to output FILE instead\n");
  printf("                             of standard out\n");
  printf("  -a, --append               append to output file (requires -o)\n");
  printf("  -s, --seconds              show start time in seconds, too\n");
  printf("  -c, --cpu [ID]             pin program to CPU ID\n");
}

static int parseopts(int argc, char *argv[], struct opts *opts)
{
  const char *progname = argv[0];
  char *old_optarg;
  int old_optind;
  int old_optopt;
  char **argvopt;
  int opt, optidx;
  int ret;
  int v;

  memset(opts, 0, sizeof(*opts));
  opts->pin_cpu = -1;

  old_optind = optind;
  old_optopt = optopt;
  old_optarg = optarg;
  argvopt = argv;
  optind = 1;
  while ((opt = getopt_long(argc, argvopt, sopts, lopts, &optidx)) != EOF) {
    switch (opt) {
    case 'h':
    case '?': /* usage */
      usage(progname);
      exit(0);
    case 'V': /* version */
      version();
      exit(0);
    case 'e': /* pass-through exit code */
      opts->exitcode = 1;
      break;
    case 'o': /* output */
      opts->sout_fpath = optarg;
      break;
    case 'a': /* append */
      opts->sout_append = 1;
      break;
    case 's': /* seconds */
      opts->seconds = 1;
      break;
    case 'c': /* cpu pin */
      v = atoi(optarg);
      if (v < 0) {
        fprintf(stderr, "%s: CPU ID has to greater than or equal to 0\n", progname);
        usage(progname);
        ret = -EINVAL;
        goto out;
      }
      opts->pin_cpu = v;
      break;
    default:
      fprintf(stderr, "%s: invalid option\n", progname);
      usage(progname);
      ret = -EINVAL;
      goto out;
    }
  }
  ret = optind;

out:
  /* restore getopt state */
  optind = old_optind;
  optopt = old_optopt;
  optarg = old_optarg;
  return ret;
}

int main(int argc, char *argv[])
{
  FILE *sout = stdout;
  const char *progname;
  struct opts opts;
  pid_t pid, caught;
  sighandler_t irqsig, quitsig;
  struct timeval start, elapsed;
  struct tm start_tm;
  struct rusage ru;
  int status;
  int ret;
  int exitcode;
  int i;
  cpu_set_t *setp;
  char strtime[256];

  if (argc < 1)
    exit(1);
  progname = argv[0];
  if ((ret = parseopts(argc, argv, &opts)) < 0)
    goto err_exit;
  argc -= ret;
  argv += ret;
  if (argc < 1) {
    printf("%s: missing command to run\n", progname);
    usage(progname);
    exit(1);
  }

  /* retrieve current scheduling affinity */
  setp = CPU_ALLOC(MAX_NUMCPUS);
  if (!setp) {
    fprintf(stderr, "%s: could not allocate CPU mask: %s\n",
            progname, strerror(errno));
    goto err_exit;
  }
  ret = sched_getaffinity(0, CPU_ALLOC_SIZE(MAX_NUMCPUS), setp);
  if (ret < 0) {
    fprintf(stderr, "%s: could not get scheduling affinity: %s\n",
            progname, strerror(errno));
    goto err_free_setp;
  }

  /* set new scheduling affinity */
  if (opts.pin_cpu >= 0) {
    CPU_ZERO_S(CPU_ALLOC_SIZE(MAX_NUMCPUS), setp);
    CPU_SET_S(opts.pin_cpu, CPU_ALLOC_SIZE(MAX_NUMCPUS), setp);

    ret = sched_setaffinity(0, CPU_ALLOC_SIZE(MAX_NUMCPUS), setp);
    if (ret < 0) {
      fprintf(stderr, "%s: could not set scheduling affinity: %s\n",
              progname, strerror(errno));
      goto err_free_setp;
    }
  }

  /* disable signal handler */
  irqsig  = signal(SIGINT, SIG_IGN);
  quitsig = signal(SIGQUIT, SIG_IGN);

  sync();
  gettimeofday(&start, NULL);

  pid = fork();
  if (unlikely(pid < 0)) {
    fprintf(stderr, "%s: failed to fork: %s\n",
	    progname, strerror(errno));
    goto err_free_setp;
  }
  if (pid == 0) {
    /* child */
    execvp(argv[0], &argv[0]); /* does not return on success */
    fprintf(stderr, "%s: failed to execute %s: %s\n",
	    progname, argv[0], strerror(errno));
    goto err_free_setp;
  }
  /* wait until child closed */
  while ((caught = wait3(&status, 0, &ru)) != pid) {
    if (unlikely(caught == -1)) {
      fprintf(stderr, "%s: failed to wait for child %d: %s\n",
	      progname, pid, strerror(errno));
      goto err_free_setp;
    }
  }

  gettimeofday(&elapsed, NULL);
  sync();

  timersub(&elapsed, &start, &elapsed);

  /* restore signal handler  */
  signal(SIGINT,  irqsig);
  signal(SIGQUIT, quitsig);

  /* print stats */
  if (opts.sout_fpath) {
    sout = fopen(opts.sout_fpath, opts.sout_append ? "a" : "w");
    if (!sout) {
      fprintf(stderr, "%s: could not open %s for writing, "
              "using standard output instead: %s\n",
	      progname, opts.sout_fpath, strerror(errno));
      sout = stdout;
    }
  }

  if (!opts.seconds) {
    localtime_r((time_t *) &start.tv_sec, &start_tm);
    strftime(strtime, sizeof(strtime), "%Y-%m-%d %H:%M:%S", &start_tm);
    fprintf(sout, "start time:                   %s.%06"PRIu64"\n",
            strtime,
            (uint64_t) start.tv_usec);
  } else {
    fprintf(sout, "start time:                   %"PRIu64".%06"PRIu64" s\n",
            (uint64_t) start.tv_sec,
            (uint64_t) start.tv_usec);

  }
  fprintf(sout, "real execution time:          %"PRIu64".%06"PRIu64" s\n",
          (uint64_t) elapsed.tv_sec,
          (uint64_t) elapsed.tv_usec);
  fprintf(sout, "user execution time:          %"PRIu64".%06"PRIu64" s\n",
          (uint64_t) ru.ru_utime.tv_sec,
          (uint64_t) ru.ru_utime.tv_usec);
  fprintf(sout, "system execution time:        %"PRIu64".%06"PRIu64" s\n",
          (uint64_t) ru.ru_stime.tv_sec,
          (uint64_t) ru.ru_stime.tv_usec);
  fprintf(sout, "initial affinity set:         ");
  for (i=0; i<MAX_NUMCPUS; ++i) {
    if (CPU_ISSET(i, setp)) {
      fprintf(sout, "%d ", i);
    }
  }
  exitcode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
  fprintf(sout, "\n");
  fprintf(sout, "maximum resident set size:    %ld kb\n", ru.ru_maxrss);
  fprintf(sout, "soft page faults:             %ld\n",    ru.ru_minflt);
  fprintf(sout, "hard page faults:             %ld\n",    ru.ru_majflt);
  fprintf(sout, "fs input ops:                 %ld\n",    ru.ru_inblock);
  fprintf(sout, "fs output ops:                %ld\n",    ru.ru_oublock);
  fprintf(sout, "voluntary context switches:   %ld\n",    ru.ru_nvcsw);
  fprintf(sout, "involuntary context switches: %ld\n",    ru.ru_nivcsw);
  fprintf(sout, "exit code:                    %d ",      exitcode);
  if (!(WIFEXITED(status)))
    fprintf(sout, "<crashed>");
  if (WIFSIGNALED(status))
    fprintf(sout, "<terminated by signal %d>", WTERMSIG(status));
  if (WIFSTOPPED(status))
    fprintf(sout, "<terminated by delivery of signal %d>", WSTOPSIG(status));
  if (WCOREDUMP(status))
    fprintf(sout, "<coredump>");
  fprintf(sout, "\n");
  fprintf(sout, "command line:                ");
  for (i=0; i<argc; ++i)
    fprintf(sout, " %s", argv[i]);
  fprintf(sout, "\n");

  fflush(sout);
  if (sout != stdout)
    fclose(sout);
  CPU_FREE(setp);
  if (opts.exitcode)
    exit(exitcode);
  exit(0);

err_free_setp:
  CPU_FREE(setp);
err_exit:
  fflush(stderr);
  exit(1);
}
