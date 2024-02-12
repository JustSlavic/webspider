#!/bin/bash

set -e

PROJECT=webspider
COMPILER=g++
STANDARD=c++17
COMPILE_COMMANDS_FILE=compile_commands.json

mkdir -p bin

if [ $# -eq 0 ]; then
    command="build"
else
    if [ "$1" = "debug" ]; then
        command="build"
        subcommand="debug"
    else
        command="$1"
        subcommand="$2"
    fi
fi

os_name=$(uname -s)

function build_() {
    $($1)
    if [ $? -eq 0 ]; then
        echo "[$1]... Success"
    fi
}

function build() {
    if [ "$command" = "pvs_analyze" ]; then
        COMPILE_DB_JSON="-MJ $COMPILE_COMMANDS_FILE"
    fi
    if [ "$subcommand" = "debug" ]; then
        # @todo: make way to remove this since I need only truncate function
        LIBS="-lm"
        DEBUG="-DDEBUG -g3"
        WARNINGS="-Wall"
    else
        LIBS="-lm"
        WARNINGS="-Wall -Werror"
    fi

    ./version.sh

    case $os_name in
        Darwin | Linux)
            build_cfg="$COMPILER code/meta/config.cpp -o bin/config -std=$STANDARD -I code/based $WARNINGS $DEBUG"
            build_ "$build_cfg"
            
            $(cd code; ../bin/config)

            build_webspider="$COMPILER code/webspider.cpp -o bin/$PROJECT -std=$STANDARD -I code/based $LIBS $WARNINGS $DEBUG $COMPILE_DB_JSON"
            build_ "$build_webspider"

            build_inspector="$COMPILER code/inspector.cpp -o bin/inspector -std=$STANDARD -I code/based $LIBS $WARNINGS $DEBUG"
            build_ "$build_inspector"

            build_fuzzer="$COMPILER code/kurl.cpp -o bin/kurl -std=$STANDARD -I code/based $WARNINGS $DEBUG"
            build_ "$build_fuzzer"

            ;;
        *)
            echo "Unrecognazied os name ($os_name)"
            ;;
    esac
}

function run() {
    ( cd www && ../bin/webspider )
}

function pvs_analyze() {
    build

    db_contents=$(cat $COMPILE_COMMANDS_FILE)
    db_contents_except_last_comma=${db_contents%?}
    echo "[$db_contents_except_last_comma]" > $COMPILE_COMMANDS_FILE

    pvs-studio-analyzer analyze -o pvs_output.log -j2
    plog-converter -a GA:1,2 -t json -o pvs_report.json pvs_output.log
    less pvs_report.json
}


function contains_in() {
    for it in $2; do
        if [ "$1" = "$it" ]; then
            return 0
        fi
    done
    return 1
}

if contains_in $command "build run pvs_analyze"; then
    "$command"
else
    echo "Could not recognize command '$command'"
fi

