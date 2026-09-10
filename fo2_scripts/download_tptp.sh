#!/bin/bash

# Script to download and extract the TPTP v8.2.0 benchmark library
# Usage: ./scripts/download_tptp.sh [TARGET_DIR]

TARGET_DIR="${1:-tests/tptp_raw}"
TPTP_URL="https://tptp.org/TPTP/Archive/TPTP-v8.2.0.tgz"
ARCHIVE_NAME="TPTP-v8.2.0.tgz"

echo "=================================================="
echo "TPTP v8.2.0 Benchmark Downloader"
echo "Target directory: $TARGET_DIR"
echo "=================================================="

mkdir -p "$TARGET_DIR"

if [ ! -f "$TARGET_DIR/$ARCHIVE_NAME" ]; then
    echo "Downloading TPTP v8.2.0 archive from $TPTP_URL ..."
    curl -L "$TPTP_URL" -o "$TARGET_DIR/$ARCHIVE_NAME" || wget "$TPTP_URL" -O "$TARGET_DIR/$ARCHIVE_NAME"
    if [ $? -ne 0 ]; then
        echo "Error: Failed to download TPTP v8.2.0 archive."
        exit 1
    fi
else
    echo "Archive $ARCHIVE_NAME already exists in $TARGET_DIR. Skipping download."
fi

echo "Extracting Problems directory from archive..."
tar -xzf "$TARGET_DIR/$ARCHIVE_NAME" -C "$TARGET_DIR" --strip-components=1 "TPTP-v8.2.0/Problems"

if [ $? -eq 0 ]; then
    echo "TPTP v8.2.0 Problems extracted successfully into $TARGET_DIR/Problems!"
else
    echo "Error: Extraction failed."
    exit 1
fi
