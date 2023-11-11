#!/bin/sh
die () {
  echo "test/run.sh: error: $*" 1>&2
  exit 1
}
run () {
  cnf=test/$1.cnf
  [ -f $cnf ] || die "could not find '$cnf'"
  echo "./cnfstats $cnf"
  log=test/$1.log
  err=test/$1.err
  ./cnfstats $cnf 1>$log 2>$err
  test $? = 0 || die "test './cnfstats $cnf' failed"
}
run empty
run false
run bin
