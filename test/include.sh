#!/bin/sh

RAW="$(%include -f pipe.sh brace.sh)"

echo "$RAW"

{ %include -f pipe.sh; } | grep -q arch && echo "btw i use arch"

%include *.sh
