{
  // End-to-end smoke test: HepMC3 gun -> edep-sim tracking -> observables -> HDF5.
  //
  //   hmp_gen_event_gun       fires one 1 GeV muon (+z) per event as a HepMC3::GenEvent.
  //   esp_tracking            runs it through edep-sim/Geant4, emitting a TG4Event.
  //   esp_observables         converts the TG4Event into the edep.observables TableGroup.
  //   phlex_arrow_hdf_output  persists the Arrow TableGroup (segments+photons) to HDF5.
  //
  // Proves the full Q5 chain (ddm-6rn): TG4Event -> Arrow observables -> HDF5.
  // The observer is kept so the run still prints the per-event row counts.
  //
  // Run with (see test/run-smoke-hdf.sh):
  //   PHLEX_PLUGIN_PATH=<hepmc>:<edep>:<phlex-arrow-hdf>:<view/lib> EDEPSIM_ROOT=<edepsim> \
  //     phlex -c smoke-gun-tracking-hdf.jsonnet

  driver: {
    cpp: 'generate_layers',
    layers: {
      event: { parent: 'job', total: 2, starting_number: 0 },
    },
  },
  sources: {
    gun: {
      cpp: 'hmp_gen_event_gun',
      output_layer: 'event',
      pdg: 13,            // mu-
      mass: 105.658,      // MeV
      energy: 1000.0,     // MeV kinetic
      direction: [0, 0, 1],
      position: [0, 0, -2500], // mm, upstream of the detector stack
      number: 1,
      momentum_unit: 'MeV',
      length_unit: 'mm',
    },
  },
  modules: {
    edep_sim_tracking: {
      cpp: 'esp_tracking',
      input_layer: 'event',
      gdml: '/home/bviren/dune/xerosere/devel/edep-sim/inputs/example.gdml',
      // physics_list omitted -> edep-sim default (QGSP_BERT + optical)
    },
    edep_observables: {
      cpp: 'esp_observables',
      input_layer: 'event',
      input_from: 'edep_sim_tracking', // consumes the TG4Event
    },
    tracking_observer: {
      cpp: 'esp_tracking_observer',
      input_layer: 'event',
      input_from: 'edep_observables', // consumes the edep.observables TableGroup
    },
    hdf_out: {
      cpp: 'phlex_arrow_hdf_output',
      output_file: 'edep-observables.h5', // persists the edep.observables TableGroup
    },
  },
}
