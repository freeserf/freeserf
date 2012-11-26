Freeserf
========

Try it
------
Copy the data file (`SPAE.PA`) to the same directory as the program. Alternatively you can put the data file in `~/.local/share/freeserf`.

Keyboard controls:

* 1, 2, 3, 4, 5: Activate one of the five buttons in the panel.
* +/-: Increase/decrease game speed.
* 0: Reset game speed.
* p: Pause game.
* s: Enable/disable sounds playback
* m: Enable/disable music playback


Save games
----------
To load a save game file from the original game:

`$ freeserf -l SAVE0.DS`

The game is paused after loading so press `p` to start the game. Saving the game in freeserf is not possible yet.

Run `freeserf -h` for more info on command line options.


Bugs
----
Please report bugs at <https://github.com/jonls/freeserf/issues>.


Development
-----------
The main source repository for this project is at <https://github.com/jonls/freeserf>. See the HACKING document in the source code root for information on how to compile and how to contribute.
