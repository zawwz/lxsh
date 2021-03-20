_lxsh_random_tmpfile() {
  echo "${TMPDIR-/tmp}/$1$(_lxsh_random_string $2)"
}
