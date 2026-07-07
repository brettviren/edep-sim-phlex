{
  // Smoke test: HepMC3 particle gun -> edep-sim tracking -> observables -> observer.
  //
  //   hmp_gen_event_gun  fires one 1 GeV muon (+z) per event as a HepMC3::GenEvent.
  //   esp_tracking       runs it through edep-sim/Geant4, emitting a TG4Event.
  //   esp_observables    converts the TG4Event into the edep.observables TableGroup.
  //   esp_tracking_observer  prints the per-event tracking summary ("[edep-smoke] ...").
  //
  // Success indicator: non-zero trajectories/segments in the observer output,
  // i.e. Geant4 actually tracked the muon through the sensitive LArTracker.
  //
  // Run with (see test/run-smoke.sh):
  //   PHLEX_PLUGIN_PATH=<hepmc build>:<edep build>:<view/lib> EDEPSIM_ROOT=<edepsim prefix> \
  //     phlex -c smoke-gun-tracking.jsonnet

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
      gdml: '/home/bviren/dune/xerosere/reference/edep-sim/inputs/example.gdml',
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
  },
}
