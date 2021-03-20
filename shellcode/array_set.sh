_lxsh_array_set()
{
  [ "$2" -gt 0 ] && printf "%s\t" "$(printf "%s" "$1" | cut -f1-$2)"
  printf "%s" "$3"
  [ "$2" -lt $(printf "%s" "$1" | tr -dc '\t' | wc -c) ] && { printf "\t" ; printf "%s" "$1"|cut -f$(($2+2))-; }
}
