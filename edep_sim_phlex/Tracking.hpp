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

// edep_sim_phlex/Tracking.hpp
//
// The edep-sim FUNCTION node (ddm-4nd.11): a Phlex transform callable.  It
// consumes one HepMC3::GenEvent, drives exactly one Geant4 event through
// edep-sim, and returns the native edep-sim event summary (TG4Event).
//
// This is the SERVICE-BASED implementation: the thread-affinity discipline
// (dedicated Geant4 thread, construct/beamOn/destroy all on it) is delegated to
// edep-sim's reusable EDepSim::TrackingService.  HepMC3 -> Geant4 conversion is
// done faithfully by installing our own GenEventKine as the service's primary
// generator (the custom-generator path), so the kinematics go straight from
// HepMC3::GenEvent to G4PrimaryVertex without a lossy TG4PrimaryVertex hop.
//
// The public interface matches the original (now `TrackingG4`), so the Phlex
// module (modules/tracking.cpp) is unchanged.
//
// Config keys (same as TrackingG4):
//   physics_list (string, optional): Geant4 reference physics list.
//   gdml         (string, optional): detector GDML file.
//   macro        (string, optional): inline Geant4 macro text (physics tunings).

#include "phlex/configuration.hpp"

#include <memory>
#include <mutex> // std::once_flag

namespace HepMC3 {
    class GenEvent;
}
class TG4Event;

namespace edep_sim_phlex {

    class Tracking {
    public:
        explicit Tracking(phlex::configuration const& config);
        ~Tracking();

        Tracking(Tracking const&) = delete;
        Tracking& operator=(Tracking const&) = delete;

        // One input GenEvent -> one Geant4 event -> TG4Event summary.  Marshals
        // the work to the service's dedicated Geant4 thread and blocks for it.
        TG4Event operator()(HepMC3::GenEvent const& ge);

    private:
        void ensure_started(); // create + initialize the service, once

        struct Impl;                  // holds the service + borrowed generator
        std::unique_ptr<Impl> impl_;
        std::once_flag started_;
    };

} // namespace edep_sim_phlex
