#!/bin/bash

bin=${1-./lxsh}

echo_red()
{
  printf "\033[1;31m%s\033[0m\n" "$*"
  exit 1
}

err=0

# $1 = file , $2 = extra print
# _LXSH_OPT : lxsh options
compile_test()
{
  printf "%s%s: " "$1" "$2"
  if errout=$($bin $_LXSH_OPT "$1" 2>&1 >/dev/null)
  then
    echo "Ok"
  else
    echo_red "Error"
    echo "$errout"
    return 1
  fi
}

# $1 = runner , $2 = file , $3 = extra print , $4 = runtime for lxsh
# _LXSH_OPT : lxsh options
exec_test()
{
  run=$1
  lxshrun=${4-$run}
  ret1=$($run "$2")
  stat1=$?
  ret2=$($bin $_LXSH_OPT "$2" | $lxshrun)
  stat2=$?
  printf "%s%s: " "$2" "$3"
  if [ "$ret1" = "$ret2" ] && [ $stat1 -eq $stat2 ]
  then echo "Ok"
  else
    echo_red "Error"
    echo ">> original stat $stat1
$ret1
>> compiled stat $stat2
$ret2"
    return 1
  fi
}

size_test()
{
  shebang=$(head -n1 "$1" | grep '^#!')
  c1=$($bin --no-shebang -m "$1" | wc -c)
  c2=$($bin -m "$1" | shfmt -mn | wc -c)
  printf "%s%s: " "$1" "$2"
  if [ $c1 -lt $c2 ]
  then echo "Ok"
  else
    echo_red "Too big"
    return 1
  fi
}

# $1 = file , $2 = extra print , $3 = list , $@ = run options
list_test()
{
  printf "%s%s: " "$1" "$2"
  file=$1
  varlist=$3
  shift 3
  diffout=$(diff <($bin "$file" "$@" | sort -k2) <(echo "$varlist" | sed '/^$/d' | sort -k2) )
  if [ -z "$diffout" ] ; then
    echo "Ok"
  else
    echo_red "Variable mismatch"
    echo "$diffout"
    return 1
  fi
}

resolve="test/include.sh test/resolve.sh"
exec_exclude="test/prompt.sh $resolve"


echo "
============
|    sh    |
============
"

echo "== Parse =="
for I in test/*.sh
do
  compile_test "$I" || err=$((err+1))
done

echo "== Exec =="
for I in $( echo test/*.sh $exec_exclude | tr -s ' \n' '\n' | sort | uniq -u )
do
  exec_test sh "$I" || err=$((err+1))
  _LXSH_OPT=-M exec_test sh "$I" " (minify)" || err=$((err+1))
done

echo "== Size =="
for I in test/*.sh
do
  size_test "$I" || err=$((err+1))
done

echo "== Resolve =="

for I in $resolve
do
  printf "%s: " "$I"
  if errmsg=$($bin "$I" | sh 2>&1 >/dev/null) && [ -z "$errmsg" ]
  then echo "Ok"
  else
    echo_red "Error"
    echo ">> stderr
$errmsg"
    err=$((err+1))
  fi
done

varlist="
2 nul
2 ABCD
1 AYE
1 BAR
3 FOO
2 TATA
1 TITI
4 TOTO
1 TUTU
4 somevar
"

vardefs="
1 ABCD
1 BAR
2 FOO
1 TATA
1 TOTO
1 TUTU
1 nul
2 somevar
"

varcalls="
1 AYE
1 ABCD
1 FOO
1 TATA
1 TITI
3 TOTO
1 nul
2 somevar
"

varlist_used="
1 AYE
2 ABCD
3 FOO
2 TATA
1 TITI
4 TOTO
2 nul
4 somevar
"

echo "== Variables =="
{
  list_test test/var.sh " (list)" "$varlist" --list-var || err=$((err+1))
  list_test test/var.sh " (list-def)" "$vardefs" --list-var-def || err=$((err+1))
  list_test test/var.sh " (list-call)" "$varcalls" --list-var-call || err=$((err+1))
  list_test test/var.sh " (remove unused)" "$varlist_used" --remove-unused --list-var || err=$((err+1))
}

fctlist="
1 toto
1 tata
"

fctlist_used="
1 toto
"

cmdlist="
2 echo
1 toto
"

echo "== Functions =="
{
  list_test test/fct.sh " (list-fct)" "$fctlist" --list-fct || err=$((err+1))
  list_test test/fct.sh " (list-cmd)" "$cmdlist" --list-cmd || err=$((err+1))
  list_test test/fct.sh " (remove unused)" "$fctlist_used" --remove-unused --list-fct || err=$((err+1))
}

echo "
============
|   bash   |
============
"

echo "== Parse =="
for I in test/*.bash
do
  compile_test "$I" || err=$((err+1))
done

echo "== Exec =="
for I in test/*.bash
do
  exec_test bash "$I" || err=$((err+1))
  _LXSH_OPT=-m exec_test bash "$I" " (minify)" || err=$((err+1))
done

echo "== Size =="
for I in test/*.bash
do
  size_test "$I" || err=$((err+1))
done

echo "== Debashify =="
for I in test/{debashify.bash,array.bash,echo.bash}
do
  _LXSH_OPT=--debashify exec_test bash "$I" "" sh || err=$((err+1))
  _LXSH_OPT="-m --debashify" exec_test bash "$I" " (minify)" sh || err=$((err+1))
done

exit $err
