#!/bin/sh
set -eu
cd "$(dirname "$0")"
make FREQUENCY="${1:-915}"
