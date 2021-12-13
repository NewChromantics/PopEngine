Build Status
==========================
![Create Release](https://github.com/NewChromantics/PopEngine/workflows/Create%20Release/badge.svg)
![Build Apple](https://github.com/NewChromantics/PopEngine/workflows/Build%20Apple/badge.svg)
![Build Windows](https://github.com/NewChromantics/PopEngine/workflows/Build%20Windows/badge.svg)
![Build Linux](https://github.com/NewChromantics/PopEngine/workflows/Build%20Linux/badge.svg)

Pop Engine
==================================

Features
---------------
Not comprehensive, and in no particular order. But things this engine can do
- Build small enough for app-clips (down to 2mb, +2mb for poph264 and +2mb for PopCameraDevice - need to remove these dependencies)
- Swift integration (mac & ios apps now expected to boot from swift `@main` classes)
- Runs on pi's
- Runs on Nvidia Jetsons
- Runs in docker
- `import` and `export` handler (preprocessor) to allow modules in native ios/mac JavascriptCore
- Once ran v8, (mac, windows and linux) but currently dropped as it's a nightmare to build v8 and impossible to find static libs for every platform
- Runs chakra-core on windows (& hololens, at one time) for native JS
- Once ran charkracore (from repository) on mac and ios as proof of concept
- Same JS (via `Pop.`) for webxr, openxr etc
- Abstracted rendering system. Serialised "Render commands" (`SetRenderTarget`,`Draw` etc) to aid network rendering, debugging etc - can even render same commands to metal and GL at the same time)
- Heavily async internally (lots of work farmed to c++ threads)
- Uses native GUI apis where possible (interfaces with HTML, swift, win32)
- Websocket servers & clients (bar web which only has WS clients), UDP, TCP sockets, HTTP servers, clients etc
- Explicit native image handling/interfacing for seamless bytes<->canvas<->rendering integrations

Javascript Runtimes
-------------
- Windows & Hololens1
	- Native JSRT/ChakraCore from windows SDK (NOT in sync with the chakracore github repository)
- IOS
	- Native Javascriptcore
- OSX
	- Native JavascriptCore (Debuggable in safari via https://developer.apple.com/documentation/security/hardened_runtime )
	- v8 build from...
	- chakracore from github repository
	- JavascriptCore/webkit-jsonly from github repository
- Raspberry PI
	- JavascriptCore from `apt-get install webkitgtk-4.0-dev`
- Jetson
	- JavascriptCore from `sudo apt-get install libjavascriptcoregtk-4.0-dev`
- Android
	- JavascriptCore from https://github.com/react-native-community/jsc-android-buildscripts v236355.1.1

Build Notes
-------------
- Raspberry PI/Linux via visual studio
	- https://docs.microsoft.com/en-us/cpp/linux/connect-to-your-remote-linux-computer?view=vs-2019
	- `sudo apt install openssh-server`
	- `sudo service ssh start`
	- `sudo systemctl enable ssh`
	- Setup Visual studio via `Tools->Options->Cross Platform->Connections`
	- [Magic_enum requires gcc9](https://github.com/Neargye/magic_enum#compiler-compatibility), jetson (and pi?) have `gcc --version` 7
		- `sudo apt-get install gcc-9` doesnt exist yet
		- https://askubuntu.com/a/1140203
		- `sudo apt-get install software-properties-common`
		- `sudo add-apt-repository ppa:jonathonf/gcc-9.0`
		- `sudo apt-get install gcc-9 g++-9`
		- make gcc/g++ default to new version `sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-9 60 --slave /usr/bin/g++ g++ /usr/bin/g++-9`

- Osx
	- Install engine package dependencies (PopH264, PopCameraDevice, PopVision) via our published packages via npm (with the github repository)
	- Modify `PopEngine.Package/.npmrc` to change `YOUR_AUTH_TOKEN` into your github auth token with access to NewChromantics packages (ask graham@newchromantics.com). 
	- Do not commit this token. Github will spot it and revoke the token for security.
	- run `cd PopEngine.Package && npm install` to download the packages
	- run `ln -s node_modules/@newchromantics/ ../Libs` to make a symbolic link to the packages in the correct (`/Libs/`) directory

- Linux(raspi buster) via VirtualBox + VSCode
	- Install `remote-ssh` into vscode
	- Make sure SSH-Server is installed (default installed in raspbi buster) [Install `ssh-server` on linux;](https://code.visualstudio.com/docs/remote/troubleshooting#_installing-a-supported-ssh-server)  `sudo apt-get install openssh-server` 
	- Enable SSH server (default disabled) `sudo raspi-config` and enable from interface options
	- Connect with default `pi@raspberry.local` by pressing `F1` and typing `Remote-ssh: connect to host` (lovely ux)
	- Updated GCC if `gcc --version` is less than 9 (See above). (Raspian currently default 8)
		- gr: the `jonathonf` routine isn't working. 
		- `sudo apt-install g++-9` installs... `clang-9` but not `g++-9`. There doesn't seem to be `gcc-9` still. 

GitHub Actions Notes
-------------
If you are getting git checkout errors on self hosted runners ssh in and manually delete the repo then restart the workflow

Edit test 
