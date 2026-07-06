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

// edep_sim_phlex/Convert.cpp

#include "edep_sim_phlex/Convert.hpp"

#include <HepMC3/FourVector.h>
#include <HepMC3/GenEvent.h>
#include <HepMC3/GenParticle.h>
#include <HepMC3/GenVertex.h>
#include <HepMC3/Units.h>

#include <G4Event.hh>
#include <G4PhysicalConstants.hh> // CLHEP::c_light
#include <G4PrimaryParticle.hh>
#include <G4PrimaryVertex.hh>
#include <G4SystemOfUnits.hh> // CLHEP::mm, ::MeV, ::ns

namespace {

    // HepMC/NuHepMC particle status: 1 == undecayed physical (final-state)
    // particle.  Only these are handed to Geant4 to track.
    constexpr int kFinalState = 1;

    // A G4PrimaryParticle built from a HepMC3 particle's PDG id and 4-momentum,
    // with momentum/energy scaled from the event's momentum unit to MeV.
    G4PrimaryParticle* make_g4_particle(const HepMC3::GenParticle& p, double p_to_MeV)
    {
        const HepMC3::FourVector& mom = p.momentum();
        return new G4PrimaryParticle(p.pid(),
                                     mom.px() * p_to_MeV * CLHEP::MeV,
                                     mom.py() * p_to_MeV * CLHEP::MeV,
                                     mom.pz() * p_to_MeV * CLHEP::MeV,
                                     mom.e() * p_to_MeV * CLHEP::MeV);
    }

} // namespace

namespace edep_sim_phlex {

    void append_primaries(const HepMC3::GenEvent& ge, G4Event* g4event)
    {
        // Scale factors from the event's declared units to the CLHEP system.
        // HepMC3 has no conversion_factor(); Units::convert(v, from, to) scales v
        // in place, so derive each factor by converting a unit value.
        double x_to_mm = 1.0;
        HepMC3::Units::convert(x_to_mm, ge.length_unit(), HepMC3::Units::MM);
        double p_to_MeV = 1.0;
        HepMC3::Units::convert(p_to_MeV, ge.momentum_unit(), HepMC3::Units::MEV);

        for (const auto& vtx : ge.vertices()) {
            if (!vtx) continue;

            // Collect the final-state particles produced at this vertex.
            std::vector<G4PrimaryParticle*> primaries;
            for (const auto& part : vtx->particles_out()) {
                if (part && part->status() == kFinalState) {
                    primaries.push_back(make_g4_particle(*part, p_to_MeV));
                }
            }
            if (primaries.empty()) continue;

            const HepMC3::FourVector& pos = vtx->position();

            // HepMC3 stores the vertex position 4-vector time component as c*t in
            // the event's length unit; divide by c_light to obtain a G4 time.
            // TODO(ddm-4nd.3): validate this against the NuHepMC time convention
            // once the input producers (gun/mixing nodes) are settled.
            auto* g4vtx = new G4PrimaryVertex(pos.x() * x_to_mm * CLHEP::mm,
                                              pos.y() * x_to_mm * CLHEP::mm,
                                              pos.z() * x_to_mm * CLHEP::mm,
                                              pos.t() * x_to_mm * CLHEP::mm / CLHEP::c_light);

            for (auto* g4part : primaries) {
                g4vtx->SetPrimary(g4part);
            }
            g4event->AddPrimaryVertex(g4vtx);
        }
    }

} // namespace edep_sim_phlex
