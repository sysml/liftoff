#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>

#define likely(x)   __builtin_expect(!(!(x)),1)
#define unlikely(x) __builtin_expect(!(!(x)),0)

int main(int argc, char *argv[])
{
  pid_t pid;
  sighandler_t irqsig, quitsig;
  struct timeval start, stop;
  int status;

  if (argc < 1)
    exit(1);
  if (argc == 1) {
    fprintf(stdout, "Usage: %s [PROGRAM] [[ARGUMENTS]]..\n", argv[0]);
    fflush(stderr);
    exit(1);
  }

  /* disable signal handling */
  irqsig = signal(SIGINT, SIG_IGN);
  quitsig = signal(SIGQUIT, SIG_IGN);

  gettimeofday(&start, (struct timezone *) 0);
  pid = fork();
  if (unlikely(pid < 0)) {
    fprintf(stderr, "%s: failed to fork: %s\n",
	    argv[0], strerror(errno));
    fflush(stderr);
    exit(1);
  }

  if (pid == 0) {
    /* child */
    execvp(argv[1], &argv[1]); /* does not return on success */
    fprintf(stderr, "%s: failed to execute %s: %s\n",
	    argv[0], argv[1], strerror(errno));
    fflush(stderr);
    exit(errno == ENOENT ? 127 : 126);    
  }

  if (waitpid(-1, &status, 0) == -1) {
    fprintf(stderr, "%s: failed to wait for child %d: %s\n",
	    argv[0], pid, strerror(errno));
    fflush(stderr);
    exit(1);
  }
  gettimeofday(&stop, (struct timezone *) 0);

  /* restore signals.  */
  signal(SIGINT, irqsig);
  signal(SIGQUIT, quitsig);

  fflush(stdout);

  exit(0);
}
