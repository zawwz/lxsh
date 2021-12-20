#!/bin/sh

{ echo tata ; echo a; } | sed 's|a|toto|g'

echo a | { grep a && echo b; }
