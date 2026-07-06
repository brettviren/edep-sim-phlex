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

// edep_sim_phlex/SummaryPersistency.hpp
//
// A minimal EDepSim::PersistencyManager subclass that exposes the TG4Event
// summary.
//
// edep-sim's base EDepSim::PersistencyManager::Store() fills its protected
// `fEventSummary` (via UpdateSummaries) with the primaries, trajectories, and
// hit/photon detectors of each event.  However its GetEventSummary() /
// GetPrimaries() / ... accessors are DECLARED in the header but NOT defined in
// libedepsim (undefined-symbol at link/run time -- an edep-sim bug).  The header
// documents `fEventSummary` as protected precisely so derived classes can read
// it, so we subclass to expose it.
//
// Using the BASE (not Root) persistency manager means Store() summarizes into
// fEventSummary but writes no ROOT file and builds no TGeo (decisions Q1/Q5).

#include "EDepSimPersistencyManager.hh"
#include "TG4Event.h"

namespace edep_sim_phlex {

    class SummaryPersistency : public EDepSim::PersistencyManager {
    public:
        // The event summary filled by the (inherited) Store() during beamOn.
        TG4Event const& summary() const { return fEventSummary; }
    };

} // namespace edep_sim_phlex
