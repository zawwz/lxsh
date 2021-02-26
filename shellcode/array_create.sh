__lxsh_array_create() {
  printf "%s" "$1"
  shift 1
  for N ; do
    printf "\t%s" "$N"
  done
}
