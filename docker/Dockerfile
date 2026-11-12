FROM ubuntu:24.04

RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
       build-essential cmake ninja-build git ca-certificates curl cpio file \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
