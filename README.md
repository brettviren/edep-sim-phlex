# edep-sim-phlex

Run the edep-sim (Geant4) energy-deposition simulation as a
[Phlex](https://github.com/framework-r-d/phlex) data-flow node.

This package tracks issue **ddm-4nd.11** (the edep-sim FUNCTION node) of the
"edep-sim as a Phlex data-flow node" epic (**ddm-4nd**).  See the beads issues
and `edep-sim-phlex.org` for the design decisions behind it.

## What it provides

All code is in the `edep_sim_phlex` C++ namespace.

- **`Tracking`** (`edep_sim_phlex/Tracking.hpp`) — the Phlex node, a callable
  class.  It is "`app/edepSim.cc::main()` minus the CLI": it stands up the Geant4
  run manager, geometry (from GDML) and physics from Phlex configuration, and per
  call feeds one input `GenEvent`, runs one `beamOn`, and returns the native
  edep-sim event summary (`TG4Event`) as its product.  Registered as a `phlex`
  transform in `modules/tracking.cpp`.  Converting the `TG4Event` into the Q5
  observables is a **separate** downstream node (`modules/observables.cpp`), so
  the tracking node carries no Arrow dependency.
- **`to_observables`** (`edep_sim_phlex/Observables.hpp`) — converts a `TG4Event`
  into the `edep.observables` Arrow `TableGroup` (see below).  Registered as its
  own `phlex` transform in `modules/observables.cpp`.
- **`GenEventKine`** (`edep_sim_phlex/GenEventKine.hpp`) — the kinematics
  generator.  It is a `G4VPrimaryGenerator` (so edep-sim can drive it) and an
  `IGenEventSink` (so the node can feed it a `HepMC3::GenEvent`).  Injected into
  edep-sim via `UserPrimaryGeneratorAction::AddGenerator` ("approach 4-A",
  ddm-4nd.9).
- **`IGenEventSink`** (`edep_sim_phlex/IGenEventSink.hpp`) — abstract sink with
  `feed_genevent(std::shared_ptr<const HepMC3::GenEvent>)`.
- **`append_primaries`** (`edep_sim_phlex/Convert.hpp`) — free functions that
  convert a `HepMC3::GenEvent` into Geant4 primary vertices/particles.  The
  conversion is delegated here, not implemented inside `GenEventKine`.

## Products

The tracking node emits the native edep-sim summary `TG4Event` (product suffix
`tg4event`).  The downstream observables node consumes that and emits one
`phlex_arrow::TableGroup` (product suffix `observables`, type `edep.observables`)
— the **observables** layer of the Q5 output decision (ddm-4nd.5).  Its members:

- `segments` (schema `edep.segments`) — one row per `TG4HitSegment` across all
  sensitive detectors.  Carries the **ionization-electron** count `n_electrons`
  (N_e) and **scintillation-photon** count `n_photons` (N_ph) derived from the
  edep-sim recombination model — `N_q = E/19.5eV`, `N_ph = N_q·Sec/E`,
  `N_e = N_q − N_ph`, no re-fluctuation — plus `energy_deposit`,
  `secondary_deposit`, `track_length`, `primary_id`, `start_{x,y,z,t}`,
  `stop_{x,y,z,t}`, and `contributors` (`list<int32>` of contributing TrackIds).
- `photons` (schema `edep.photons`) — one row per `TG4PhotonHit`: `energy`,
  `process`, `primary_id`, and `start`/`stop` 4-positions.

Units are edep-sim / CLHEP native (mm, MeV, ns), declared in each table's schema
metadata (`edep.units.{length,energy,time}`).

## Dependencies

- **edep-sim** (`EDepSim::edepsim`, `EDepSim::edepsim_io`)
- **phlex** (`phlex::core`, `phlex::module`)
- **phlex-arrow-common** (`phlex_arrow_common::phlex_arrow_common`) + **Arrow**
  (`Arrow::arrow_shared`) — the `TableGroup` product and its Arrow tables.
- **HepMC3** (`HepMC3::HepMC3`) — must be added to the Spack environment view.
- **Geant4**, **Boost.json** (transitively via the above).

## Status / TODO

- Output implements the Q5 **observables** layer (segments + photons Arrow
  tables; ddm-6rn).  The remaining Q5 layers — the HepMC **MC-truth** graph
  (primaries + trajectory genealogy) and the **associations** tables — are
  follow-on work under **ddm-4nd.5**.
- GDML is taken as a file path; the Phlex geometry *resource* is **ddm-4nd.1/.2**.
- Macro handling applies user text verbatim; validation + lifecycle
  categorization is **ddm-4nd.4**.
- User hooks are **ddm-4nd.6**.
