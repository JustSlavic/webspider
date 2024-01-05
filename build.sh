#!/bin/bash

set -e

mkdir -p build
mkdir -p bin

if [ $# -eq 0 ]; then
    command="build"
else
    command="$1"
fi
subcommand="$2"

os_name=$(uname -s)

function build() {
    if [ "$subcommand" = "debug" ]; then
        DEBUG="-DDEBUG -g3"
    else
        WARNINGS="-Wall -Werror"
    fi

    case $os_name in
        Darwin | Linux)
            gcc code/main_webspider.c -o bin/webspider -I code/based $WARNINGS $DEBUG
            echo "[gcc code/main_webspider.c -o bin/webspider -I code/based $DEBUG $WARNINGS]... Success"
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

