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
// edep-sim, and returns the native edep-sim event summary (TG4Event) as its
// product.  Turning that TG4Event into the Q5 observables (Arrow tables) is a
// SEPARATE downstream Phlex node (see modules/observables.cpp + Observables.hpp)
// so this node stays a pure Geant4/edep-sim concern with no Arrow dependency.
//
// THREAD AFFINITY: Geant4's sequential G4RunManager is thread-affine -- the
// navigator/world it builds live in G4ThreadLocal state.  Phlex runs a node's
// operator() on whatever TBB pool thread is free, which varies across calls over
// the job.  So this node owns a single dedicated "G4 thread" that performs init
// and every beamOn; operator() marshals each event to it and blocks for the
// result.  The dedicated thread is a blocking hand-off, not added concurrency
// (the calling TBB thread parks while the G4 thread runs), so there is no
// oversubscription.  Assumes serial invocation (concurrency::serial): at most one
// operator() in flight.
//
// The G4 thread, its handshake state, and the Geant4 objects live in a heap
// G4Worker (see the .cpp).  Crucially, the geometry is created, used AND
// DESTROYED on that one thread: init, every beamOn, and -- at stop -- emptying
// Geant4's global geometry stores (G4PhysicalVolumeStore::Clean() etc.) all run
// there.  Those stores are otherwise torn down by MAIN-thread static destructors
// at process exit, which would delete worker-built volumes cross-thread and crash
// (in ~G4PVPlacement/GetRotation); emptying them on the worker first leaves the
// static destructors nothing to do.  ~Tracking signals the thread to stop and
// joins it, so this cleanup completes before the object goes away.

#include "phlex/configuration.hpp"

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
        // the work to the dedicated G4 thread and blocks until it completes.
        TG4Event operator()(HepMC3::GenEvent const& ge);

    private:
        struct G4Worker; // defined in the .cpp; owns the G4 thread + Geant4 objects

        void ensure_started(); // create the worker + start its thread, once

        phlex::configuration config_;
        std::once_flag started_;
        G4Worker* worker_ = nullptr; // heap-allocated; stopped, joined and deleted in ~Tracking
    };

} // namespace edep_sim_phlex
