cmake_minimum_required(VERSION 3.25)
project(edep_sim_phlex VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# phlex's exported CMake config does NOT find_dependency() its own transitive
# dependencies, yet phlexTargets.cmake sets INTERFACE_LINK_LIBRARIES on
# spdlog::spdlog, TBB::tbb and fmt::fmt.  Those imported targets must therefore
# already exist when phlexTargets.cmake is included, so we find_package() them
# BEFORE find_package(phlex) -- otherwise its Targets file errors with
# "target not found".  (wire-cell-phlex only avoids this because it finds TBB
# explicitly and pulls spdlog transitively via find_package(WireCell).)
find_package(Arrow CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS json)
find_package(TBB REQUIRED)
find_package(spdlog REQUIRED)
find_package(fmt REQUIRED)
find_package(phlex REQUIRED)
find_package(phlex_arrow_common CONFIG REQUIRED)
find_package(edep_arrow CONFIG REQUIRED)
find_package(Geant4 REQUIRED)
find_package(EDepSim REQUIRED)
find_package(HepMC3 REQUIRED)

add_compile_options(-Wall -Wpedantic)

# ---------------------------------------------------------------------------
# edep_sim_phlex — shared library
#
# The edep-sim/Phlex integration code: the HepMC3->G4 kinematics generator
# (GenEventKine + IGenEventSink), the GenEvent->G4 converter, and the Tracking
# node body.  No Phlex module registration here; the MODULE target below links it.
# ---------------------------------------------------------------------------
add_library(edep_sim_phlex SHARED
  edep_sim_phlex/Convert.cpp
  edep_sim_phlex/GenEventKine.cpp
  edep_sim_phlex/Observables.cpp
  edep_sim_phlex/Tracking.cpp     # service-based (EDepSim::TrackingService)
  edep_sim_phlex/TrackingG4.cpp   # original self-contained implementation
)

target_include_directories(edep_sim_phlex
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
)

target_link_libraries(edep_sim_phlex
  PUBLIC
    EDepSim::edepsim_service
    EDepSim::edepsim
    EDepSim::edepsim_io
    ${Geant4_LIBRARIES}
    HepMC3::HepMC3
    phlex::core
    phlex_arrow_common::phlex_arrow_common
    edep_arrow::edep_arrow
    Arrow::arrow_shared
    Boost::json
)

# ---------------------------------------------------------------------------
# Phlex MODULE target (dlopen-only) registering the Tracking transform.
# ---------------------------------------------------------------------------
add_library(esp_tracking MODULE modules/tracking.cpp)
target_link_libraries(esp_tracking PRIVATE
  edep_sim_phlex
  phlex::module
)
target_compile_options(esp_tracking PRIVATE -UNDEBUG)

# TG4Event -> Q5 observables (Arrow TableGroup): a separate node from tracking.
add_library(esp_observables MODULE modules/observables.cpp)
target_link_libraries(esp_observables PRIVATE
  edep_sim_phlex
  phlex::module
)
target_compile_options(esp_observables PRIVATE -UNDEBUG)

add_library(esp_tracking_observer MODULE modules/tracking_observer.cpp)
target_link_libraries(esp_tracking_observer PRIVATE
  edep_sim_phlex
  phlex::module
)
target_compile_options(esp_tracking_observer PRIVATE -UNDEBUG)

# ---------------------------------------------------------------------------
# Install rules
# ---------------------------------------------------------------------------
include(GNUInstallDirs)

install(TARGETS edep_sim_phlex
  EXPORT edep_sim_phlexTargets
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

install(DIRECTORY edep_sim_phlex/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/edep_sim_phlex"
  FILES_MATCHING PATTERN "*.hpp"
)

# Phlex MODULE libraries (dlopen only — no EXPORT needed).
install(TARGETS esp_tracking esp_observables esp_tracking_observer
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}"
)

install(EXPORT edep_sim_phlexTargets
  FILE edep_sim_phlexTargets.cmake
  NAMESPACE edep_sim_phlex::
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/edep_sim_phlex"
)

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/edep_sim_phlexConfigVersion.cmake"
  VERSION "${PROJECT_VERSION}"
  COMPATIBILITY SameMajorVersion
)
configure_package_config_file(
  "${CMAKE_CURRENT_SOURCE_DIR}/cmake/edep_sim_phlexConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/edep_sim_phlexConfig.cmake"
  INSTALL_DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/edep_sim_phlex"
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/edep_sim_phlexConfig.cmake"
  "${CMAKE_CURRENT_BINARY_DIR}/edep_sim_phlexConfigVersion.cmake"
  DESTINATION "${CMAKE_INSTALL_LIBDIR}/cmake/edep_sim_phlex"
)
