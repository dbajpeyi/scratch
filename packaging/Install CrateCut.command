#!/bin/bash
# Double-click to install CrateCut (VST3 + Audio Unit + standalone app) for this user.
#
# What it does:
#   1. copies the plugins into ~/Library/Audio/Plug-Ins (and the app into ~/Applications)
#   2. removes the "downloaded from the internet" quarantine flag
#   3. re-signs them locally (ad-hoc) so macOS will load them
#   4. refreshes the Audio Unit cache
# Nothing is installed system-wide and no admin password is needed.

cd "$(dirname "$0")" || exit 1
HERE="$(pwd)"

VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
APP_DIR="$HOME/Applications"

bold=$(tput bold 2>/dev/null); normal=$(tput sgr0 2>/dev/null)
failed=0

finish() {
    echo
    read -n 1 -s -r -p "Press any key to close this window."
    echo
    exit "$failed"
}

install_bundle() {
    local name="$1" dest_dir="$2"
    local src="$HERE/$name" dest="$dest_dir/$name"

    if [ ! -d "$src" ]; then
        echo "  - $name not found next to the installer, skipped"
        return
    fi

    mkdir -p "$dest_dir"
    rm -rf "$dest"
    if ! ditto "$src" "$dest"; then
        echo "  ✗ couldn't copy $name to $dest_dir"
        failed=1
        return
    fi
    xattr -dr com.apple.quarantine "$dest" 2>/dev/null
    if ! codesign --force --deep --sign - "$dest" >/dev/null 2>&1; then
        echo "  ✗ couldn't sign $name"
        failed=1
        return
    fi
    echo "  ✓ $name  →  ~${dest_dir#"$HOME"}"
}

echo "${bold}Installing CrateCut${normal}"
echo

if [ ! -d "$HERE/CrateCut.vst3" ] && [ ! -d "$HERE/CrateCut.component" ]; then
    echo "Couldn't find CrateCut.vst3 or CrateCut.component next to this installer."
    echo "Keep 'Install CrateCut.command' in the same folder as the plugins and try again."
    failed=1
    finish
fi

install_bundle "CrateCut.vst3"      "$VST3_DIR"
install_bundle "CrateCut.component" "$AU_DIR"
install_bundle "CrateCut.app"       "$APP_DIR"

# Make macOS rescan Audio Units on next host launch.
killall -9 AudioComponentRegistrar >/dev/null 2>&1

echo
if [ "$failed" -eq 0 ]; then
    echo "${bold}Done.${normal}"
    echo
    echo "In FL Studio: restart it, then Options → Manage plugins → Find installed plugins."
    echo "CrateCut shows up under Generators."
else
    echo "${bold}Something went wrong${normal} - see the ✗ lines above."
fi
finish
