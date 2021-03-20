_lxsh_map_set() {
  printf "%s\n" "$1" | grep -v "^$2\]"
  if [ -n "$3" ] ; then
    printf "%s]%s\n" "$2" "$3"
  fi
}
