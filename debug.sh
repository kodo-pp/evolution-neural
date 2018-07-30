#!/usr/bin/env bash
case $1 in
gdb)
    echo "==> Starting GDB debugger"
    gdb ./main
    ;;
cgdb)
    echo "==> Starting CGDB debugger"
    cgdb -- ./main
    ;;
lldb)
    echo "==> Starting LLDB debugger"
    lldb ./main
esac
