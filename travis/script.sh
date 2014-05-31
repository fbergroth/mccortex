#!/bin/sh

set -e

RUN_TRAVIS=`./travis/run.sh`

if [ "$RUN_TRAVIS" == "yes" ]
then
  if [ "${TRAVIS_BRANCH}" == 'coverity_scan' ]
  then
    ./travis/travisci_build_coverity_scan.sh
  else
    make all RELEASE=1 && make clean && make all && make test
  fi
fi
