# This is the version needs to match what was used in the build in Github Workflows so the package versions match
FROM ubuntu:18.04

# To avoid "tzdata" asking for geographic area
ARG DEBIAN_FRONTEND=noninteractive

# tsdk: have to update gcc to avoid the error /usr/lib/x86_64-linux-gnu/libstdc++.so.6: version `GLIBCXX_3.4.26' not found
# is there an simpler way to do this?
RUN apt update -qq && \
    apt install -qq -y software-properties-common && \
    add-apt-repository -y ppa:ubuntu-toolchain-r/test && \
    apt install -qq -y \
        git \
        npm \
        libx264-dev \
        libjavascriptcoregtk-4.0-dev \
        gcc-10 \
        g++-10 \
        kmscube \
        libgles2-mesa-dev \
        libgbm-dev \
        libudev-dev \
        gdb

RUN update-alternatives --install \
        /usr/bin/gcc gcc \
        /usr/bin/gcc-10 10

RUN update-alternatives --install \
        /usr/bin/g++ g++ \
        /usr/bin/g++-10 10