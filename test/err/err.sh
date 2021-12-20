#!/bin/sh

read var=a
var+=a
var=(foo)

$((var~2))

${!var}
${~}
${#var-a}
`echo \`echo\`  `

$(( (var) )

>& /dev/null
echo &> /dev/null

cat 2< file
cat <<< var
echo >

echo &| cat
echo |& cat

[[ a = b ]] foo

()

fct() abc
{ ; }

fct() { }

typeset var
var=val read var
case foo ; esac
case foo in aiae ; esac
case foo in ) ; esac
case foo in a) ; b) esac

for 2 in a ; do true ; done
for foo do ; do true ; done
for foo & ; do true ; done
for I in ; true ; done

while
do true ; done

while true ; do
done

if true ;then
fi

if
then true ; fi

if true ; then true ; else
fi

fct-foo() { true; }

function foo { true; }

{ foo; } bar
