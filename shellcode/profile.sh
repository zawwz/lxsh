{
  > lxshprofile.dat
  _lxsh_profile() {
    export TIMEFORMAT="%R;%U;%S;$(printf "%s" "$*" | tr '%\n' '_')"
    { time "$@" 2>/dev/null; } 2>> lxshprofile.dat
  }
}
