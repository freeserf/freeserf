Freeserf
========

[![Join the chat at https://gitter.im/freeserf/freeserf](https://badges.gitter.im/Join%20Chat.svg)](https://gitter.im/freeserf/freeserf?utm_source=badge&utm_medium=badge&utm_campaign=pr-badge&utm_content=badge)

Build status
------------

[![Build Status](https://travis-ci.org/freeserf/freeserf.svg?branch=master)](https://travis-ci.org/freeserf/freeserf)
[![Build Status](https://ci.appveyor.com/api/projects/status/github/freeserf/freeserf?branch=master&svg=true)](https://ci.appveyor.com/project/jonls/freeserf)

Last build downloads
--------------------
* [Windows x86](https://ci.appveyor.com/api/projects/jonls/freeserf/artifacts/freeserf-VC-x86-Release.zip?branch=master&job=Configuration%3A%20Release%3B%20Platform%3A%20x86)
* [Windows x64](https://ci.appveyor.com/api/projects/jonls/freeserf/artifacts/freeserf-VC-x64-Release.zip?branch=master&job=Configuration%3A%20Release%3B%20Platform%3A%20x64)
* [Linux Sources TAR.GZ](https://s3.amazonaws.com/freeserftest1/freeserf-0.2.tar.gz)
* [Linux Sources TAR.XZ](https://s3.amazonaws.com/freeserftest1/freeserf-0.2.tar.xz)
* [macOS](https://s3.amazonaws.com/freeserftest1/FreeSerf.dmg)

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

Other keyboard controls:

* `F10`: Return to main menu to start a new game
* `s`: Enable/disable sounds playback
* `m`: Enable/disable music playback
* CTRL+`f`: Switch fullscreen mode on/off.
* CTRL+`z`: Save game in current directory.
* `[`/`]`: Zoom -/+


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
