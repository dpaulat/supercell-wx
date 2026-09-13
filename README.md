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
  - Fedora Linux 39+
  - openSUSE Tumbleweed
  - Ubuntu 24.04+
  - NixOS 25.05+
  - Most distributions supporting the GCC Standard C++ Library 13+
- macOS
  - 15.0+ for Intel-based Macs
  - 14.0+ for Apple silicon-based Macs

## Linux Dependencies

Supercell Wx requires the following Linux dependencies:

- Linux with support for GCC 13 and Vulkan 1.3
- If using X11, XCB libraries including xcb-cursor

## FAQ

Frequently asked questions:

- Q: Why is the map black when loading for the first time?

  - A. You must obtain a free API key from either (or both) [MapTiler](https://cloud.maptiler.com/auth/widget?next=https://cloud.maptiler.com/maps/) which currently does not require a credit/debit card, or [Mapbox](https://account.mapbox.com/) which ***does*** require a credit/debit card, but as of writing, you will receive 200K free requests per month, which should be sufficient for an individual user.

- Q: Why is it that when I change my grid width/height settings, nothing happens after hitting apply?

  - A. You must restart Supercell Wx in order to apply these changes. Each version reduces the number of settings requiring a restart.

- Q: How can I contribute?
  - A. Head to [Developer Setup](https://supercell-wx.readthedocs.io/en/stable/development/developer-setup.html) and [Contributing](CONTRIBUTING.md) to configure the Supercell Wx development environment for your IDE. Currently Visual Studio and Visual Studio Code are recommended, with other IDEs remaining untested at this time.

## Sponsors

<table>
  <tr>
    <td><a href="https://signpath.io/"><img src="https://signpath.org/assets/favicon-50x50.png" alt="SignPath" width="32" /></a></td>
    <td>Free code signing on Windows provided by <a href="https://signpath.io/">SignPath.io</a>, certificate by <a href="https://signpath.org/">SignPath Foundation</a></td>
  </tr>
</table>
