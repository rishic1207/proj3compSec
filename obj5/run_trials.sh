#!/bin/bash
# Launch N concurrent fuzzing trials (default 10), each in its own folder and log.
# Usage: ./run_trials.sh [N]
N=${1:-10}
if pgrep -u "$(id -un)" -x fuzzer > /dev/null; then
    echo "A fuzzer is already running; stop it first (pkill -u $(id -un) -x fuzzer)." >&2
    exit 1
fi
echo "Launching $N trials on $(hostname)"
for i in $(seq 1 "$N"); do
    mkdir -p "trial$i"
    cp fuzzer targetCovLoop "trial$i/"
    rm -f "trial$i/crash.bin"
    (cd "trial$i" && nohup ./fuzzer ./targetCovLoop > "trial$i.log" 2>&1 < /dev/null &)
    sleep 1   # different start second -> different default RNG seed
done    