#!/usr/bin/env bash
#
# Run the GenEventGun -> edep-sim tracking -> observer smoke test and check for a
# success indicator (non-zero tracking segments).
#
# Assumes the gcc15 Spack view and that hepmc-phlex + edep-sim-phlex have been
# built via ./umbrella gcc15 build (into builds/envs/gcc15/<pkg>/).
#
# Usage:  devel/edep-sim-phlex/test/run-smoke.sh
#
# Success is judged by the tracking OUTPUT (non-zero segments), not by phlex's
# exit code -- so it stays correct even if a shutdown-time issue reappears.
set -uo pipefail

# Repo root = three levels up from this script (devel/edep-sim-phlex/test/).
here="$(cd "$(dirname "$(readlink -f "$0")")" && pwd)"
root="$(cd "$here/../../.." && pwd)"

view="$root/extern/envs/gcc15/view"
edepsim_root="$(echo "$root"/extern/spack/opt/spack/*/edepsim-*/ | awk '{print $1}')"

export PHLEX_PLUGIN_PATH="$root/builds/envs/gcc15/hepmc-phlex:$root/builds/envs/gcc15/edep-sim-phlex:$view/lib"
export EDEPSIM_ROOT="$edepsim_root"

log="$(mktemp)"
echo $log
"$view/bin/phlex" -c "$here/smoke-gun-tracking.jsonnet" > "$log" 2>&1 || true
grep -E '\[edep-smoke\]' "$log" || true

echo "----------------------------------------------------------------"
if grep -qE '\[edep-smoke\].*segments=[1-9]' "$log"; then
    echo "SMOKE TEST PASSED: non-zero tracking segments produced."
    exit 0
else
    echo "SMOKE TEST FAILED: no non-zero-segment [edep-smoke] line found."
    exit 1
fi
