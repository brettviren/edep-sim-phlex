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

// modules/tracking.cpp
//
// Phlex algorithm module: registers edep_sim_phlex::Tracking as a transform.
// Phlex prepends 'lib' and appends '.so' to the 'cpp' field in the workflow
// JSON, so add_library(esp_tracking MODULE ...) -> libesp_tracking.so, matching
// cpp: 'esp_tracking' in the workflow.
//
// Config keys:
//   input_layer   (string, required):  Phlex layer for the input Kinematics product.
//   physics_list  (string, optional):  Geant4 reference physics list (default: edep-sim's).
//   gdml          (string, optional):  path to the detector GDML file.
//   macro         (string, optional):  inline Geant4 macro text (physics tunings).

#include "edep_sim_phlex/Tracking.hpp"

#include "phlex_arrow_common/TableGroup.hpp"

#include "phlex/module.hpp"

#include <HepMC3/GenEvent.h>

#include <memory>
#include <string>

using namespace phlex;

PHLEX_REGISTER_ALGORITHMS(m, config)
{
    auto const layer = config.get<std::string>("input_layer");

    auto tracking = std::make_shared<edep_sim_phlex::Tracking>(config);

    m.transform("edep_sim_tracking",
                [tracking](HepMC3::GenEvent const& in)
                  -> phlex_arrow::TableGroup { return (*tracking)(in); },
                concurrency::serial) // one G4RunManager per process (ddm-4nd.9)
      .input_family(product_selector{.creator = "input", .layer = layer, .suffix = "genevent"})
      .output_product_suffixes("observables");
}
