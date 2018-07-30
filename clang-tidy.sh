#!/usr/bin/env bash
echo "==> Starting clang-tidy analyzer"

TIDY_FLAGS='-fix'
COMPILER_FLAGS='-Iinclude -D_PROJECT_VERSION="none"'
SOURCES="$(find src/ -type f -regex '.*[.]\(cpp\|C\|cc\|c\|cxx\|c[+][+]\)')"

clang-tidy ${TIDY_FLAGS} ${SOURCES} -- ${COMPILER_FLAGS}
