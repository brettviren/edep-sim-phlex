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

// edep_sim_phlex/Observables.cpp
//
// TG4Event -> edep_arrow rows shim.  The edep.* Arrow schemas, table builders
// and read facades live in the pure-Arrow edep-arrow package (ddm-69y.2);
// this file only unpacks the ROOT-side TG4Event into the neutral row structs
// and applies the edep-sim quanta model.

#include "edep_sim_phlex/Observables.hpp"
#include "edep_sim_phlex/Data.hpp"

#include "edep_arrow/Photons.hpp"
#include "edep_arrow/Segments.hpp"

#include "TG4Event.h"

#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>

namespace edep_sim_phlex {

    namespace {

        // Recombination constant: mean energy per quantum in liquid argon.
        // 19.5 eV expressed in MeV (edep-sim / CLHEP energy unit).
        constexpr double kWQuanta_MeV = 19.5e-6;

        // One row per TG4HitSegment across all sensitive detectors.
        std::vector<edep_arrow::Segment> segment_rows(const TG4Event& summary)
        {
            // Track id -> PDG code, for labeling each segment with its
            // primary_id's particle type.  (Trajectories are nominally indexed
            // by TrackId but a map avoids relying on that invariant.)
            std::unordered_map<int, std::int32_t> track_pdg;
            track_pdg.reserve(summary.Trajectories.size());
            for (auto const& traj : summary.Trajectories) {
                track_pdg.emplace(traj.GetTrackId(), traj.GetPDGCode());
            }

            std::vector<edep_arrow::Segment> rows;
            for (auto const& [sd, segments] : summary.SegmentDetectors) {
                for (auto const& seg : segments) {
                    edep_arrow::Segment row;
                    row.sd = sd;
                    row.energy_deposit = seg.EnergyDeposit;       // MeV
                    row.secondary_deposit = seg.SecondaryDeposit; // MeV (scintillation part)

                    // Quanta -> N_ph (scintillation) and N_e (ionization electrons).
                    // Guard energy<=0 (no deposit -> no quanta).  SecondaryDeposit
                    // already carries the fluctuation, so no re-fluctuation here.
                    const double energy = row.energy_deposit;
                    const double n_q = energy > 0.0 ? energy / kWQuanta_MeV : 0.0;
                    row.n_photons = energy > 0.0 ? n_q * row.secondary_deposit / energy : 0.0;
                    row.n_electrons = n_q - row.n_photons;

                    row.track_length = seg.TrackLength;
                    row.primary_id = seg.PrimaryId;
                    // pdg shares primary_id's heuristic status (ddm-q3y): a
                    // segment may fold several tracks' deposits.
                    if (auto it = track_pdg.find(seg.PrimaryId); it != track_pdg.end()) {
                        row.pdg = it->second;
                    }
                    row.start_x = seg.Start.X();
                    row.start_y = seg.Start.Y();
                    row.start_z = seg.Start.Z();
                    row.start_t = seg.Start.T();
                    row.stop_x = seg.Stop.X();
                    row.stop_y = seg.Stop.Y();
                    row.stop_z = seg.Stop.Z();
                    row.stop_t = seg.Stop.T();

                    // contributors: the TrackId of every trajectory folded into
                    // this segment (delta-rays etc.) -- truth-association keys.
                    row.contributors.assign(seg.Contrib.begin(), seg.Contrib.end());

                    rows.push_back(std::move(row));
                }
            }
            return rows;
        }

        // One row per TG4PhotonHit across all photon detectors.
        std::vector<edep_arrow::Photon> photon_rows(const TG4Event& summary)
        {
            std::vector<edep_arrow::Photon> rows;
            for (auto const& [sd, hits] : summary.PhotonDetectors) {
                for (auto const& hit : hits) {
                    edep_arrow::Photon row;
                    row.sd = sd;
                    row.energy = hit.EnergyDeposit;
                    row.process = hit.Process;
                    row.primary_id = hit.PrimaryId;
                    row.start_x = hit.Start.X();
                    row.start_y = hit.Start.Y();
                    row.start_z = hit.Start.Z();
                    row.start_t = hit.Start.T();
                    row.stop_x = hit.Stop.X();
                    row.stop_y = hit.Stop.Y();
                    row.stop_z = hit.Stop.Z();
                    row.stop_t = hit.Stop.T();
                    rows.push_back(std::move(row));
                }
            }
            return rows;
        }

    } // namespace

    phlex_arrow::TableGroup to_observables(const TG4Event& summary)
    {
        // The builders validate the tables (ValidateFull) before returning.
        phlex_arrow::TableGroup group;
        group.type = kObservablesType;
        group.members[kSegmentsMember] = edep_arrow::to_table(segment_rows(summary));
        group.members[kPhotonsMember] = edep_arrow::to_table(photon_rows(summary));
        return group;
    }

} // namespace edep_sim_phlex
