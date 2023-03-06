Forkserf
========

A continuation of Freeserf, a SerfCity/Settlers1 clone in C/C++.  Freeserf was created by jonls and wdigger.  

Game Information Website
========================

- https://forkserf.github.io/
- Discord channel 'Forkserf'

Current Release
===============

version 0.6.2 released Feb15 2023


Play
------
Copy the data file(s) from the original game to the same directory as freeserf. Alternatively you can put the data file in `~/.local/share/freeserf`. You may use data file(s) from DOS or Amiga game version.  If available, it is recommended to include BOTH Amiga and DOS files as Forkserf will use the best of both asset sets.

* DOS data file is called `SPAE.PA`, `SPAD.PA`, `SPAF.PA` or `SPAU.PA`, depending on the language of the game.
* Amiga files `gfxheader`, `gfxfast`, `gfxchip`, `gfxpics`, `sounds`, `music`.

Keyboard gameplay controls:

* `1`, `2`, `3`, `4`, `5`: Activate one of the five buttons in the panel.
* `b`: Toggle overlay showing possibilities for constructions.  Can also be brought up by special-clicking on Build icon in panel bar
* `+`/`-`: Increase/decrease game speed.  Default is 2, can go up to 40
* `0`: Reset default game speed
* `p`: Pause game.  Also pauses AI player logic thread at the start of their next loop
* `j`: Switch player, you can control even AI players while they play, though it might cause instability if you go too crazy with it
* `y`: AI info overlay (only shows for AI players)
* `d`: Debug overlay
* `g`: Grid/Serf-State debug overlay
* `w`: Enable/disable Four Seasons graphics (it is no longer tied to AdvancedFarming though it is recommended to use them together)
* `f`: Toggle FogOfWar
* `t`: Play next music track, switches between DOS and Amiga music if both available and last track reached
* `s`: Toggle sounds playback
* `m`: Toggle music playback
* `h`: Hidden resource overlay (THIS IS CHEATING!)
* `i`: Next mine popup (cycle through player's Mines)
* `CTRL`+`f`: Switch fullscreen mode on/off.  (should add ALT-ENTER at some point also)
* `CTRL`+`z`: Quicksave game in current directory.
* `[`/`]`: Zoom -/+
* `MouseWheel`: Zoom -/+     
* `CTRL`+`n` or `F10`: Raise game-init popup, can start a new game
* ~~`TAB`/SHIFT-`TAB`: Open next notification message; or return from last message.~~ removed for now because of alt-tab issues
* `ESCAPE`: Close current popup (unless it is moved/pinned).  Can also use right-click to do this

Mouse:
* most operations left-click
* double left-click, OR "special-click" (both left and right at same time), OR center-button/mousewheel-click, OR right-click to trigger original game "special-click" functions
* click and drag the viewport to scroll
* click and drag popup windows to move them around the game window.  If done, multiple windows can be opened at the same time and will auto-refresh
* right click anywhere to close popup window (unless it was moved, then it will stay until closed with its close button)


Audio
-----

To play back the sound track that is included in the original data files,
SDL2_mixer has to be enabled at compile-time and a set of sound patches
for SDL2_mixer has to be available at runtime. See the SDL2_mixer
documentation for more information.


Save games
----------
To load a save game file:

`$ Forkserf -l FILE`

Freeserf will (try to) load save games from the original game, as well as saves from freeserf itself.
The game is paused after loading so press `p` to start the game.

Run `Forkserf -h` for more info on command line options.


Bugs
----
Please report bugs at <https://github.com/forkserf/forkserf/issues>.

Development
-----------
The main source repository for this project is at <https://github.com/forkserf/forkserf>

back in business, for 2022-2023 winter

