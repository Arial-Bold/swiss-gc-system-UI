# Swiss System UI

An unofficial fork of [Swiss](https://github.com/emukidid/swiss-gc) that restyles its screens in the GameCube "System" look. Only the interface changes. Games, programs, devices and patching all work exactly as they do in Swiss.

**Version 1.1 is based on Swiss 0.7.0.**

> This fork isn't made or supported by the Swiss developers. Please report UI problems here rather than to the Swiss project.

### What's different
- **The System look:** a TV-screen backdrop, teal-black panels, frosted setting rows with alternating stripes, glass buttons and the gold selection ring, across Swiss's own screens. The original Swiss logo is kept.
- **Widescreen:** a 16:9 setting that lays Swiss out wider so nothing looks stretched on widescreen TVs.
- **Picture Width:** Full (704) to fill the TV like games do, or Standard (640) for TVs and scalers that cut off the edges.
- **Overscan:** pulls the screens in from the edges for TVs that hide part of the picture.
- **Resolution:** pick the video mode for Swiss's own screens, or leave it on Auto.
- **Menu Music and Menu Sounds:** an original, mellow tune and soft menu sounds, both generated on the console. Both stop before a game or program starts, and the music waits while the MP3 player is open.

All of these are under **Settings → Interface Settings**. They only affect Swiss's screens, never games.

### What's new in 1.1
- Merged up to Swiss 0.7.0, which brings System Date, System Time and time zone settings, a new SMB library, smaller builds, and updated game databases. See the [Swiss 0.7.0 release](https://github.com/emukidid/swiss-gc/releases/tag/v0.7.0) for the full list.
- The new System Date and System Time rows are restyled to match the rest of the System look.

### Installing
Download the latest release from this repository's **Releases** page and install it the same way as Swiss (see [Usage](#usage) below). Your existing Swiss settings carry over. The fork's own settings start at their defaults.

### Building
Same as Swiss: push to GitHub and the included workflow builds every release package, or build locally with devkitPPC and libogc2. The version shown in the title bar is set in `cube/swiss/Makefile` (`SWISS_BASE_VERSION`), because GitHub forks don't receive Swiss's newer release tags.

### Credits
Swiss is by emu_kidid, Extrems and its contributors. The System UI restyle is a fan project, and the menu music and sounds are original compositions.

---

*The original Swiss README follows.*

# Swiss

[![Build Status](https://github.com/emukidid/swiss-gc/actions/workflows/continuous-integration-workflow.yml/badge.svg)](https://github.com/emukidid/swiss-gc/actions/workflows/continuous-integration-workflow.yml)

## Table of Contents
- [Purpose](#purpose)
	- [Main Features](#main-features)
	- [Requirements](#requirements)
	- [Usage](#usage)
- [Navigating Swiss](#navigating-swiss)
	- [Controls](#controls)
	- [Swiss UI](#swiss-ui)

## Purpose
Swiss aims to be an all-in-one homebrew utility for the Nintendo GameCube.

### Main Features
**Can browse the following devices**
- SDSC/SDHC/SDXC Cards via [SD Gecko](https://www.gc-forever.com/wiki/index.php?title=SDGecko), [SD2SP1](https://github.com/silverstee1/SD2SP1) or [SD2SP2](https://github.com/Extrems/SD2SP2)
- DVD±R or original Game Discs via Optical Disc Drive
- [Qoob Pro](https://www.gc-forever.com/wiki/index.php?title=Qoob) flash memory
- [USB Gecko](https://www.gc-forever.com/wiki/index.php?title=USBGecko) remote file storage
- [WASP](https://www.gc-forever.com/wiki/index.php?title=WASP_Fusion) / [Wiikey Fusion](https://www.gc-forever.com/wiki/index.php?title=Wiikey_Fusion)
- SMB, FTP, FSP via [Broadband Adapter](https://www.gc-forever.com/wiki/index.php?title=Broadband_Adapter), ENC28J60, W5500, W6100 or W6300
- [WODE Jukebox](https://www.gc-forever.com/wiki/index.php?title=Wii_Optical_Drive_Emulator)
- [IDE-EXI](https://www.gc-forever.com/wiki/index.php?title=IDE-EXI), M.2 Loader or USB Dolphin
- Memory Cards
- [GC Loader](https://www.gc-forever.com/wiki/index.php?title=GCLoader) or CUBEODE
- [FlippyDrive](https://www.gc-forever.com/wiki/index.php?title=FlippyDrive)
- [KunaiGC](https://github.com/KunaiGC/KunaiGC) flash memory
- [FlipperMCE](https://flippermce.github.io/), GCMCE or MemCard PRO GC

Note: Most devices and the exFAT filesystem are not supported by libogc and only by [libogc2](https://github.com/extremscorner/libogc2).

**Can emulate the following devices**
- Processor Interface
- DVD Interface
	- Optical Disc Drive
- External Expansion Interface
	- Broadband Adapter via [ENC28J60](https://www.microchip.com/en-us/product/enc28j60) (ETH2GC, GCNET and SP1ETH), [W5500](https://wiznet.io/products/ethernet-chips/w5500) (ETH2SP1), [W6100](https://wiznet.io/products/ethernet-chips/w6100) or [W6300](https://wiznet.io/products/ethernet-chips/w6300)
	- Memory Cards via SD Cards
- Audio Streaming Interface

Note: Emulation is only available for the Dolphin SDK. Homebrew requires native drivers as provided by libogc2.

**Can provide the following services**
- Game ID for BlueRetro, FlipperMCE, MemCard PRO GC and PixelFX RetroGEM GC
- Profile selection for RetroTINK-4K using [ser2net](https://github.com/cminyard/ser2net)
- Return to loader and environment setup for [libogc2](https://github.com/extremscorner/libogc2) applications
- Return to loader for libogc v3.2.0+ applications
- Return to loader for older applications using a legacy mechanism
- Timekeeping using SNTP and daylight saving time data
- [wiiload](https://wiibrew.org/wiki/Wiiload) v0.5 over TCP/IP or USB Gecko

### Requirements
- GameCube with controller
- A [way to boot homebrew](https://www.gc-forever.com/wiki/index.php?title=Booting_homebrew)

### Usage
1. [Download latest Swiss release](https://github.com/emukidid/swiss-gc/releases/latest) and extract its contents.
2. Copy the Swiss DOL file found in the DOL folder to the device/medium you are using to boot homebrew.
3. Launch Swiss, browse your device and load a DOL or GCM!

Note: Specific devices will have specific locations/executable file variants that need to be used, please check the documentation with those devices on where Swiss will need to be placed.

## Navigating Swiss
### Controls
| Control                       | Action                  |
| ----------------------------- | ----------------------- |
| Control Stick or +Control Pad | Navigate through the UI |
| A Button                      | Select                  |
| B Button                      | Enter/Exit bottom pane  |
| X Button                      | Move back up a folder   |
| Z Button                      | Manage file or folder   |
| L Button                      | Move up a page          |
| R Button                      | Move down a page        |
| Start/Pause                   | Access recent list      |

### Swiss UI
- The top heading shows the version number, commit hash, and revision number of Swiss.
- The left panes show what device you are using.
- The largest portion is the Swiss file browser, through which you can navigate files and folders. The top of every folder includes a `..` option, and selecting this moves you back up a folder.
- The bottom pane, from the left:
	- Device Selection
	- Global Settings, Network Settings, Global Game Settings, Default Game Settings, and Current Game Settings
	- System Info, Device Info, Version Info, and Greetings
	- Return to top of file system
	- Restart GameCube
