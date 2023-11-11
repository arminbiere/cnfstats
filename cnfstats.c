#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
static void die (const char * msg, ...) {
  fputs ("cnfstats: error: ", stderr);
  va_list ap;
  va_start (ap, msg);
  vfprintf (stderr, msg, ap);
  va_end (ap);
  fputc ('\n', stderr);
  exit (1);
}
static const char * path;
static int close_file;
static FILE * file;
int main(int argc, char ** argv) {
  for (int i = 1; i != argc; i++) {
    const char * arg = argv[i];
    if (!strcmp (arg, "-h")) {
      printf ("usage: cnfstats [-h] [ <cnf> ]\n");
      exit (0);
    }
    if (arg[0] == '-')
      die ("invalid argument '%s' (try '-h')", arg);
    if (path)
      die ("expected at most one argument");
    path = arg;
  }
}
