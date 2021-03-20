#!/bin/sh

file=include/g_shellcode.h
tmpfile=${TMPDIR-/tmp}/lxsh_shellcodegen
codedir=shellcode

# $1 = file
minify() {
  if which lxsh >/dev/null 2>&1 ; then
    lxsh -m "$1"
  elif which shfmt >/dev/null 2>&1 ; then
    shfmt -mn "$1"
  else
    cat "$1"
  fi
}

to_cstr() {
  sed 's|\\|\\\\|g;s|\"|\\\"|g' | sed ':a;N;$!ba;s/\n/\\n/g;'
}


cat > "$tmpfile" << EOF
#ifndef G_VERSION_H
#define G_VERSION_H
EOF

unset all_fields
for I in "$codedir"/*.sh
do
  field=$(basename "$I" | tr [:lower:] [:upper:] | tr '.' '_')
  all_fields="$all_fields $field"
  printf '#define %s "%s\\n"\n' "$field" "$(minify "$I" | to_cstr)" >> "$tmpfile"
done


echo "#endif" >> "$tmpfile"

if [ "$(md5sum "$tmpfile" | cut -d' ' -f1)" != "$(md5sum "$file" | cut -d' ' -f1)" ] ; then
  mv "$tmpfile" "$file"
else
  rm "$tmpfile"
fi
