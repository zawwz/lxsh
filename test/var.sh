#!/bin/sh

foo()
{
  echo $FOO
}

TOTO=tata
TATA=titi
echo $TOTO
echo "$TOTO  $TATA$TITI"
echo "${AYE-aye}"

export TUTU=ta

foo
FOO=bar foo
BAR=foo foo
ABCD=$(FOO=true foo)
echo $ABCD
nul=/dev/null
echo toto > "$nul"

somevar=val
echo $somevar
unset somevar
echo $somevar

cat << EOF
$TOTO
EOF
