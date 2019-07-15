Build a .framework from the official repos.
Despite it's name (opencv2.framework) it's opencv4!

The current head has a build issue:
https://github.com/opencv/opencv/issues/13759

TLDR: Update opencv/platforms/osx/build_framework.py deployment OS from 10.9 to 10.12

We also want to include the aruco module, so the contrib repository also needs to be cloned;
https://github.com/opencv/opencv_contrib

To only include the aruco module, I deleted all the other modules in opencv_contrib!

Build with 
`python platforms/osx/build_framework.py --contrib /Volumes/Code/opencv_contrib/ osxbuild`

