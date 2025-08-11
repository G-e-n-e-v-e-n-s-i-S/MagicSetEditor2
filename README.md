# Magic Set Editor 2

Magic Set Editor, or MSE for short, is a program with which you can design your own cards for popular trading card games. MSE can then generate images of those cards that you can print or upload to the internet. Magic Set Editor also has a statistics window that will give useful information about your set, like the average mana cost, number of rares, etc. When you have finished your set, you can export it to an HTML file to use on the Internet, or to Apprentice or CCG Lackey so you can play with your cards online.

More information on https://magicseteditor.boards.net/

Documentation on https://mseverse.miraheze.org/wiki/Category:MSE_Documentation


## Dependencies

The code depends on
 * wxWidgets >= 3.3.1
 * boost
 * hunspell


## Building on Windows with Visual Studio

On windows, the program can be compiled with Visual Studio (recommended) or with mingw-gcc.
 * Visual Studio instructions up-to-date as of April 2024
 * Download and install [Visual Studio Community Edition](https://visualstudio.microsoft.com/vs/community/)
 * Download and install [vcpkg](https://github.com/microsoft/vcpkg)
 * Use vcpkg to install pkgconf, wxwidgets, boost, hunspell:

````
.\vcpkg install pkgconf wxwidgets[fonts] boost-smart-ptr boost-regex boost-logic boost-pool boost-iterator boost-json hunspell --triplet=x64-windows-static
````
and/or
````
.\vcpkg install pkgconf wxwidgets[fonts] boost-smart-ptr boost-regex boost-logic boost-pool boost-iterator boost-json hunspell --triplet=x86-windows-static
````
(these two lines differ only by the triplet at the end. Use x64 for 64 bit operating systems, and x86 for 32 bit.)

then, regardless of your choice
````
.\vcpkg integrate install
````

 * Inside Visual Studio go to File menu > Open > Folder... > select the Magic Set Editor source code root folder.
 * Select the configuration that you want to build (probably release x64-windows-static). Make sure you have installed all the packages with the corresponding triplet.

![configuration](https://github.com/G-e-n-e-v-e-n-s-i-S/MagicSetEditor2/blob/main/resource/readme/configuration.png)

 * Just to the right of that, select magicseteditor.exe. (These options sometimes get de-selected. If you suddenly can't build anymore, make sure to re-select these.)
 * To build the app go to Build menu > build magicseteditor.exe

Notes:
 * You will most likely get a message about being unable to open hunspell-1.7.lib because pkgconf forgets to add the actual path to HUNSPELL_LIBRARIES. If so, uncomment the noted line in CMakeLists.txt (line 31) and point it to the root vcpkg installation to find the correct hunspell-1.7.lib file.

![hunspell](https://github.com/G-e-n-e-v-e-n-s-i-S/MagicSetEditor2/blob/main/resource/readme/hunspell.png)

 * For running tests you will also need to download and install perl (For example [Strawberry perl](http://strawberryperl.com/) or using [MSYS2](https://www.msys2.org/)). The tests can be run from inside visual studio


## Building on Windows with GCC (MSYS2)

 * Download and install [msys2](https://www.msys2.org/)
 * Install a recent version of the gcc compiler, cmake, and wxWidgets libraries:

```
pacman -S mingw32/mingw-w64-i686-gcc
pacman -S mingw32/mingw-w64-i686-wxWidgets
pacman -S mingw32/mingw-w64-i686-boost
pacman -S mingw32/mingw-w64-i686-hunspell
pacman -S cmake
```

   Use `mingw64/mingw-w64-x86_64-...` instead of for the 64bit build.
 * Build

```
cmake -G "MSYS Makefiles" -H. -Bbuild -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

 Use `CMAKE_BUILD_TYPE=Debug` for a debug build.


## Building on Linux / FreeBSD

 * Install the dependencies:

```
# debian 12+ / ubuntu 24.04+
sudo apt install libboost-dev libboost-regex-dev libwxgtk3.2-dev libhunspell-dev cmake
# debian 11 / ubuntu 22.04
sudo apt install libboost-dev libboost-regex-dev libwxgtk3.0-gtk3-dev libhunspell-dev cmake
# fedora / centos
sudo dnf install boost-devel wxGTK-devel hunspell-devel git cmake
# archlinux / manjaro
sudo pacman -Syu wxgtk3 hunspell boost git cmake
# freebsd
sudo pkg install hunspell cmake wx30-gtk3 boost-all
```

Then use cmake to build:

```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build .
```

 Use `-CMAKE_BUILD_TYPE=Debug` for a debug build.

### wx-config can't be found
On old versions it's possible that cmake can't find wx-config, to solve this add the tool to the cmake command manually like this: `-DwxWidgets_CONFIG_EXECUTABLE=/usr/bin/wx-config-gtk3`

### Installing resources
Install the resource folder to the .magicseteditor dir: `mkdir -p $HOME/.magicseteditor && cp -rT ./resource $HOME/.magicseteditor/resource`
Templates are installed to `~/.magicseteditor/data`. Fonts are installed to `~/.local/share/fonts`.


## Building on Mac OS

 * Install the dependencies; for example, using Homebrew: (Note: Tested with boost 1.84.0, wxmac (wxwidgets) 3.2.4, hunspell 1.7.2, cmake 3.28.3, dylibbundler 1.0.5.)

```
brew install boost wxwidgets hunspell cmake dylibbundler
```

 * Then use cmake to build:

```
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_APPLE_BUNDLE=1 ..
cmake --build .
```

 Use `CMAKE_BUILD_TYPE=Debug` for a debug build.
 * Finally, copy the resources to a SharedSupport directory and run the executable:

```
mkdir magicseteditor.app/Contents/SharedSupport
cp -r ../resource magicseteditor.app/Contents/SharedSupport
open ./magicseteditor.app
```
