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

// edep_sim_phlex/Convert.hpp
//
// Free functions that convert HepMC3 kinematics into Geant4 primary objects.
// GenEventKine delegates to these rather than implementing the conversion
// inline, so the conversion policy (unit handling, final-state selection,
// NuHepMC status-code interpretation) lives in one testable place.

namespace HepMC3 {
    class GenEvent;
}
class G4Event;

namespace edep_sim_phlex {

    // Append primary vertices and particles from a HepMC3 GenEvent onto a
    // Geant4 event.
    //
    // Behaviour:
    //  - One G4PrimaryVertex is created per HepMC3 GenVertex that has outgoing
    //    final-state particles, positioned at that vertex.
    //  - Only final-state particles (HepMC/NuHepMC status == 1) are attached as
    //    G4PrimaryParticles for Geant4 to track.  Beam (4), target (20) and
    //    struck-nucleon (21) particles are informational and are not tracked,
    //    matching edep-sim's existing rooTracker behaviour.
    //  - Units are read from the GenEvent (length_unit / momentum_unit) and
    //    converted to the Geant4/CLHEP system (mm, MeV, ns).
    void append_primaries(const HepMC3::GenEvent& ge, G4Event* g4event);

} // namespace edep_sim_phlex
