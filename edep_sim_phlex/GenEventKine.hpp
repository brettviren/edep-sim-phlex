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

// edep_sim_phlex/GenEventKine.hpp
//
// A Geant4 primary generator driven by an in-memory HepMC3 GenEvent.
//
// GenEventKine implements BOTH interfaces:
//   * G4VPrimaryGenerator -- so edep-sim's UserPrimaryGeneratorAction can call
//     GeneratePrimaryVertex(G4Event*) once per beamOn (decision Q7 / ddm-4nd.9,
//     "approach 4-A": the node constructs this and injects it via AddGenerator).
//     Note this is a *direct* G4VPrimaryGenerator, NOT an EDepSim::PrimaryGenerator
//     composite: the GenEvent already carries vertex positions/times, so
//     edep-sim's position/time/count sub-generators are bypassed entirely.
//   * IGenEventSink -- so the node can feed the GenEvent to track.
//
// GenEvent -> Geant4 conversion is delegated to edep_sim_phlex::append_primaries
// (Convert.hpp); it is not implemented in the body of this class.

#include "edep_sim_phlex/IGenEventSink.hpp"

#include <G4VPrimaryGenerator.hh>

namespace HepMC3 {
    class GenEvent;
}
class G4Event;

namespace edep_sim_phlex {

    class GenEventKine : public G4VPrimaryGenerator, public IGenEventSink {
    public:
        GenEventKine();
        ~GenEventKine() override;

        // IGenEventSink: set the event to be tracked by the next beamOn.
        void feed_genevent(HepMC3::GenEvent const& ge) override;

        // G4VPrimaryGenerator: build the primaries for one Geant4 event.
        void GeneratePrimaryVertex(G4Event* event) override;

    private:
        // The event most recently fed; consulted by the next GeneratePrimaryVertex.
        // Non-owning: it points at the current Phlex input product, which is alive
        // for the whole transform call (see IGenEventSink).
        HepMC3::GenEvent const* current_ = nullptr;
    };

} // namespace edep_sim_phlex
