#!/bin/sh

grep "^ID=" /etc/os-release | cut -d '=' -f2-

echo toto | #
grep to

echo '#toto' | grep '#toto'
