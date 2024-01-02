#!/bin/bash

set -e

echo "number of arguments: $#"

mkdir -p build
mkdir -p bin

if [ $# -eq 0 ]; then
    command="build"
else
    command="$1"
fi


function build() {
    gcc code/main.c -o bin/webspider -I code/based
}

function run() {
    ( cd www && ../bin/webspider )
}


case $command in
    build)
        build
        ;;

    run)
        run
        ;;

    *)
        echo "Could not recognize command '$command'"
        ;;
esac

