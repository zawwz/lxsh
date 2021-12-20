TOTO=toto
(TOTO=tata; echo $TOTO; echo a) | sed 's|a|titi|g'
echo $TOTO

echo a | ( grep a && echo b )

echo ab | ( grep a )
pwd
(cd /)
pwd
