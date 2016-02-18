/*
 * Liftoff is a program like time but specifically written for
 * Linux. It prints process statistics with a higher granularity.
 *
 * Copyright 2016 NEC EUrope Ltd.
 * Authors:
 *    Simon Kuenzer <simon.kuenzer@neclab.eu>
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
#include <sys/types.h>
#include <sys/param.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>


#define likely(x)   __builtin_expect(!(!(x)),1)
#define unlikely(x) __builtin_expect(!(!(x)),0)
#define sync()      __sync_synchronize()

struct opts {
  const char *sout_fpath;
  int sout_append;
};

static const char *sopts = "h?Vo:a";
static struct option lopts[] = {
  {"help",	no_argument,		NULL,	'h'},
  {"version",	no_argument,		NULL,	'V'},
  {"out",	required_argument,	NULL,	'o'},
  {"append",	no_argument,		NULL,	'a'},
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
  printf("  -o, --out [FILE]           write statistics to output FILE instead\n");
  printf("                             of standard out\n");
  printf("  -a, --append               append to output file (requires -o)\n");
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

  memset(opts, 0, sizeof(*opts));

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
    case 'o': /* output */
      opts->sout_fpath = optarg;
      break;
    case 'a': /* append */
      opts->sout_append = 1;
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
  struct rusage ru;
  int status;
  int ret;
  int i;

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

  /* disable signal handler */
  irqsig  = signal(SIGINT, SIG_IGN);
  quitsig = signal(SIGQUIT, SIG_IGN);

  sync();
  gettimeofday(&start, NULL);

  pid = fork();
  if (unlikely(pid < 0)) {
    fprintf(stderr, "%s: failed to fork: %s\n",
	    progname, strerror(errno));
    goto err_exit;
  }
  if (pid == 0) {
    /* child */
    execvp(argv[0], &argv[0]); /* does not return on success */
    fprintf(stderr, "%s: failed to execute %s: %s\n",
	    progname, argv[0], strerror(errno));
    goto err_exit;
  }
  /* wait until child closed */
  while ((caught = wait3(&status, 0, &ru)) != pid) {
    if (unlikely(caught == -1)) {
      fprintf(stderr, "%s: failed to wait for child %d: %s\n",
	      progname, pid, strerror(errno));
      goto err_exit;
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
      fprintf(stderr, "%s: could not open %s for writing, using standard output instead: %s\n",
	      progname, opts.sout_fpath, strerror(errno));
      sout = stdout;
    }
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
  fprintf(sout, "maximum resident set size:    %ld kb\n", ru.ru_maxrss);
  fprintf(sout, "soft page faults:             %ld\n",    ru.ru_minflt);
  fprintf(sout, "hard page faults:             %ld\n",    ru.ru_majflt);
  fprintf(sout, "fs input ops:                 %ld\n",    ru.ru_inblock);
  fprintf(sout, "fs output ops:                %ld\n",    ru.ru_oublock);
  fprintf(sout, "voluntary context switches:   %ld\n",    ru.ru_nvcsw);
  fprintf(sout, "involuntary context switches: %ld\n",    ru.ru_nivcsw);
  fprintf(sout, "command line:                ");
  for (i=0; i<argc; ++i)
    fprintf(sout, " %s", argv[i]);
  fprintf(sout, "\n");

  fflush(sout);
  if (sout != stdout)
    fclose(sout);
  exit(0);

 err_exit:
  fflush(stderr);
  exit(1);
}
