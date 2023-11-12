#include <assert.h>
#include <ctype.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void die(const char *, ...) __attribute__((format(printf, 1, 2)));

static void die(const char *msg, ...) {
  fputs("cnfstats: error: ", stderr);
  va_list ap;
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static long lineno = 0;
static const char *name;

static void err(const char *, ...) __attribute__((format(printf, 1, 2)));

static void err(const char *msg, ...) {
  fprintf(stderr, "cnfstats: parse error: at line %ld in '%s': ", lineno + 1,
	  name);
  va_list ap;
  va_start(ap, msg);
  vfprintf(stderr, msg, ap);
  va_end(ap);
  fputc('\n', stderr);
  exit(1);
}

static FILE *file;
static long bytes;

static int next_char() {
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

static bool next_int(int *res_ptr) {
  int ch;
  while ((ch = next_char()) == ' ' || ch == '\t' || ch == '\n')
    ;
  if (ch == EOF)
    return false;
  int sign;
  if (ch == '-') {
    sign = -1;
    if (!isdigit(ch = next_char()))
      err("expected digit after '-'");
    ;
  } else {
    sign = 1;
    if (!isdigit(ch))
      err("expected digit or '-'");
    ;
  }
  int res = ch - '0';
  while (isdigit(ch = next_char())) {
    if (INT_MAX / 10 < res)
      err("number too large");
    res *= 10;
    int digit = ch - '0';
    if (INT_MAX - digit < res)
      err("number too large");
    res += digit;
  }
  res *= sign;
  if (ch != EOF && ch != ' ' && ch != '\t' && ch != '\n')
    err("unexpected character 0x%02x after '%d'", ch, res);
  *res_ptr = res;
  return true;
}

static int variables;
static int clauses;
static int counts[10];
static int largest_size = -1;
static int smallest_size = -1;
static int log10sizes[11];
static long literals;

static long *occurrences;
static long *sizes;

// clang-format off

static int intlog10(int n) {
  if (n <= 1) return 0;
  if (n <= 10) return 1;
  if (n <= 100) return 2;
  if (n <= 1000) return 3;
  if (n <= 10000) return 4;
  if (n <= 100000) return 5;
  if (n <= 1000000) return 6;
  if (n <= 10000000) return 7;
  if (n <= 100000000) return 8;
  if (n <= 1000000000) return 9;
  return 10;
}

static int intexp10(int n) {
  switch (n) {
    case 0: return 1;
    case 1: return 10;
    case 2: return 100;
    case 3: return 1000;
    case 4: return 10000;
    case 5: return 100000;
    case 6: return 1000000;
    case 7: return 10000000;
    case 8: return 100000000;
    case 9: return 1000000000;
    default: return INT_MAX;
  }
}

// clang-format on

static void parse(void) {
  int lit = 0, parsed = 0, size = 0;
  while (next_int(&lit)) {
    if (lit) {
      if (parsed >= clauses)
	err("too many clauses");
      if (abs(lit) > variables)
	err("literal '%d' exceeds variables", lit);
      occurrences[lit]++;
      literals++;
      size++;
    } else {
      sizes[parsed++] = size;
      if (size < 10)
	counts[size]++;
      if (smallest_size < 0 || size < smallest_size)
	smallest_size = size;
      if (largest_size < 0 || size > largest_size)
	largest_size = size;
      log10sizes[intlog10(size)]++;
      size = 0;
    }
  }
  if (lit)
    err("terminating zero missing");
  if (parsed < clauses)
    err("clause missing");
}

static void msg(const char *, ...) __attribute__((format(printf, 1, 2)));

static void msg(const char *msg, ...) {
  fputs("cnfstats ", stdout);
  va_list ap;
  va_start(ap, msg);
  vprintf(msg, ap);
  va_end(ap);
  fputc('\n', stdout);
  fflush(stdout);
}

static FILE *open_pipe(const char *fmt, const char *path) {
  char *cmd = malloc(strlen(fmt) + strlen(path));
  sprintf(cmd, fmt, path);
  FILE *res = popen(cmd, "r");
  free(cmd);
  return res;
}

static double average(double a, double b) { return b ? a / b : 0; }

static double percent(double a, double b) { return average(100 * a, b); }

static bool match(const char *str, char *suffix) {
  size_t l = strlen(str), k = strlen(suffix);
  return k <= l && !strcmp(str + l - k, suffix);
}

static int qsort_cmp_size(const void *p, const void *q) {
  return *(long *)p - *(long *)q;
}

int main(int argc, char **argv) {
  msg("version 0.0.1");
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
  bool close_file = false;
  bool pclose_file = false;
  if (!name)
    file = stdin, name = "<stdin>";
  else if (match(name, ".xz"))
    file = open_pipe("xz -c -d %s", name), pclose_file = true;
  else if (match(name, ".gz"))
    file = open_pipe("gunzip -c -d %s", name), pclose_file = true;
  else if (match(name, ".bz2"))
    file = open_pipe("bunzip2 -c -d %s", name), pclose_file = true;
  else
    file = fopen(name, "r"), close_file = true;
  if (!file)
    die("could not open and read '%s'", name);
  msg("cnf %s", name);
  int ch = next_char();
  while (ch == 'c') {
    while ((ch = next_char()) != '\n')
      if (ch == EOF)
	err("unexpected end-of-line in comment");
    ch = next_char();
  }
  if (ch != 'p' || next_char() != ' ' || next_char() != 'c' ||
      next_char() != 'n' || next_char() != 'f' || next_char() != ' ')
    err("invalid header");
  if (!next_int(&variables))
    err("could not parse number of variables in header");
  if (variables < 0)
    err("found negative number of variables in header");
  msg("variables %d", variables);
  if (!next_int(&clauses))
    err("could not parse number of clauses in header");
  if (clauses < 0)
    err("found negative number of clauses in header");
  msg("clauses %d", clauses);
  msg("clause-variable-ratio %.3f", average(clauses, variables));
  occurrences = calloc(1 + 2 * (size_t)variables, sizeof *occurrences);
  if (!occurrences)
    die("failed to allocate occurrences array");
  sizes = malloc(clauses * sizeof *sizes);
  occurrences += variables;
  parse();
  if (close_file)
    fclose(file);
  if (pclose_file)
    pclose(file);
  msg("lines %ld", lineno);
  msg("bytes %zu", bytes);
  msg("literals %zu", literals);
  long max_one_sided = 0;
  for (int lit = -variables; lit <= variables; lit++) {
    long one_sided = occurrences[lit];
    if (one_sided > max_one_sided)
      max_one_sided = one_sided;
  }
  msg("maximum-literal-occurrence %ld", max_one_sided);
  long max_two_sided = 0;
  for (int idx = 1; idx <= variables; idx++) {
    long two_sided = occurrences[idx];
    two_sided += occurrences[-idx];
    if (two_sided > max_two_sided)
      max_two_sided = two_sided;
  }
  msg("maximum-variable-occurrence %ld", max_two_sided);
  occurrences -= variables;
  free(occurrences);
  if (smallest_size > 0)
    msg("smallest-size %d", smallest_size);
  if (clauses) {
    qsort(sizes, clauses, sizeof *sizes, qsort_cmp_size);
    if (clauses & 1)
      msg("median-size %ld", sizes[clauses / 2]);
    else {
      long lower = sizes[clauses/2];
      long upper = sizes[clauses/2 + 1];
      if (lower == upper)
	msg("median-size %ld", lower);
      else {
	double median_size = (lower + upper)/2;
	msg("median-size %.3f", median_size);
      }
    }
    double average_size = average(literals, clauses);
    msg("average-size %.3f", average_size);
  }
  free(sizes);
  if (largest_size > 0)
    msg("largest-size %d", largest_size);
  for (int i = 0; i != 10; i++)
    if (counts[i])
      msg("sizes[%d] %d %.2f%%", i, counts[i], percent(counts[i], clauses));
  for (int i = 1; i != 11; i++)
    if (log10sizes[i])
      msg("log10[%d] %d %.2f%% [%d..%d]", i, log10sizes[i],
	  percent(log10sizes[i], clauses), intexp10(i - 1) + 1, intexp10(i));
  return 0;
}
