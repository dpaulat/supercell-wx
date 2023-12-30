#!/bin/bash
pip install --upgrade --user --break-system-packages "conan<2.0"
pip install --upgrade --user --break-system-packages geopandas
pip install --upgrade --user --break-system-packages GitPython
conan profile new default --detect
