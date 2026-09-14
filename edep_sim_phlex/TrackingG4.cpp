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

// edep_sim_phlex/TrackingG4.cpp

#include "edep_sim_phlex/TrackingG4.hpp"

#include "edep_sim_phlex/GenEventKine.hpp"
#include "edep_sim_phlex/SummaryPersistency.hpp"

#include "EDepSimCreateRunManager.hh"
#include "EDepSimUserPrimaryGeneratorAction.hh"
#include "TG4Event.h"

#include <G4GeometryManager.hh>
#include <G4LogicalVolumeStore.hh>
#include <G4PhysicalVolumeStore.hh>
#include <G4RunManager.hh>
#include <G4SolidStore.hh>
#include <G4UImanager.hh>

#include <condition_variable>
#include <exception>
#include <memory>
#include <mutex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>

namespace {

    // Apply inline Geant4 macro text (as delivered in Phlex config, Q4) one
    // command per line, skipping blank lines and '#' comments.
    void apply_macro_text(G4UImanager& ui, const std::string& text)
    {
        std::istringstream iss(text);
        std::string line;
        while (std::getline(iss, line)) {
            const auto first = line.find_first_not_of(" \t\r\n");
            if (first == std::string::npos) continue; // blank
            if (line[first] == '#') continue;         // comment
            ui.ApplyCommand(line);
        }
    }

} // namespace

namespace edep_sim_phlex {

    // Owns the dedicated Geant4 thread, the single-slot request/response handshake,
    // and the Geant4 objects.  The Geant4 objects are created, used AND destroyed
    // only on this one thread -- init, every beamOn, and the final teardown all run
    // there.  That single-thread discipline is what makes it safe: G4RunManager is
    // thread-affine (G4ThreadLocal navigator/world), so building on one thread and
    // running/destroying on another corrupts G4 and crashes (e.g. in the
    // G4PhysicalVolumeStore teardown at process exit).
    struct TrackingG4::G4Worker {
        std::thread thread;
        std::mutex mtx;
        std::condition_variable cv;
        HepMC3::GenEvent const* input = nullptr; // guarded by mtx
        TG4Event result;                         // guarded by mtx
        bool job_ready = false;
        bool result_ready = false;
        bool stop = false;
        std::exception_ptr init_exception; // set if initialize() threw
        bool init_failed = false;

        phlex::configuration config;

        // Geant4 objects: created, used and destroyed only on `thread`.
        std::unique_ptr<GenEventKine> kine;
        std::unique_ptr<G4RunManager> run_manager;
        std::unique_ptr<SummaryPersistency> persistency;
        EDepSim::UserPrimaryGeneratorAction* action = nullptr; // owned by run_manager

        void loop();
        void initialize();
        TG4Event run_one(HepMC3::GenEvent const& ge);
    };

    void TrackingG4::G4Worker::loop()
    {
        // All Geant4 setup + every beamOn happen on THIS thread, so the
        // G4ThreadLocal navigator/world belong to the thread that runs them.
        try {
            initialize();
        } catch (...) {
            std::lock_guard<std::mutex> lk(mtx);
            init_exception = std::current_exception();
            init_failed = true;
        }

        for (;;) {
            std::unique_lock<std::mutex> lk(mtx);
            cv.wait(lk, [this] { return job_ready || stop; });
            if (stop) break;

            HepMC3::GenEvent const* in = input;
            job_ready = false;

            TG4Event out;
            if (!init_failed && in) {
                lk.unlock(); // run Geant4 without holding the lock
                out = run_one(*in);
                lk.lock();
            }

            result = std::move(out);
            result_ready = true;
            cv.notify_one();
        }

        // Empty Geant4's GLOBAL geometry stores HERE, on the worker thread that
        // BUILT the volumes, so their (main-thread) process-exit static destructors
        // find nothing to tear down.  Otherwise the main-thread static destructor
        // of the global G4PhysicalVolumeStore would delete worker-built volumes
        // cross-thread -> crash in ~G4PVPlacement/GetRotation.  This completes the
        // "construction + use + destruction all on the worker thread" discipline.
        // OpenGeometry() is required first because the geometry is closed
        // (optimized) after /run/initialize.
        G4GeometryManager::GetInstance()->OpenGeometry();
        G4PhysicalVolumeStore::Clean();
        G4LogicalVolumeStore::Clean();
        G4SolidStore::Clean();

        // The run manager itself is still leaked: ~G4RunManager is separately
        // crash-prone at teardown and one node lives for the whole process.
        (void)kine.release();
        (void)run_manager.release();
        (void)persistency.release();
    }

