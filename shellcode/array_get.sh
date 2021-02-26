__lxsh_array_get() {
  if [ "$2" = "*" ] || [ "$2" = "@" ] ; then
    printf "%s" "$1" | tr '\t' ' '
  else
    printf "%s" "$1" | cut -f$(($2+1))
  fi
}
