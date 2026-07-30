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

// edep_sim_phlex/Data.hpp
//
// Phlex product identifiers for this package.
//
// The node INPUT is HepMC3::GenEvent itself (decision Q3, ddm-4nd.3), emitted by
// value by an upstream producer (e.g. hepmc_phlex::GenEventGun) -- Phlex owns the
// value and delivers it as `HepMC3::GenEvent const&`, so no wrapper is needed.
//
// The node OUTPUT is the Q5 OBSERVABLES layer (ddm-4nd.5 / ddm-6rn): a single
// phlex_arrow::TableGroup (built by to_observables(), see Observables.hpp).  The
// MC-truth (HepMC) and associations layers are follow-on work.  These constants
// name the product's type marker and member tables so the producer and any
// consumer agree without duplicating string literals.

#include "edep_arrow/Schema.hpp"

namespace edep_sim_phlex {

    // The edep.* schema conventions are owned by the pure-Arrow edep-arrow
    // package (ddm-69y.2); re-expose the names this package's producer and
    // consumers use.
    using edep_arrow::kObservablesType; // TableGroup type marker
    using edep_arrow::kSegmentsMember;  // edep.segments table member
    using edep_arrow::kPhotonsMember;   // edep.photons table member

} // namespace edep_sim_phlex