    void TrackingG4::G4Worker::initialize()
    {
        // ---- one-time configuration (replaces app/edepSim.cc CLI handling) ----
        auto const physics_list = config.get<std::string>("physics_list", std::string{});
        auto const gdml = config.get<std::string>("gdml", std::string{});
        auto const macro = config.get<std::string>("macro", std::string{});

        // Build the edep-sim run manager.  The physics list is a Geant4
        // construction-time choice (Q4), not a runtime macro command.
        run_manager.reset(EDepSim::CreateRunManager(physics_list));

        // Install the (no-ROOT-output) persistency manager whose Store() fills a
        // TG4Event summary we read after each beamOn (Q5).  NB: this does NOT stop
        // edep-sim building a TGeoManager during /edep/update (ddm-4nd.1).
        persistency = std::make_unique<SummaryPersistency>();

        // Approach 4-A (ddm-4nd.9): construct our generator and inject it.  The
        // action is owned by the run manager; we borrow it.
        // GetUserPrimaryGeneratorAction() is const, hence the const_cast.
        kine = std::make_unique<GenEventKine>();
        action = const_cast<EDepSim::UserPrimaryGeneratorAction*>(
          static_cast<const EDepSim::UserPrimaryGeneratorAction*>(
            run_manager->GetUserPrimaryGeneratorAction()));
        if (!action) {
            throw std::runtime_error(
              "edep_sim_phlex::TrackingG4: run manager has no UserPrimaryGeneratorAction");
        }
        action->AddGenerator(kine.get());

        auto* ui = G4UImanager::GetUIpointer();

        // Geometry: build the Geant4 geometry directly from GDML (Q1).
        // TODO(ddm-4nd.1/.2): source the GDML via a Phlex geometry resource.
        if (!gdml.empty()) {
            ui->ApplyCommand("/edep/gdml/read " + gdml);
        }

        // edep-sim defaults: ionization model, trajectory-save thresholds, etc.
        ui->ApplyCommand("/edep/control edepsim-defaults 1.0");

        // User-supplied physics-tuning macro text (Q4).
        // TODO(ddm-4nd.4): validate against reserved-command rules; support importstr.
        if (!macro.empty()) {
            apply_macro_text(*ui, macro);
        }

        // Initialize geometry + physics (triggers /run/initialize).
        ui->ApplyCommand("/edep/update");
    }

    TG4Event TrackingG4::G4Worker::run_one(HepMC3::GenEvent const& ge)
    {
        // Feed the event, then run exactly one Geant4 event (the node owns beamOn; Q4).
        kine->feed_genevent(ge);
        G4UImanager::GetUIpointer()->ApplyCommand("/run/beamOn 1");

        // The persistency manager's Store() has filled the TG4Event summary; return
        // a copy as this node's product.  Converting it to the Q5 observables (Arrow
        // tables) is a separate downstream node (modules/observables.cpp).
        return persistency->summary();
    }

    TrackingG4::TrackingG4(phlex::configuration const& config) : config_(config) {}

    TrackingG4::~TrackingG4()
    {
        if (worker_) {
            if (worker_->thread.joinable()) {
                {
                    std::lock_guard<std::mutex> lk(worker_->mtx);
                    worker_->stop = true;
                }
                worker_->cv.notify_one();
                worker_->thread.join(); // waits for the G4 shutdown on its own thread
            }
            delete worker_;
        }
    }

    void TrackingG4::ensure_started()
    {
        std::call_once(started_, [this] {
            worker_ = new G4Worker();
            worker_->config = config_;
            worker_->thread = std::thread([w = worker_] { w->loop(); });
        });
    }

    TG4Event TrackingG4::operator()(HepMC3::GenEvent const& ge)
    {
        ensure_started();

        TG4Event out;
        std::exception_ptr ex;
        {
            std::unique_lock<std::mutex> lk(worker_->mtx);
            worker_->input = &ge; // safe: the caller blocks below, so ge outlives the job
            worker_->job_ready = true;
            worker_->cv.notify_one();
            worker_->cv.wait(lk, [w = worker_] { return w->result_ready; });
            worker_->result_ready = false;
            out = std::move(worker_->result);
            ex = worker_->init_exception; // set only if Geant4 init failed
        }
        if (ex) std::rethrow_exception(ex);
        return out;
    }

} // namespace edep_sim_phlex
