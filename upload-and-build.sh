#!/bin/bash

#PIDFILE="./pio-upload-monitor.pid"
#
#if [ -f "$PIDFILE" ]; then
#    OLD_PID=$(cat "$PIDFILE")
#    if kill -0 "$OLD_PID" 2>/dev/null; then
#        echo "Stopping previous instance ($OLD_PID)..."
#        kill "$OLD_PID"
#        sleep 1
#    fi
#fi
#
#echo $$ > "$PIDFILE"
#
#cleanup() {
#    rm -f "$PIDFILE"
#}
#
#trap cleanup EXIT

PIO="/home/xav/.local/bin/pio"

if [ ! -f "platformio.ini" ]; then
    echo "platformio.ini not found"
    exit 1
fi

# Try default_envs first
ENV=$(grep -E '^[[:space:]]*default_envs[[:space:]]*=' platformio.ini | sed 's/.*=//' | xargs)

# If no default_envs, use the first [env:...] section
if [ -z "$ENV" ]; then
    ENV=$(grep -E '^\[env:' platformio.ini | head -n1 | sed -E 's/^\[env:([^]]+)\].*/\1/')
fi

if [ -z "$ENV" ]; then
    echo "No PlatformIO environment found"
    exit 1
fi

echo "Using PlatformIO environment: $ENV"

"$PIO" run -t upload -e "$ENV" && exec "$PIO" device monitor -e "$ENV"