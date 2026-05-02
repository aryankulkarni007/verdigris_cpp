FROM ubuntu:latest

ENV DEBIAN_FRONTEND=noninteractive

RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    valgrind \
    gdb \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /app
