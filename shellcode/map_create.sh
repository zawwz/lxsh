__lxsh_map_create() {
  for I
  do
    printf "%s]%s\n" "$(echo "$I" | cut -d']' -f1 | cut -d '[' -f2)" "$(echo "$I" | cut -d '=' -f2-)"
  done
}
