#!/usr/bin/env bash

exec_name="main"

if ! [ -f "${exec_name}" ] || [ ".$1" == ".-f" ]; then
    ./build.sh || exit 1
fi

echo -e "==> Running executable    : \e[1;37m${exec_name}\e[0m"
"./${exec_name}"
