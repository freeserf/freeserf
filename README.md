Freeserf
========

Compile
-------
After first checkout run `./bootstrap`, `./configure` then `make`. After making changes to the code just run `make` to rebuild.


Try it
------
Copy the data file (`SPAE.PA`) to the same directory as the program.

Keyboard controls:

* 1, 2, 3, 4, 5: Activate one of the five buttons in the panel.
* +/-: Increase/decrease game speed.
* 0: Reset game speed.
* p: Pause game.
* s: Enable/disable sounds playback
* m: Enable/disable music playback

Because of a bug in SDL the game may hang when playing sound effects if you are using pulseaudio. This can be worked around by selecting a different SDL audio driver (e.g. SDL_AUDIODRIVER=alsa).


Save games
----------
To load a save game file from the original game:

`$ freeserf -l SAVE0.DS`

The game is paused after loading so press `p` to start the game. Saving the same in freeserf is not possible yet.

Run `freeserf -h` for more info on command line options.
