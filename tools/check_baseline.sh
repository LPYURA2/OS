#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
python matryoshka.py --selftest
python tools/export_images.py --check
