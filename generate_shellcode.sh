#!/bin/sh

file=include/g_shellcode.h
tmpfile=${TMPDIR-/tmp}/lxsh_shellcodegen
codedir=shellcode

# $1 = file
minimize() {
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

echo '#ifndef G_VERSION_H' > "$tmpfile"
echo '#define G_VERSION_H' >> "$tmpfile"
for I in "$codedir"/*.sh
do
  printf '#define %s "%s\\n"\n' "$(basename "$I" | tr [:lower:] [:upper:] | tr '.' '_')" "$(minimize "$I" | to_cstr)" >> "$tmpfile"
done
echo "#endif" >> "$tmpfile"

if [ "$(md5sum "$tmpfile" | cut -d' ' -f1)" != "$(md5sum "$file" | cut -d' ' -f1)" ] ; then
  mv "$tmpfile" "$file"
else
  rm "$tmpfile"
fi
