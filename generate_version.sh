#!/bin/sh

SHA_FULL=$(git rev-parse HEAD)
IDIR=$(grep '^IDIR=' Makefile | cut -d '=' -f2-)
file="$IDIR/g_version.h"

OLD_SHA=$(grep 'VERSION_SHA' "$file" | cut -d '"' -f2)
OLD_SUFFIX=$(grep 'VERSION_SUFFIX' "$file" | cut -d '"' -f2)

[ "$RELEASE" != "true" ] && SUFFIX="-dev-$(echo "$SHA_FULL" | cut -c1-10)"


if [ "$OLD_SHA" != "$SHA_FULL" ] || [ "$OLD_SUFFIX" != "$SUFFIX"  ] ; then
  cat > "$file" << EOF
#ifndef G_VERSION_H
#define G_VERSION_H
#define VERSION_SUFFIX "$SUFFIX"
#define VERSION_SHA "$SHA_FULL"
#endif
EOF
fi

exit 0
