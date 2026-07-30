#!/usr/bin/env bash
#
# End-to-end smoke test: GenEventGun -> edep-sim tracking -> observables -> HDF5.
#
# Extends run-smoke.sh by adding the phlex-arrow-hdf output node, so it exercises
# the FULL Q5 chain (ddm-6rn): TG4Event -> Arrow edep.observables -> HDF5 file.
# Success requires BOTH non-zero tracking segments AND a non-empty HDF5 file that
# contains the segments/photons tables.
#
# Assumes the gcc15 Spack view and that hepmc-phlex, edep-sim-phlex and
# phlex-arrow-hdf have been built via ./umbrella gcc15 build (into
# builds/envs/gcc15/<pkg>/).
#
# Usage:  devel/edep-sim-phlex/test/run-smoke-hdf.sh
set -uo pipefail

# Repo root = three levels up from this script (devel/edep-sim-phlex/test/).
here="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
root="$(cd "$here/../../.." && pwd)"

view="$root/extern/envs/gcc15/view"
edepsim_root="$(echo "$root"/extern/spack/opt/spack/*/edepsim-*/ | awk '{print $1}')"

export PHLEX_PLUGIN_PATH="$root/builds/envs/gcc15/hepmc-phlex:$root/builds/envs/gcc15/edep-sim-phlex:$root/builds/envs/gcc15/phlex-arrow-hdf:$view/lib"
export EDEPSIM_ROOT="$edepsim_root"

# Run in a scratch dir so the output .h5 does not litter the source tree.
workdir="$(mktemp -d)"
h5="$workdir/edep-observables.h5"
log="$workdir/run.log"
echo "workdir: $workdir"

( cd "$workdir" && "$view/bin/phlex" -c "$here/smoke-gun-tracking-hdf.jsonnet" > "$log" 2>&1 ) || true
grep -E '\[edep-smoke\]' "$log" || true

echo "----------------------------------------------------------------"
ok_seg=false; ok_h5=false
grep -qE '\[edep-smoke\].*segments=[1-9]' "$log" && ok_seg=true
# NB: capture h5ls output into a variable first; piping straight into `grep -q`
# makes grep close the pipe on the first match, SIGPIPE-kills h5ls, and with
# `set -o pipefail` that non-zero status would falsely fail the check.
h5_contents="$("$view/bin/h5ls" -r "$h5" 2>/dev/null)"
if ! printf '%s\n' "$h5_contents" | grep -q 'observables/segments/pdg'; then
    echo "SMOKE TEST FAILED: no segments/pdg dataset in $h5"
    exit 1
fi
# The gun fires a mu- so the dominant segment pdg must be 13.  (Same
# capture-first dance as h5ls above: grep -q + pipefail SIGPIPE-kills h5dump.)
pdg_path="$(printf '%s\n' "$h5_contents" | grep -om1 '/event/[^ ]*observables/segments/pdg')"
pdg_dump="$("$view/bin/h5dump" -d "$pdg_path" "$h5" 2>/dev/null)"
if ! printf '%s\n' "$pdg_dump" | grep -q '13'; then
    echo "SMOKE TEST FAILED: segments/pdg carries no muon (13) entries"
    exit 1
fi
if [ -s "$h5" ] && printf '%s\n' "$h5_contents" | grep -q 'observables/segments/n_electrons'; then
    ok_h5=true
    echo "HDF5 written: $h5 ($(stat -c%s "$h5") bytes), segments table present."
fi

if $ok_seg && $ok_h5; then
    echo "SMOKE TEST PASSED: observables tracked AND persisted to HDF5."
    exit 0
else
    echo "SMOKE TEST FAILED (segments=$ok_seg hdf5=$ok_h5).  See $log"
    exit 1
fi
