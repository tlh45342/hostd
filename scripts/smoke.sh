#!/usr/bin/env bash
set -euo pipefail
sock="${1:-/tmp/hostd.sock}"

./hostd -f -S "$sock" -l /tmp/hostd.log &
pid=$!
trap 'kill $pid 2>/dev/null || true' EXIT
sleep 0.2

echo "VERSION"    | ./vim-cmd -S "$sock" VERSION
echo "PING"       | ./vim-cmd -S "$sock" PING
./vim-cmd -S "$sock" "VM.CREATE name=demo mem=512"
./vim-cmd -S "$sock" VM.LIST
./vim-cmd -S "$sock" "VM.INFO id=1"
./vim-cmd -S "$sock" "VM.DESTROY id=1"
./vim-cmd -S "$sock" SHUTDOWN
