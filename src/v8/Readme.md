Latest V8 version
===========================
Official build-bot urls. JSVU helps with this.

- https://github.com/GoogleChromeLabs/jsvu/blob/master/engines/v8/get-specific-version.js
- https://storage.googleapis.com/chromium-v8/official/canary/v8-mac64-rel-latest.json
	{"version": "7.6.115"}

- https://ci.chromium.org/p/v8/builders/ci/V8%20Mac64/28723
- `mac64` for osx
- https://storage.googleapis.com/chromium-v8/official/canary/v8-mac64-rel-7.6.115.zip
- https://storage.googleapis.com/chromium-v8/official/canary/v8-mac64-dbg-7.6.115.zip <--- has libs!
- https://storage.googleapis.com/chromium-v8/official/canary/v8-win64-dbg-7.6.115.zip
- Get headers from github releases

win64-rel only has binary files, no DLL's or libs...
win64-dbg only has DLL's/libs linked to debug CRT grrrr