#!/usr/bin/env bash

exec_name="main"

echo '==> Cleaning up'
echo '  -> Removing object files'
find src/ -name '*.o' -delete
echo "  -> Removing ${exec_name}"
rm -f "${exec_name}"

if [ ".$1" == ".-f" ]; then
    echo "  -> Removing .build-cache"
    rm -rf .build-cache
    echo "  -> Removing build.log"
    rm -f build.log
fi
