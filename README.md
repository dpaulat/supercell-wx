# Supercell Wx

[![CI](https://github.com/dpaulat/supercell-wx/actions/workflows/ci.yml/badge.svg?branch=develop)](https://github.com/dpaulat/supercell-wx/actions/workflows/ci.yml)
[![Documentation Status](https://readthedocs.org/projects/supercell-wx/badge/?version=latest)](https://supercell-wx.readthedocs.io/en/latest/?badge=latest)
[![Discord](https://img.shields.io/badge/Discord-%235865F2.svg?style=flat&logo=discord&logoColor=white&labelColor=%235865f2)](https://discord.gg/vFMV76brwU)
[![GitHub Sponsor](https://img.shields.io/github/sponsors/dpaulat?label=Sponsor&logo=GitHub)](https://github.com/sponsors/dpaulat)

Supercell Wx is a free, open source application to visualize live and archive
NEXRAD Level 2 and Level 3 data, and severe weather alerts. It displays
continuously updating weather data on top of a responsive map, providing the
capability to monitor weather events using reflectivity, velocity, and other
products.

Please be sure to check out the documentation before getting started: [Supercell Wx Documentation](https://supercell-wx.rtfd.io/)

![image](https://supercell-wx.readthedocs.io/en/latest/_images/initial-setup-03-initial-configured-small.png)

## Supported Platforms

Supercell Wx supports the following 64-bit operating systems:

- Windows 10 (1809 or later)
- Windows 11
- Linux
  - Arch Linux (EndeavourOS, SteamOS [Steam Deck], and other Arch derivatives)
  - Fedora Linux 34+
  - openSUSE Tumbleweed
  - Ubuntu 22.04+
  - Most distributions supporting the GCC Standard C++ Library 11+
 
## Linux Dependencies

Supercell Wx requires the following Linux dependencies:

- Linux/X11 (Wayland works too) with support for GCC 11 and OpenGL 3.3
- X11/XCB libraries including xcb-cursor
 
## FAQ

Frequently asked questions:

- Q: Why is the map black when loading for the first time?
  
  - A. You must obtain a free API key from either (or both) [MapTiler](https://cloud.maptiler.com/auth/widget?next=https://cloud.maptiler.com/maps/) which currently does not require a credit/debit card, or [Mapbox](https://account.mapbox.com/) which ***does*** require a credit/debit card, but as of writing, you will receive 200K free requests per month, which should be sufficient for an individual user.

- Q: Why is it that when I change my color table, API key, grid width/height settings, nothing happens after hitting apply?

  - A. As of right now, you must restart Supercell Wx in order to apply these changes. In future iterations, this will no longer be an issue.
   
- Q. Is it possible to get dark mode?
 
  - A. In Windows, make sure to set the flag `-style fusion` at the end of the target path of the .exe
    - Example: `C:\Users\Administrator\Desktop\Supercell-Wx\bin\supercell-wx.exe -style fusion`
  - A. In Linux, if you're using KDE, Supercell Wx should automatically follow your theme settings.
