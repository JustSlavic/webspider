#!/bin/bash

set -e

mkdir -p build
mkdir -p bin

if [ $# -eq 0 ]; then
    command="build"
else
    command="$1"
fi

os_name=$(uname -s)

function build() {
    case $os_name in
        Darwin)
            gcc code/main_macos.c -o bin/webspider -I code/based -Wall
            ;;
        *)
            echo "Unrecognazied os name ($os_name)"
            ;;
    esac
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

