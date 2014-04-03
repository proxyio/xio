#!/bin/bash

if [ $# -eq 0 ]; then
    exit 1
fi

sys=sys_$1

platform_code() {
    sysh=$sys
    if [[ $1 != "" ]]; then
	sysh=$sys"_"$1
    fi

    if [ ! -f $sysh.h ]; then
	touch $sysh.h
	echo -e "#ifndef _HPIO_"$sysh"_" >> $sysh.h
	echo -e "#define _HPIO_"$sysh"_" >> $sysh.h
	echo -e "#endif" >> $sysh.h
	touch $sysh.c
	echo -e "#include \""$sysh".h\"" >> $sysh.c
    fi
}

nr=""
win="win"
unix="unix"
platform_code $nr
platform_code $win
platform_code $unix