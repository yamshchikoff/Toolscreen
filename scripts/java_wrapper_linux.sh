#!/bin/bash
export LD_PRELOAD=/home/user/Toolscreen/out/build/linux-test/bin/libtoolscreen.so
exec "$(dirname "$0")/java.real" "$@" 2>/home/user/toolscreen.log
