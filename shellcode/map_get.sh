__lxsh_map_get() {
  if [ "$2" = \* ] || [ "$2" = @ ] ; then
    printf "%s\n" "$1" | sort | cut -d ']' -f2-
  else
    printf "%s\n" "$1" | grep "^$2\]" | cut -d ']' -f2-
  fi
}
