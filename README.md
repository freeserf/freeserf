Freeserf
========

Build status
------------

[![Build Status](https://travis-ci.org/freeserf/freeserf.svg?branch=master)](https://travis-ci.org/freeserf/freeserf)

Play
------
Copy the data file from the original game to the same directory as freeserf. Alternatively you can put the data file in `~/.local/share/freeserf`. The data file is called `SPAE.PA`, `SPAD.PA`, `SPAF.PA` or `SPAU.PA`, depending on the language of the game.

Keyboard gameplay controls:

* `1`, `2`, `3`, `4`, `5`: Activate one of the five buttons in the panel.
* `b`: Toggle overlay showing possibilities for constructions.
* `TAB`/SHIFT-`TAB`: Open next notification message; or return from last message.
* `+`/`-`: Increase/decrease game speed.
* `0`: Reset game speed.
* `p`: Pause game.

Other keyboard controls:

* `F10`: Return to main menu to start a new game
* `s`: Enable/disable sounds playback
* `m`: Enable/disable music playback
* CTRL+`f`: Switch fullscreen mode on/off.
* CTRL+`z`: Save game in current directory.


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
Please report bugs at <https://github.com/freeserf/freeserf/issues>.


Development
-----------
The main source repository for this project is at <https://github.com/freeserf/freeserf>. See the HACKING document in the source code root for information on how to compile and how to contribute.
