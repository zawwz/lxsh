_lxsh_random_string() {
  env LC_CTYPE=C tr -dc 'a-zA-Z0-9' </dev/urandom | head -c "${1-20}"
}
