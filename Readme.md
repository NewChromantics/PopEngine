Build Status
==========================
![Create Release](https://github.com/NewChromantics/PopEngine/workflows/Create%20Release/badge.svg)
![Build Apple](https://github.com/NewChromantics/PopEngine/workflows/Build%20Apple/badge.svg)
![Build Windows](https://github.com/NewChromantics/PopEngine/workflows/Build%20Windows/badge.svg)
![Build Linux](https://github.com/NewChromantics/PopEngine/workflows/Build%20Linux/badge.svg)

Pop Engine
==================================

Javascript Runtimes
-------------
- Windows & Hololens1
	- Native JSRT/ChakraCore from windows SDK (NOT in sync with the chakracore github repository)
- IOS
	- Native Javascriptcore
- OSX
	- Native JavascriptCore
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
	- Magic_enum requires gcc9, jetson (and pi?) have `gcc --version` 7
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

GitHub Actions Notes
-------------
If you are getting git checkout errors on self hosted runners ssh in and manually delete the repo then restart the workflow

Edit test 
