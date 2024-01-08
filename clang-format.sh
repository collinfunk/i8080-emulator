#!/bin/sh

this_file=$0
this_dir=$(dirname "$this_file")

: "${CLANG_FORMAT=clang-format}"

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1 ; then
  echo "Set CLANG_FORMAT to the proper file path."
  exit 1
fi

"$CLANG_FORMAT" -style=file --verbose -i "$this_dir"/misc/*.c "$this_dir"/*.h \
                "$this_dir"/*.c

