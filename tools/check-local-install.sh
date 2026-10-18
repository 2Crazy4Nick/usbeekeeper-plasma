#!/usr/bin/bash
# Read-only: run as the desktop user, without sudo.
set -eu

applet_id=io.github._2crazy4nick.usbeekeeper
qml_module=io/github/_2crazy4nick/usbeekeeper
data_root=${XDG_DATA_HOME:-$HOME/.local/share}
found=0
declare -A checked=()

report_copy() {
    local path=$1
    local description=$2
    if [[ -e "$path" && -z ${checked["$path"]+present} ]]; then
        checked["$path"]=1
        printf '%s: %s\n' "$description" "$path"
        found=1
    fi
}

report_copy "$data_root/plasma/plasmoids/$applet_id" "User applet shadows the RPM"
report_copy "$data_root/plasma/plasmoids/org.kde.whatcable" "Separate legacy WhatCable applet"

system_qml_root=$(qtpaths6 --query QT_INSTALL_QML)
declare -a import_roots=()
IFS=: read -r -a import_roots <<< "${QML_IMPORT_PATH:-}:${QML2_IMPORT_PATH:-}"
# These are earlier documented development locations, not automatic Qt paths.
import_roots+=("$HOME/.local/lib64/qml" "$HOME/.local/lib/qml")
for root in "${import_roots[@]}"; do
    [[ -n "$root" ]] || continue
    [[ ${root%/} != "${system_qml_root%/}" ]] || continue
    report_copy "${root%/}/$qml_module" "Manual QML module; check the Plasma session's QML_IMPORT_PATH"
    report_copy "${root%/}/org/kde/whatcable" "Separate legacy WhatCable QML module"
done

if (( found )); then
    printf '%s\n' \
        'Review these copies before using the RPM. Move obsolete copies to a backup outside the applet/import directories.' \
        'Nothing was changed. Plasma configuration and aliases must be kept.' \
        'The running Plasma session may have a different QML_IMPORT_PATH from this terminal.'
    exit 1
fi
printf '%s\n' 'No local copies found in the checked paths. Nothing was changed.'
