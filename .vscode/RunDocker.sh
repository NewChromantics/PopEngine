#!/bin/bash

docker build --tag PopEngineTest_Docker_Linux .

# This will run the app interactively (to make sure its going in a shell) and then when you close the shell it will remove the docker image
# Make sure to run the docker build before hand and change the mounted volumes

docker run -it --rm -v ~/Development/PopEngine:/build PopEngineTest_Docker_Linux sh