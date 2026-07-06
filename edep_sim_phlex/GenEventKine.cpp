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

// edep_sim_phlex/GenEventKine.cpp

#include "edep_sim_phlex/GenEventKine.hpp"

#include "edep_sim_phlex/Convert.hpp"

#include <HepMC3/GenEvent.h>

#include <G4Event.hh>
#include <G4ios.hh>

namespace edep_sim_phlex {

    GenEventKine::GenEventKine() = default;
    GenEventKine::~GenEventKine() = default;

    void GenEventKine::feed_genevent(HepMC3::GenEvent const& ge)
    {
        current_ = &ge; // non-owning; valid for the current transform call
    }

    void GenEventKine::GeneratePrimaryVertex(G4Event* event)
    {
        if (!current_) {
            // The node is expected to feed exactly one GenEvent before each
            // beamOn.  Without one we produce an empty event; edep-sim's
            // fAllowEmptyEvents handling then decides what to do.
            G4cerr << "edep_sim_phlex::GenEventKine: no GenEvent fed before beamOn;"
                   << " generating an empty event." << G4endl;
            return;
        }
        append_primaries(*current_, event);
    }

} // namespace edep_sim_phlex
