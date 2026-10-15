#!/usr/bin/env bash
# Rack 2 source headers plus a stub libRack.so, enough to compile and link the plugin without the SDK zip.
set -euo pipefail
dir=$(realpath -m "${1:-$HOME/rack-src}")
if [ ! -d "$dir/.git" ]; then
	git clone --depth 1 --branch v2.6.6 https://github.com/VCVRack/Rack.git "$dir"
fi
cd "$dir"
for sub in dep/nanovg dep/nanosvg dep/oui-blendish dep/simde; do
	git submodule update --init --depth 1 "$sub"
done
mkdir -p dep/include
for h in nanovg nanovg_gl nanovg_gl_utils; do ln -sf ../nanovg/src/$h.h dep/include/$h.h; done
for h in nanosvg nanosvgrast; do ln -sf ../nanosvg/src/$h.h dep/include/$h.h; done
ln -sf ../oui-blendish/blendish.h dep/include/blendish.h
ln -sfn ../simde/simde dep/include/simde
[ -f dep/include/pffft.h ] || curl -fsSL https://bitbucket.org/jpommier/pffft/raw/master/pffft.h -o dep/include/pffft.h
if ! dpkg -s libjansson-dev libglew-dev libglfw3-dev libspeexdsp-dev >/dev/null 2>&1; then
	apt-get install -y -qq libjansson-dev libglew-dev libglfw3-dev libspeexdsp-dev >/dev/null || \
		{ apt-get update -qq && apt-get install -y -qq libjansson-dev libglew-dev libglfw3-dev libspeexdsp-dev >/dev/null; }
fi
[ -f libRack.so ] || { echo | gcc -shared -o libRack.so -x c -; }
echo "RACK_DIR=$dir"
