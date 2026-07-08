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

// modules/observables.cpp
//
// Phlex algorithm module: converts an edep-sim TG4Event summary (the product of
// the Tracking node) into the Q5 OBSERVABLES product -- an "edep.observables"
// phlex_arrow::TableGroup (segments + photons Arrow tables; see Observables.hpp).
// Kept separate from the Tracking node so the Geant4/edep-sim tracking and the
// Arrow marshaling are independent Phlex nodes.  Module library: esp_observables.
//
// Config keys:
//   input_layer  (string, required):        Phlex layer of the TG4Event product.
//   input_from   (string, optional="edep_sim_tracking"): creator (the tracking
//                module's instance key in the workflow).

#include "edep_sim_phlex/Observables.hpp"

#include "phlex_arrow_common/TableGroup.hpp"

#include "phlex/module.hpp"

#include <TG4Event.h>

#include <string>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    auto const layer = config.get<std::string>("input_layer");
    auto const from = config.get<std::string>("input_from", std::string{"edep_sim_tracking"});

    m.transform("edep_observables",
                [](TG4Event const& ev) -> phlex_arrow::TableGroup {
                    return edep_sim_phlex::to_observables(ev);
                },
                concurrency::serial)
      .input_family(product_selector{.creator = from, .layer = layer, .suffix = "tg4event"})
      .output_product_suffixes("observables");
}
