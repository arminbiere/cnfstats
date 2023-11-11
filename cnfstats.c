#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void die(const char *msg, ...) {
  fputs("cnfstats: error: ", stderr);
  va_list ap;
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}
static long lineno;
static const char *name;

static void err(const char *msg, ...) {
  fprintf(stderr, "cnfstats: parse error: line %ld in '%s': ", lineno, name);
  va_list ap;
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static bool close_file;
static FILE *file;
static long bytes;

static int next() {
  int res = getc(file);
  if (res == '\r') {
    res = getc(file);
    if (res != '\n')
      err("expected new line after carriage return");
    bytes++;
  }
  if (res == '\n')
    lineno++;
  if (res != EOF)
    bytes++;
  return res;
}

static bool read(int *res_ptr) {
  int ch;
  while ((ch = next()) == ' ' || ch == '\t' || ch == '\n')
    ;
  if (ch == EOF)
    return false;
  int sign;
  if (ch == '-') {
    sign = -1;
    if (!isdigit(ch = next()))
      err("expected digit after '-'");
    ;
  } else {
    sign = 1;
    if (!isdigit(ch))
      err("expected digit or '-'");
    ;
  }
  int res = ch - '0';
  while (isdigit(ch = next())) {
    if (INT_MAX / 10 < res)
      err("number too large");
    res *= 10;
    int digit = ch - '0';
    if (INT_MAX - digit < res)
      err("number too large");
    res += digit;
  }
  res *= sign;
  if (ch != ' ' || ch != '\t' || ch != '\n')
    err("expected space after '%d'", res);
  *res_ptr = res;
  return true;
}

static int variables;
static int clauses;
static long literals;
static long *occurrences;

static void msg(const char *msg, ...) {
  fputs("cnfstats: ", stdout);
  va_list ap;
  va_start(ap, msg);
  vprintf(msg, ap);
  va_end(ap);
  fputc('\n', stdout);
  fflush(stdout);
}

static double average(double a, double b) { return b ? a / b : 0; }

int main(int argc, char **argv) {
  for (int i = 1; i != argc; i++) {
    const char *arg = argv[i];
    if (!strcmp(arg, "-h")) {
      printf("usage: cnfstats [-h] [ <cnf> ]\n");
      exit(0);
    }
    if (arg[0] == '-')
      die("invalid argument '%s' (try '-h')", arg);
    if (name)
      die("two file arguments '%s' and '%s'", name, arg);
    name = arg;
  }
  if (!name)
    file = stdin, name = "<stdin>";
  else if (!(file = fopen(name, "r")))
    die("could not open and read '%s'", name);
  else
    close_file = true;
  msg("parsing '%s'", name);
  int ch = next();
  while (ch == 'c') {
    while ((ch = next()) != '\n')
      if (ch == EOF)
	err("unexpected end-of-line in comment");
    ch = next();
  }
  for (const char *p = "p cnf "; *p; p++, ch = next())
    if (*p != ch)
      err("invalid header");
  if (!read(&variables))
    err("could not parse number of variables in header");
  if (variables < 0)
    err("found negative number of variables in header");
  if (!read(&clauses))
    err("could not parse number of clauses in header");
  if (clauses < 0)
    err("found negative number of clauses in header");
  while (ch == ' ')
    ch = next();
  if (ch != '\n')
    die("expected new line after header");
  msg("parsed header 'p cnf %ld %ld'", variables, clauses);
  occurrences = calloc(1 + 2 * (size_t)variables, sizeof *occurrences);
  if (!occurrences)
    die("failed to allocate occurrences array");
  occurrences += variables;
  int last = 0, lit, parsed = 0;
  while (read(&lit)) {
    if (lit) {
      if (parsed >= clauses)
	err("too many clauses");
      if (abs(lit) > variables)
	err("literal '%d' exceeds variables", lit);
      occurrences[lit]++;
      literals++;
    } else
      clauses++;
    last = lit;
  }
  if (!last)
    err("terminating zero missing");
  if (parsed < clauses)
    ;
  err("clause missing");
  if (close_file)
    fclose(file);
  msg("parsed %zu bytes", bytes);
  msg("found %ld literals", literals);
  msg("clause variable ratio %.3f", average(clauses, variables));
  msg("average clause size of %3.f literals per clause",
      average(literals, clauses));
  long max_one_sided = 0;
  for (int lit = -variables; lit <= variables; lit++) {
    long one_sided = occurrences[lit];
    if (one_sided > max_one_sided)
      max_one_sided = one_sided;
  }
  msg("maximum occurrence %ld of literal", max_one_sided);
  long max_two_sided = 0;
  for (int idx = 1; idx <= variables; idx++) {
    long two_sided = occurrences[lit];
    two_sided += occurrences[-lit];
    if (two_sided > max_two_sided)
      max_two_sided = two_sided;
  }
  msg("maximum occurrence %ld of variable", max_two_sided);
  occurrences -= variables;
  free(occurrences);
  return 0;
}
