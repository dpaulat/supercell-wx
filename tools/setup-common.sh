#!/bin/bash
pip install --upgrade --user "conan<2.0"
pip install --upgrade --user geopandas
pip install --upgrade --user GitPython
conan profile new default --detect
