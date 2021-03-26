_lxsh_random() {
  printf %d "0x$(head -c"${1-2}" </dev/urandom | od -A n -vt x1 | tr -d ' ')"
}
