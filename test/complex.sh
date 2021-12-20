while [ -z "$I" ]
do
case $U in
*)echo toto;I=y
esac
done
echo "$I"

case toto in
  tutu) ;;

  toto)
cat << EOF
toto
EOF
;;
  tata)
esac

echo to 2>&1
echo to2 >&1
echo to$(echo 2) >&1
