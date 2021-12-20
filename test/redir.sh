{ echo toto >&2; } 2>&1

{ echo tata; }>/dev/null

grep abc < test/redir.sh
