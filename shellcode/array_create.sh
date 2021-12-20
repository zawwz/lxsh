_lxsh_array_create() {
  printf "%s" "$1"
  shift 1 2>/dev/null || return
  for N ; do
    printf "\t%s" "$N"
  done
}
