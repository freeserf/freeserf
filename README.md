Forkserf
========

A continuation of Freeserf, a SerfCity/Settlers1 clone in C/C++.  Freeserf was created by jonls and wdigger.  

I am moving forward with it in my own way.  My vision is an authentic version of the original game but with an outstanding AI instead of the terrible one in the original, plus many *optional* features to tweak things that interest me.  I am only interested in random single player vs AI games at this time, I am not playtesting the single-player Missions nor working on multiplayer.  If you are looking for a very accurate SerfCity/Settlers1 experience with original AI, missions, and multiplayer I suggest trying Serflings by nicymike or playing the original game with DosBox

One unique thing about Freeserf (and remains in Forkserf) is that you can play with the original Amiga graphics, sounds, and music if you like.  The default is to prefer DOS graphics and a mix of Amiga & DOS audio if both are supplied.

Forkserf adds:
- advanced AI for non-human players
  - greatly improved building placement logic
  - greatly improved road building logic
  - builds stocks (warehouses) intelligently, with entire parallel economies surrounding them
  - note that all AI characters use same logic for now, I will add Character flavors back in at some point
- fixes for Freeserf bugs
  - major crash bugs during normal gameplay fixed
  - crash & other bugs related to combat
  - some minor visual & audio issues
  - crash & other issue with food resource delivery
  - serfs become forever stuck on roads and block the road
  - missing ambient sounds added
- new features (mostly as optional checkboxes)
  - auto-saving, see https://github.com/forkserf/forkserf/wiki/auto-saving
  - uses both DOS *and* Amiga sounds if both available, see https://github.com/forkserf/forkserf/wiki/using-both-DOS-*and*-Amiga-sounds
  - custom map generator, see https://github.com/forkserf/forkserf/wiki/Custom-Map-Generator
  - seasonal progression with unique graphics for winter/spring/summer/fall, see wiki https://github.com/forkserf/forkserf/wiki/Four-Seasons
  - advanced wheat farming,  see https://github.com/forkserf/forkserf/wiki/Advanced-Wheat-Farming
  - serfs can be transported in Boats!  see https://github.com/forkserf/forkserf/wiki/serfs-can-be-transported-in-Boats!
  - empty building sites don't require any burn wait, see https://github.com/forkserf/forkserf/wiki/empty-building-sites-don't-burn
  - baby trees mature slowly, nerfs forester/ranger,  see https://github.com/forkserf/forkserf/wiki/Baby-Trees-Mature-Slowly
  - trees spawn spontaneously, allows forests to grow on their own, see https://github.com/forkserf/forkserf/wiki/Trees-Reproduce
- internal game mechanics changes to reduce gridlock issues that exist even in the original game
  - timeout & retry of building resource requests, see https://github.com/forkserf/forkserf/wiki/timeout-&-retry-of-building-resource-requests
  - prioritized transport of resources that can be immediately used, see https://github.com/forkserf/forkserf/wiki/prioritized-transport-of-resources-that-can-be-immediately-used
  - faster clearing of Lost serfs, see https://github.com/forkserf/forkserf/wiki/faster-clearing-of-Lost-serfs
- debugging tools
  - mark positions with colored dots, see https://github.com/forkserf/forkserf/wiki/mark-positions-with-colored-dots
  - mark specific serfs and their state, see https://github.com/forkserf/forkserf/wiki/mark-specific-serfs-and-their-state
  - manually "boot" specific serfs by setting them to Lost state
  - trace road pathfinding, see https://github.com/forkserf/forkserf/wiki/trace-road-pathfinding
  - identification & colored highlighting of arterial roads, see https://github.com/forkserf/forkserf/wiki/identification-&-colored-highlighting-of-arterial-roads
  - hidden resource overlay (THIS IS CHEATING!)
- no multiplayer yet, sorry.  No plans for this anytime soon

Details about all of the above are on the wiki: https://github.com/forkserf/forkserf/wiki

Build status
------------
check here: https://github.com/forkserf/forkserf/actions

Last build downloads
--------------------

Because Github Actions is kind of half-baked, it doesn't allow linking directly to the most recent commit's artifact.
You must manually go here: https://github.com/forkserf/forkserf/actions  and select either the linux or windows build workflow, scroll 
 down to the bottom under 'Artifacts: produced during runtime;, and you can download the compiled binary there.  
 Hopefully they will improve this soon.

Play
------
Copy the data file(s) from the original game to the same directory as freeserf. Alternatively you can put the data file in `~/.local/share/freeserf`. You may use data file(s) from DOS or Amiga game version.

* DOS data file is called `SPAE.PA`, `SPAD.PA`, `SPAF.PA` or `SPAU.PA`, depending on the language of the game.
* Amiga files `gfxheader`, `gfxfast`, `gfxchip`, `gfxpics`, `sounds`, `music`.

Keyboard gameplay controls:

* `1`, `2`, `3`, `4`, `5`: Activate one of the five buttons in the panel.
* `b`: Toggle overlay showing possibilities for constructions.
* ~~`TAB`/SHIFT-`TAB`: Open next notification message; or return from last message.~~ removed for now because of alt-tab issues
* `+`/`-`: Increase/decrease game speed.  Default is 2, can go up to 40
* `0`: Reset game speed.
* `p`: Pause game.
* `j`: Switch player, you can control even AI players games while they play, though it might cause instability if you go too crazy with it
* `y`: AI/debug overlay (only shows for AI players)
* `g`: grid/serf state debug overlay
* `h`: hidden resource overlay (THIS IS CHEATING!)
* `w`: enable/disable Four Seasons & Advanced Farming (they are tied together right now, will allow separation later)
* ~~`q`: advance to next season~~  disabled for now, season is now tied to tick so it works with save/load game)
* ~~`e`: advance to next subseason~~ disabled for now, subseason is now tied to tick so it works with save/load game

Other keyboard controls:

* `CTRL`+`n` or `F10`: Raise game-init popup, can start a new game
* `s`: Enable/disable sounds playback
* `m`: Enable/disable music playback
* `CTRL`+`f`: Switch fullscreen mode on/off.  (should add ALT-ENTER at some point also)
* `CTRL`+`z`: Quicksave game in current directory.
* `[`/`]`: Zoom -/+
* `CTRL`-`MouseWheel`: Zoom -/+     
  *note there's a bug where if you resize the game window after zooming you need to zoom again or cursor gets messed up


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

back in business, for 2022-2023 winter
