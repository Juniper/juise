#!/bin/bash

mkdir -p ~/.ipython/kernels/clira
START_SCRIPT_PATH=$(cd `dirname "${BASH_SOURCE[0]}"` && pwd)/src/clira.py
PYTHON_PATH=$(which python)
CONTENT='{
   "argv": ["'${PYTHON_PATH}'", "'${START_SCRIPT_PATH}'", "{connection_file}"],
                "display_name": "clira",
                "language": "clira"
}'
echo $CONTENT > ~/.ipython/kernels/clira/kernel.json
