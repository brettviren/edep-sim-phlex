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

// modules/tracking_observer.cpp
//
// Phlex observer module: prints per-event row counts of the edep-sim observables
// product (an "edep.observables" TableGroup).  Used as the success indicator for
// the smoke test -- a non-zero segment count proves Geant4 actually tracked the
// primaries through the sensitive geometry.  Module library: esp_tracking_observer.
//
// Config keys:
//   input_layer  (string, required):        Phlex layer of the observables product.
//   input_from   (string, optional="edep_observables"): creator (the observables
//                module's instance key in the workflow).

#include "edep_sim_phlex/Data.hpp"

#include "phlex_arrow_common/TableGroup.hpp"

#include "phlex/configuration.hpp"
#include "phlex/module.hpp"

#include <iostream>
#include <string>

using namespace phlex;

namespace {

    // Row count of a TableGroup member, or 0 if the member is absent/null.
    std::int64_t member_rows(phlex_arrow::TableGroup const& g, const std::string& name)
    {
        auto it = g.members.find(name);
        if (it == g.members.end() || !it->second) return 0;
        return it->second->num_rows();
    }

} // namespace

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    auto const layer = config.get<std::string>("input_layer");
    auto const from = config.get<std::string>("input_from", std::string{"edep_observables"});

    m.observe("edep_tracking_observer",
              [](phlex_arrow::TableGroup const& g) {
                  std::cout << "[edep-smoke] type=" << g.type
                            << " segments=" << member_rows(g, edep_sim_phlex::kSegmentsMember)
                            << " photons=" << member_rows(g, edep_sim_phlex::kPhotonsMember)
                            << std::endl;
              },
              concurrency::serial)
      .input_family(product_selector{.creator = from, .layer = layer, .suffix = "observables"});
}
