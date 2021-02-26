__lxsh_random_tmpfile() {
  echo "${TMPDIR-/tmp}/$1$(__lxsh_random_string 20)"
}
