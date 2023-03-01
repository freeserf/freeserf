
Build from repository
----------------------------

This project uses the [CMake](https://cmake.org) build system.
Use the following commands to build (it is recommended to build in a separate directory as shown here):

``` shell
$ mkdir build && cd build
$ cmake ..
$ make
```
or select platform dependent [generator](https://cmake.org/cmake/help/v3.0/manual/cmake-generators.7.html) for your favorite IDE.

Some useful configure variables (set as environment variable or with `-D` command line option):

* `SDL2_DIR` - path to SDL2 root directory
* `SDL2_mixer_DIR` - path to SDL2_mixer root directory (optional)
* `SDL2_image_DIR` - path to SDL2_image root directory (optional)

Dependencies
------------

* [SDL2](https://github.com/libsdl-org/SDL/releases) (Development Libraries)
* [SDL2_mixer](https://github.com/libsdl-org/SDL_mixer/releases) (Optional; for audio playback) (Development Libraries)
* [SDL2_image](https://github.com/libsdl-org/SDL_image/releases) (Optional; for custom resources) (Development Libraries)

Coding style
------------

For the C++ code we are following the [Google C++ Style Guide](http://google.github.io/styleguide/cppguide.html).

You can check style during the build

``` shell
$ make check_style
```


Creating a pull request
-----------------------

1. Create a topic branch for your specific changes. You can base this off the
   master branch or a specific version tag if you prefer (`git co -b topic master`).
2. Create a commit for each logical change on the topic branch. The commit log
   must contain a one line description (max 80 chars). If you cannot describe
   the commit in 80 characters you should probably split it up into multiple
   commits. The first line can be followed by a blank line and a longer
   description (split lines at 80 chars) for more complex commits. If the commit
   fixes a known issue, mention the issue number in the first line (`Fix #11:
   ...`).
3. The topic branch itself should tackle one problem. Feel free to create many
   topic branches and pull requests if you have many different patches. Putting
   them into one branch makes it harder to review the code.
4. Push the topic branch to Github, find it on github.com and create a pull
   request to the master branch. If you are making a bug fix for a specific
   release you can create a pull request to the release branch instead
   (e.g. `release-1.9`).
5. Discussion will ensue. If you are not prepared to partake in the discussion
   or further improve your patch for inclusion, please say so and someone else
   may be able to take on responsibility for your patch. Otherwise we will
   assume that you will be open to critisism and suggestions for improvements
   and that you will take responsibility for further improving the patch. You
   can add further commits to your topic branch and they will automatically be
   added to the pull request when you push them to Github.
6. You may be asked to rebase the patch on the master branch if your patch
   conflicts with recent changes to the master branch. However, if there is no
   conflict, there is no reason to rebase. Please do not merge the master back
   into your topic branch as that will convolute the history unnecessarily.
7. Finally, when your patch has been refined, you may be asked to squash small
   commits into larger commits. This is simply so that the project history is
   clean and easy to follow. Remember that each commit should be able to stand
   on its own, be able to compile and function normally. Commits that fix a
   small error or adds a few comments to a previous commit should normally just
   be squashed into that larger commit.

If you want to learn more about the Git branching model that we use please see
<http://nvie.com/posts/a-successful-git-branching-model/> but note that we use
the `master` branch as `develop`.


Creating a new release
----------------------

1. Select a commit in master to branch from, or if making a bugfix release
   use previous release tag as base (e.g. for 1.9.1 use 1.9 as base)
2. Create release branch `release-X.Y`
3. Apply any bugfixes for release
4. Update version in `CMakeLists.txt`
5. Commit and tag release (`vX.Y` or `vX.Y.Z`)
6. Push tag to Github and also upload package files: Windows `.exe` package (from Appveyor), macOS `.dmg` (from Travis)
