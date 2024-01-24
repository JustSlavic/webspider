#!/bin/bash

version=$(git describe --long --match "v*")
regex='v([0-9]+)\.([0-9]+)-([0-9]+)?-(.*)?'

[[ "$version" =~ $regex ]]

major="${BASH_REMATCH[1]}"
minor="${BASH_REMATCH[2]}"
patch="${BASH_REMATCH[3]}"
hash="${BASH_REMATCH[4]}"

echo "v$major.$minor.$patch-$hash"

cat > code/version.c <<- EOM
#include "version.h"

const char *version = "$major.$minor.$patch-$hash";

EOM
