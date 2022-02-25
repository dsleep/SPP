#!/bin/bash
# My example bash script

python3 -m ensurepip --default-pip
python3 -m pip install --upgrade pip
python3 -m pip install requests-aws
python3 -m pip install requests
python3 -m pip install py7zr
python3 ./PythonUtils/Validate3rdParty.py
