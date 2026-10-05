#!/bin/bash

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PARENT_DIR="$(dirname "$SCRIPT_DIR")"
if [ -f "$PARENT_DIR/FOF_EQ_3MODE.sh" ]; then
    exec "$PARENT_DIR/FOF_EQ_3MODE.sh" "$@"
elif [ -f "$SCRIPT_DIR/FOF_EQ_3MODE.sh" ]; then
    exec bash "$SCRIPT_DIR/FOF_EQ_3MODE.sh" "$@"
fi
