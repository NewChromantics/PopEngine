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
	- JavascriptCore from `apt-get webkitgtk-4.0-dev`
- Android
	- V8 build from https://github.com/JaneaSystems/nodejs-mobile/releases

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
