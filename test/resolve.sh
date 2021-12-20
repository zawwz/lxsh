#!/bin/sh

echo "a$(%resolve echo tata titi)b"

echo $(%resolve cut -d ' ' -f1 /proc/uptime)s

%resolve echo "TIME=$(cut -d ' ' -f1 /proc/uptime)s;echo This was compiled at \${TIME}s uptime"

FOO=$(%resolve echo bar) echo foo
