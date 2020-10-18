#!/bin/bash

docker build --tag popenginetest_docker_linux .

# This will run the app interactively (to make sure its going in a shell) and then when you close the shell it will remove the docker image
# Make sure to run the docker build before hand and change the mounted volumes

docker run -it --rm -v ./:/build popenginetest_docker_linux sh
