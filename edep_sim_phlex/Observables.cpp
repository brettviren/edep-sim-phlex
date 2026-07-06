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

#include "edep_sim_phlex/Observables.hpp"
#include "edep_sim_phlex/Data.hpp"

#include "TG4Event.h"

#include <arrow/api.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace edep_sim_phlex {

    namespace {

        // Throw on a bad arrow::Status; return the value of a Result.
        void ok(const arrow::Status& s, const char* what)
        {
            if (!s.ok()) {
                throw std::runtime_error(std::string("edep_sim_phlex observables: ") + what +
                                         ": " + s.ToString());
            }
        }

        template <typename T>
        std::shared_ptr<arrow::Array> finish(T& builder)
        {
            std::shared_ptr<arrow::Array> arr;
            ok(builder.Finish(&arr), "builder.Finish");
            return arr;
        }

        // Recombination constant: mean energy per quantum in liquid argon.
        // 19.5 eV expressed in MeV (edep-sim / CLHEP energy unit).
        constexpr double kWQuanta_MeV = 19.5e-6;

        // Schema metadata common to every edep.* observable table: the schema
        // name/version (matching the ecosystem's arrow.schema convention) plus the
        // CLHEP native units the columns are expressed in.
        std::shared_ptr<arrow::KeyValueMetadata> schema_metadata(const std::string& name)
        {
            return std::make_shared<arrow::KeyValueMetadata>(
              std::vector<std::string>{"arrow.schema", "arrow.schema.version",
                                       "edep.units.length", "edep.units.energy",
                                       "edep.units.time"},
              std::vector<std::string>{name, "1", "mm", "MeV", "ns"});
        }

        // ---- segments (edep.segments) ---------------------------------------
        //
        // One row per TG4HitSegment across all sensitive detectors.  The `sd`
        // column preserves the sensitive-detector name so a single flat table can
        // carry every detector's segments (map<sdname, vector<segment>>).
        std::shared_ptr<arrow::Table> build_segments(const TG4Event& summary)
        {
            auto* pool = arrow::default_memory_pool();

            arrow::StringBuilder sd_b(pool);
            arrow::DoubleBuilder n_electrons_b(pool);
            arrow::DoubleBuilder n_photons_b(pool);
            arrow::DoubleBuilder energy_deposit_b(pool);
            arrow::DoubleBuilder secondary_deposit_b(pool);
            arrow::DoubleBuilder track_length_b(pool);
            arrow::Int32Builder primary_id_b(pool);
            arrow::DoubleBuilder start_x_b(pool), start_y_b(pool), start_z_b(pool), start_t_b(pool);
            arrow::DoubleBuilder stop_x_b(pool), stop_y_b(pool), stop_z_b(pool), stop_t_b(pool);
            arrow::ListBuilder contributors_b(pool, std::make_shared<arrow::Int32Builder>(pool));
            auto* contributor_b = static_cast<arrow::Int32Builder*>(contributors_b.value_builder());

            for (auto const& [sd, segments] : summary.SegmentDetectors) {
                for (auto const& seg : segments) {
                    const double energy = seg.EnergyDeposit;    // MeV
                    const double secondary = seg.SecondaryDeposit; // MeV (scintillation part)

                    // Quanta -> N_ph (scintillation) and N_e (ionization electrons).
                    // Guard energy<=0 (no deposit -> no quanta).  SecondaryDeposit
                    // already carries the fluctuation, so no re-fluctuation here.
                    const double n_q = energy > 0.0 ? energy / kWQuanta_MeV : 0.0;
                    const double n_ph = energy > 0.0 ? n_q * secondary / energy : 0.0;
                    const double n_e = n_q - n_ph;

                    ok(sd_b.Append(sd), "segments.sd");
                    ok(n_electrons_b.Append(n_e), "segments.n_electrons");
                    ok(n_photons_b.Append(n_ph), "segments.n_photons");
                    ok(energy_deposit_b.Append(energy), "segments.energy_deposit");
                    ok(secondary_deposit_b.Append(secondary), "segments.secondary_deposit");
                    ok(track_length_b.Append(seg.TrackLength), "segments.track_length");
                    ok(primary_id_b.Append(seg.PrimaryId), "segments.primary_id");

                    ok(start_x_b.Append(seg.Start.X()), "segments.start_x");
                    ok(start_y_b.Append(seg.Start.Y()), "segments.start_y");
                    ok(start_z_b.Append(seg.Start.Z()), "segments.start_z");
                    ok(start_t_b.Append(seg.Start.T()), "segments.start_t");
                    ok(stop_x_b.Append(seg.Stop.X()), "segments.stop_x");
                    ok(stop_y_b.Append(seg.Stop.Y()), "segments.stop_y");
                    ok(stop_z_b.Append(seg.Stop.Z()), "segments.stop_z");
                    ok(stop_t_b.Append(seg.Stop.T()), "segments.stop_t");

                    // contributors: the TrackId of every trajectory folded into
                    // this segment (delta-rays etc.) -- truth-association keys.
                    ok(contributors_b.Append(), "segments.contributors.open");
                    for (int track_id : seg.Contrib) {
                        ok(contributor_b->Append(track_id), "segments.contributors.value");
                    }
                }
            }

            auto schema = arrow::schema(
              {
                arrow::field("sd", arrow::utf8()),
                arrow::field("n_electrons", arrow::float64()),
                arrow::field("n_photons", arrow::float64()),
                arrow::field("energy_deposit", arrow::float64()),
                arrow::field("secondary_deposit", arrow::float64()),
                arrow::field("track_length", arrow::float64()),
                arrow::field("primary_id", arrow::int32()),
                arrow::field("start_x", arrow::float64()),
                arrow::field("start_y", arrow::float64()),
                arrow::field("start_z", arrow::float64()),
                arrow::field("start_t", arrow::float64()),
                arrow::field("stop_x", arrow::float64()),
                arrow::field("stop_y", arrow::float64()),
                arrow::field("stop_z", arrow::float64()),
                arrow::field("stop_t", arrow::float64()),
                arrow::field("contributors", arrow::list(arrow::int32())),
              },
              schema_metadata("edep.segments"));

            std::vector<std::shared_ptr<arrow::Array>> arrays{
              finish(sd_b),
              finish(n_electrons_b),
              finish(n_photons_b),
              finish(energy_deposit_b),
              finish(secondary_deposit_b),
              finish(track_length_b),
              finish(primary_id_b),
              finish(start_x_b),
              finish(start_y_b),
              finish(start_z_b),
              finish(start_t_b),
              finish(stop_x_b),
              finish(stop_y_b),
              finish(stop_z_b),
              finish(stop_t_b),
              finish(contributors_b),
            };
            return arrow::Table::Make(schema, arrays);
        }

        // ---- photons (edep.photons) -----------------------------------------
        //
        // One row per TG4PhotonHit across all photon detectors.  Stop is the
        // absorption point (on the sensitive surface); Start is the creation point
        // (may be unavailable when photon tracking is offloaded).
        std::shared_ptr<arrow::Table> build_photons(const TG4Event& summary)
        {
            auto* pool = arrow::default_memory_pool();

            arrow::StringBuilder sd_b(pool);
            arrow::DoubleBuilder energy_b(pool);
            arrow::Int32Builder process_b(pool);
            arrow::Int32Builder primary_id_b(pool);
            arrow::DoubleBuilder start_x_b(pool), start_y_b(pool), start_z_b(pool), start_t_b(pool);
            arrow::DoubleBuilder stop_x_b(pool), stop_y_b(pool), stop_z_b(pool), stop_t_b(pool);

            for (auto const& [sd, hits] : summary.PhotonDetectors) {
                for (auto const& hit : hits) {
                    ok(sd_b.Append(sd), "photons.sd");
                    ok(energy_b.Append(hit.EnergyDeposit), "photons.energy");
                    ok(process_b.Append(hit.Process), "photons.process");
                    ok(primary_id_b.Append(hit.PrimaryId), "photons.primary_id");

                    ok(start_x_b.Append(hit.Start.X()), "photons.start_x");
                    ok(start_y_b.Append(hit.Start.Y()), "photons.start_y");
                    ok(start_z_b.Append(hit.Start.Z()), "photons.start_z");
                    ok(start_t_b.Append(hit.Start.T()), "photons.start_t");
                    ok(stop_x_b.Append(hit.Stop.X()), "photons.stop_x");
                    ok(stop_y_b.Append(hit.Stop.Y()), "photons.stop_y");
                    ok(stop_z_b.Append(hit.Stop.Z()), "photons.stop_z");
                    ok(stop_t_b.Append(hit.Stop.T()), "photons.stop_t");
                }
            }

            auto schema = arrow::schema(
              {
                arrow::field("sd", arrow::utf8()),
                arrow::field("energy", arrow::float64()),
                arrow::field("process", arrow::int32()),
                arrow::field("primary_id", arrow::int32()),
                arrow::field("start_x", arrow::float64()),
                arrow::field("start_y", arrow::float64()),
                arrow::field("start_z", arrow::float64()),
                arrow::field("start_t", arrow::float64()),
                arrow::field("stop_x", arrow::float64()),
                arrow::field("stop_y", arrow::float64()),
                arrow::field("stop_z", arrow::float64()),
                arrow::field("stop_t", arrow::float64()),
              },
              schema_metadata("edep.photons"));

            std::vector<std::shared_ptr<arrow::Array>> arrays{
              finish(sd_b),      finish(energy_b),  finish(process_b), finish(primary_id_b),
              finish(start_x_b), finish(start_y_b), finish(start_z_b), finish(start_t_b),
              finish(stop_x_b),  finish(stop_y_b),  finish(stop_z_b),  finish(stop_t_b),
            };
            return arrow::Table::Make(schema, arrays);
        }

    } // namespace

    phlex_arrow::TableGroup to_observables(const TG4Event& summary)
    {
        auto segments = build_segments(summary);
        auto photons = build_photons(summary);

        // Table::Make does NOT check that each array's type matches its schema
        // field (notably the list<int32> contributors column), so validate here
        // -- a mismatch otherwise surfaces only later at IPC/serialization.
        ok(segments->ValidateFull(), "segments.ValidateFull");
        ok(photons->ValidateFull(), "photons.ValidateFull");

        phlex_arrow::TableGroup group;
        group.type = kObservablesType;
        group.members[kSegmentsMember] = std::move(segments);
        group.members[kPhotonsMember] = std::move(photons);
        return group;
    }

} // namespace edep_sim_phlex
