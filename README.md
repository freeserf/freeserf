Forkserf
========

A continuation of Freeserf, a SerfCity/Settlers1 clone in C/C++.  Freeserf was created by jonls and wdigger

Forkserf adds:
- fixes for Freeserf bugs
- advanced AI for non-human players
- new features (mostly as optional checkboxes)
  - foo
  - foo2
- internal game mechanics changes to fix problems that existed even in the original game

Build status
------------
coming soon

Last build downloads
--------------------
coming soon, check https://github.com/tlongstretch/freeserf-with-AI-plus-binaries for the moment
Play
------
Copy the data file(s) from the original game to the same directory as freeserf. Alternatively you can put the data file in `~/.local/share/freeserf`. You may use data file(s) from DOS or Amiga game version.

* DOS data file is called `SPAE.PA`, `SPAD.PA`, `SPAF.PA` or `SPAU.PA`, depending on the language of the game.
* Amiga files `gfxheader`, `gfxfast`, `gfxchip`, `gfxpics`, `sounds`, `music`.

Keyboard gameplay controls:

* `1`, `2`, `3`, `4`, `5`: Activate one of the five buttons in the panel.
* `b`: Toggle overlay showing possibilities for constructions.
* `TAB`/SHIFT-`TAB`: Open next notification message; or return from last message.
* `+`/`-`: Increase/decrease game speed.
* `0`: Reset game speed.
* `p`: Pause game.
* `j`: Switch player.
* `y`: AI/debug overlay (only shows for AI players)

Other keyboard controls:

* `F10`: Return to main menu to start a new game
* `s`: Enable/disable sounds playback
* `m`: Enable/disable music playback
* CTRL+`f`: Switch fullscreen mode on/off.
* CTRL+`z`: Save game in current directory.
* `[`/`]`: Zoom -/+
* CTRL-MouseWheel: Zoom -/+     // note there's a bug where if you resize the game window after zooming you need to zoom again or cursor gets messed up


Audio
-----

To play back the sound track that is included in the original data files,
SDL2_mixer has to be enabled at compile-time and a set of sound patches
for SDL2_mixer has to be available at runtime. See the SDL2_mixer
documentation for more information.


Save games
----------
To load a save game file:

`$ freeserf -l FILE`

Freeserf will (try to) load save games from the original game, as well as saves from freeserf itself.
The game is paused after loading so press `p` to start the game.

Run `freeserf -h` for more info on command line options.


Bugs
----
Please report bugs at <https://github.com/forkserf/forkserf/issues>.


Development
-----------
The main source repository for this project is at <https://github.com/forkserf/forkserf>. See the HACKING document in the source code root for information on how to compile and how to contribute.
