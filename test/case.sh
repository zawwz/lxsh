
echo "$*" | case $1 in
  to*) echo toto ;;
  tata) echo wew tata ;;
  a | b)
    echo titi
    ;;
  *)
    cat ;;
esac

case foo in bar)echo a;;foo)echo b;esac
case foo in bar)echo a;;foo)echo b
esac

case far in foo) echo a;;bar)
esac
