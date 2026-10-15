#!/usr/bin/env bash
# Builds a portable linux-x64 .vcvplugin (glibc 2.27 floor) in Ubuntu 18.04 with gcc-8.
# Usage: build-dist.sh <repo> <rack-src> [outdir]. Needs tools/setup-rack.sh's rack-src. Starts dockerd if needed.
set -euo pipefail
repo=$(realpath "$1"); rack=$(realpath "$2"); out=$(realpath -m "${3:-/mnt/project-files/builds}")
docker info >/dev/null 2>&1 || { nohup dockerd >/tmp/dockerd.log 2>&1 & for _ in $(seq 30); do docker info >/dev/null 2>&1 && break; sleep 1; done; }
if ! docker image inspect cxo-build:bionic >/dev/null 2>&1; then
	docker image inspect ubuntu:18.04 >/dev/null 2>&1 || docker pull ubuntu:18.04 || { docker pull mirror.gcr.io/library/ubuntu:18.04 && docker tag mirror.gcr.io/library/ubuntu:18.04 ubuntu:18.04; }
	docker run --name cxo-setup --network host ubuntu:18.04 bash -c \
		'apt-get update -qq && DEBIAN_FRONTEND=noninteractive apt-get install -y -qq g++-8 make zstd jq libjansson-dev libglew-dev libglfw3-dev libspeexdsp-dev >/dev/null'
	docker commit cxo-setup cxo-build:bionic >/dev/null && docker rm cxo-setup >/dev/null
fi
docker run --rm --network host -v "$repo":/src -v "$rack":/rack -w /src cxo-build:bionic \
	bash -c 'make clean RACK_DIR=/rack CC=gcc-8 CXX=g++-8 >/dev/null && make -j8 RACK_DIR=/rack CC=gcc-8 CXX=g++-8 dist >/dev/null && chown -R '"$(id -u):$(id -g)"' build dist plugin.so'
objdump -T "$repo"/dist/*/plugin.so | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -1
mkdir -p "$out" && cp "$repo"/dist/*.vcvplugin "$out"/ && ls -l "$repo"/dist/*.vcvplugin
