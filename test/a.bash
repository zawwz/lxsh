#!/usr/bin/env bash

diff <(echo a) <(echo b)

write_to_file() { echo "$2" > "$1"; }
write_to_file >(grep bar) bar
wait $!

echo a &> /tmp/foo
echo b >& /tmp/bar
echo c &>> /tmp/foo

cat /tmp/bar /tmp/foo
rm /tmp/bar /tmp/foo


TOTO="foo
bar"
grep ar <<< ar$TOTO

declare -a A
A=("fo o" bar)
echo ${A[1]}

declare -A B
B[foo]=ta
B[bar]=tu
echo ${B[foo]}
echo ${B[bar]}
echo ${B[*]}

C=([foo]=bar [bar]=foo)
echo ${C[foo]}
echo ${C[bar]}
echo ${C[*]}

BAR=FOO
echo ${!BAR}

[[ $DEBUG == true ]] && echo debug

a=a
[[ $a = a && foo = fo* && bar =~ b.r || 2 < 3 ]]

for I in A B C ; do
  echo "$I"
done > >(cat)
