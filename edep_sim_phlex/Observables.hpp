/*
 * This file is part of the DUNE Xerosere project.
 *
 * Copyright (c) 2026, Brookhaven Science Associates, LLC
 *
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0
 */

#pragma once

// edep_sim_phlex/Observables.hpp
//
// Q5 OBSERVABLES layer (ddm-4nd.5 / ddm-6rn): convert an edep-sim TG4Event
// summary into the canonical Arrow observables product.
//
// The observables are the "always emitted" layer of the three-layer Q5 output
// (observables -> Arrow; MC-truth -> HepMC; associations -> Arrow).  Only the
// observables are built here; the truth and association layers are follow-on
// work.
//
// Product: one phlex_arrow::TableGroup, type "edep.observables", with members:
//   "segments" (edep.segments) -- one row per TG4HitSegment across all sensitive
//       detectors.  Carries the IONIZATION-ELECTRON count N_e and the
//       SCINTILLATION-PHOTON count N_ph derived from the edep-sim recombination
//       model (see below), NOT raw energy -- N_e is what the downstream drift /
//       detector-response sims consume.  The line-segment (Start->Stop) form is
//       canonical; point-depo sampling is a separate downstream node.
//   "photons" (edep.photons) -- one row per TG4PhotonHit across all photon
//       detectors (optical output).
//
// Recombination (documented on TG4HitSegment; edep-sim's ionization model has
// already applied the quenching and filled SecondaryDeposit, so DO NOT
// re-fluctuate):
//     N_q  = EnergyDeposit / (19.5 eV)
//     N_ph = N_q * SecondaryDeposit / EnergyDeposit
//     N_e  = N_q - N_ph
//
// Units are edep-sim / CLHEP native (length mm, energy MeV, time ns) and are
// declared in each table's Arrow schema metadata.

#include "phlex_arrow_common/TableGroup.hpp"

class TG4Event;

namespace edep_sim_phlex {

    // Build the "edep.observables" TableGroup (segments + photons) from one
    // edep-sim event summary.  Throws std::runtime_error on any Arrow failure.
    phlex_arrow::TableGroup to_observables(const TG4Event& summary);

} // namespace edep_sim_phlex
