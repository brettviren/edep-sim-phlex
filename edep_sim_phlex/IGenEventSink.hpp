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

// edep_sim_phlex/IGenEventSink.hpp
//
// Abstract sink for feeding primary kinematics (a HepMC3 GenEvent) into a
// Geant4 primary generator.  The Phlex node holds an IGenEventSink* and calls
// feed_genevent() immediately before it drives one /run/beamOn.
//
// Passing semantics: `const HepMC3::GenEvent&`, NON-owning.  Phlex owns the
// GenEvent product and delivers it to the node as a `const&` that is alive for
// the whole transform call -- which includes the synchronous beamOn where the
// generator reads it.  So the sink only needs to borrow it; it must not retain
// the reference past the current call, and the caller must feed before beamOn.
// (A shared_ptr would be wrong here: it would force a deep copy of HepMC3's
// particle/vertex graph, since Phlex hands us a reference, not a shared_ptr.)

namespace HepMC3 {
    class GenEvent;
}

namespace edep_sim_phlex {

    class IGenEventSink {
    public:
        virtual ~IGenEventSink() = default;

        /// Provide the GenEvent to be tracked by the next generated Geant4 event.
        /// The reference must outlive the subsequent GeneratePrimaryVertex call
        /// (it does: it is the current Phlex input product).
        virtual void feed_genevent(HepMC3::GenEvent const& ge) = 0;
    };

} // namespace edep_sim_phlex
