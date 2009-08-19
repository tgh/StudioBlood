#!/bin/sh
aclocal -I .
autoconf
automake --foreign -ac
./configure "$@"
