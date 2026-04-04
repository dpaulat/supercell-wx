# Project Context

- **Owner:** Dan Paulat
- **Project:** supercell-wx
- **Product:** Cross-platform application for visualizing live and archived NEXRAD weather radar data
- **Stack:** C++20, Qt6, CMake, Conan 2, GoogleTest, Python build helpers
- **Created:** 2026-04-04

## Learnings

- Team initialized on 2026-04-04 under the Alien cast.
- Bishop owns wxdata, radar parsing, providers, and non-Qt background processing.
- Qt dependencies do not belong in the core data library.