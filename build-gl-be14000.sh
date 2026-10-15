#!/bin/sh
# Build the GL.iNet GL-BE14000 (Flint 4) firmware of this branch.
#
# Usage: ./build-gl-be14000.sh [make arguments]
#
# Takes the package selection from gl-be14000.diffconfig (Tailscale,
# conntrack, avahi, full wpad with FreeRADIUS for 802.1X, LuCI with HTTPS,
# the front panel UI, ...) and builds the image into
# bin/targets/mediatek/filogic/. Only the vendor-U-Boot image is built
# (glinet_gl-be14000, not the -ubootmod variant).
#
# Regenerate the diffconfig after changing packages in menuconfig:
#   ./scripts/diffconfig.sh > gl-be14000.diffconfig

set -e
cd "$(dirname "$0")"

./scripts/feeds update -a
./scripts/feeds install -a

if [ -f .config ] && ! cmp -s .config gl-be14000.diffconfig; then
	cp .config .config.before-build
	echo "existing .config saved as .config.before-build"
fi
cp gl-be14000.diffconfig .config
make defconfig

# Host Python 3.14 fails to build its curses module on some build hosts
# ("COLOR_YELLOW undeclared"); the build never uses it.
export py_cv_module__curses=n/a py_cv_module__curses_panel=n/a

make -j"$(nproc)" download world "$@"

ls -l bin/targets/mediatek/filogic/*glinet_gl-be14000-squashfs-sysupgrade.bin
