#!/bin/bash
# Фильтр core_pattern: сохраняет корку только для указанного PID.
# Вызывается ядром: argv[1]=crashing_pid(%p) argv[2]=signal(%s) argv[3]=time(%t) argv[4]=target_pid
# Корка приходит на stdin.
if [ "$1" == "$4" ]; then
    cat > "/tmp/core.$1.$3"
fi
