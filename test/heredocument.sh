#!/bin/sh

cat << EOF
toto
tata
EOF

toto=toto
cat << EOF | grep toto
$toto
tata
EOF

cat << EOF
'
EOF

cat << EOF |
azjeha
kijejaze
ljksdjk
EOF
cut -c1

grep -q toto << EOF &&
toto
EOF
echo found toto

{ cat << EOF | grep toto; }
toto
tata
EOF

( cat << EOF | grep toto )
toto
tata
EOF

{ cat << EOF | grep toto && echo true; echo eyy; }
toto
tata
EOF

( cat << EOF | grep toto && echo true ; echo eyy )
toto
tata
EOF
