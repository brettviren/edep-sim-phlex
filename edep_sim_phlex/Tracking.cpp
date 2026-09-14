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

// edep_sim_phlex/Tracking.cpp
//
// Service-based tracking node: the thread affinity is owned by
// EDepSim::TrackingService, and HepMC3 -> Geant4 conversion is done by our
// GenEventKine installed as the service's custom primary generator.

#include "edep_sim_phlex/Tracking.hpp"

#include "edep_sim_phlex/GenEventKine.hpp"

#include "EDepSimTrackingService.hh"
#include "TG4Event.h"

#include <HepMC3/GenEvent.h>

#include <mutex>
#include <sstream>
#include <string>
#include <vector>

namespace {

    // Split inline Geant4 macro text into one command per line.  The service's
    // initialize() applies each line and skips blanks/'#' comments, so we only
    // need to break the blob apart here.
    std::vector<std::string> split_lines(std::string const& text)
    {
        std::vector<std::string> lines;
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            lines.push_back(line);
        }
        return lines;
    }

} // namespace

namespace edep_sim_phlex {

    struct Tracking::Impl {
        // Configuration captured at construction.
        std::string physics_list;
        std::string gdml;
        std::string macro;

        std::unique_ptr<EDepSim::TrackingService> service;

        // The generator that converts each GenEvent to G4 primaries.  It is
        // OWNED by the service's worker (constructed on the Geant4 thread by the
        // factory below); we only borrow the pointer to feed it per event.
        GenEventKine* kine = nullptr;

        // Serializes feed(ge) + simulate() into one atomic pair, so the event a
        // simulate() tracks is always the one just fed even if the node is ever
        // driven concurrently (the module is concurrency::serial today, but the
        // service itself is re-entrant).
        std::mutex feed_mutex;

        // Fallback RNG seed source.  simulate() reseeds from the event id and a
        // zero seed is rejected by Geant4 (CLHEP), so when a GenEvent carries no
        // event number (0, e.g. from a particle gun) we seed from this instead.
        unsigned long fallback_seed = 0;
    };

    Tracking::Tracking(phlex::configuration const& config)
      : impl_(std::make_unique<Impl>())
    {
        impl_->physics_list = config.get<std::string>("physics_list", std::string{});
        impl_->gdml = config.get<std::string>("gdml", std::string{});
        impl_->macro = config.get<std::string>("macro", std::string{});
    }

    Tracking::~Tracking() = default; // ~Impl destroys the service (joins worker)

    void Tracking::ensure_started()
    {
        std::call_once(started_, [this] {
            impl_->service = std::make_unique<EDepSim::TrackingService>();

            // The factory runs ON the Geant4 thread, so GenEventKine (and any G4
            // state its construction touches) is bound to that thread.  It
            // publishes its pointer so operator() can feed it.  initialize()
            // blocks until this has run, so impl_->kine is set on return.
            auto holder = std::make_shared<GenEventKine*>(nullptr);
            impl_->service->initialize(
              impl_->physics_list, impl_->gdml,
              EDepSim::GeneratorFactory([holder] {
                  auto* g = new GenEventKine();
                  *holder = g;
                  return g;
              }),
              split_lines(impl_->macro));
            impl_->kine = *holder;
        });
    }

    TG4Event Tracking::operator()(HepMC3::GenEvent const& ge)
    {
        ensure_started();

        // Feed this event and run exactly one Geant4 event on the service's
        // dedicated thread, blocking for the result.  The id seeds the RNG (a
        // given id reproduces a given event): use the GenEvent's event number
        // when it carries one, else a monotonic counter so the seed is never
        // zero (which Geant4 rejects) and stays reproducible by input order.
        std::lock_guard<std::mutex> lk(impl_->feed_mutex);
        impl_->kine->feed_genevent(ge);
        unsigned long id = static_cast<unsigned long>(ge.event_number());
        if (id == 0) {
            id = ++impl_->fallback_seed;
        }
        auto event = impl_->service->simulate(id);
        return *event;
    }

} // namespace edep_sim_phlex
