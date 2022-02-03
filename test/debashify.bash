#!/bin/bash

readonly tutu titi=tata
echo "$tutu $titi"

diff <(echo a) <(echo b)

write_to_file() { echo "$2" > "$1"; }
write_to_file >(grep tutu) tutu
wait $!

echo a &> /tmp/toto
echo b >& /tmp/tata
echo c &>> /tmp/toto

cat /tmp/tata /tmp/toto
rm /tmp/tata /tmp/toto


TOTO="ta
to"
grep ta <<< toto$TOTO

TATA=ti
TATA+=tu
echo $TATA

[[ $DEBUG == true ]] && echo debug

[ $((RANDOM+RANDOM)) -gt 0 ]
echo randomstat: $?

a=a
[[ $a = a && foo = fo* && bar =~ b.r || 2 < 3 ]]
echo $?

N=1
TOTO=tatitu
echo "${TOTO:2}"
echo "${TOTO:$N:2}"

echo ${TOTO:-tutu}
echo ${TITI:-bar}

TATA=TOTO
echo ${!TATA}
