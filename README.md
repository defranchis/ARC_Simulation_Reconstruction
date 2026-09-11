# ARC_Simulation_Reconstruction

Standalone simulation, reconstruction and geometry optimisation of the ARC (Array of RICH Cells) detector proposed for FCC-ee. Charged tracks are propagated through a hexagonal array of RICH cells, Cherenkov photons are generated in the radiators, traced via a spherical mirror onto a SiPM plane, and the Cherenkov angle is reconstructed photon by photon. A differential-evolution optimiser tunes the mirror and detector placement of each cell to minimise the Cherenkov-angle resolution.

## Physics model in brief

- Geometry (`options/ARCGeometry.txt`): a barrel of radius `Radius` and length `Length` carrying two rows of hexagonal cells (`CellsPerRow` in the main row, one fewer plus a half cell in the upper row), and two end caps at `|z| = BarrelZ` with 23 valid cells listed in `include/EndCapRadiatorCell.h`. Tracks with `|cos theta|` below `CosTheta_boundary` belong to the barrel, above it to the end cap. Random end-cap tracks are generated towards the positive-z end cap only; a track towards negative z (possible in single-track mode) is reflected onto the positive end cap, its mirror image. All lengths are in metres, momenta in GeV, angles in radians, photon energies in eV.
- Each cell (`include/RadiatorCell.h`) is a stack: the SiPM sits on the detector plane at the cell's local origin, with the 5 mm cooling plate just below it on the beam side; outwards from the detector plane come the aerogel, the gas (C4F10, Sellmeier parametrisation) and, closing the cell, the spherical mirror.
- Photon yield follows Frank–Tamm with a fixed efficiency factor (`src/ParticleTrack.cpp`, `GetPhotonYield`); photon energy is uniform in 1.55–4.31 eV, the SiPM applies a wavelength-dependent photon-detection efficiency (`src/SiPM.cpp`).
- Tracks follow a helix if `FieldStrength` is non-zero (`src/HelixPath.cpp`); with the committed value 0.0 they are straight lines.
- Reconstruction solves the mirror-reflection quartic for each photon (`src/PhotonReconstructor.cpp`), using both the true emission point and the mid-point of the radiator as the assumed emission point.
- Optimisation cost (`src/ResolutionUtilities.cpp`, `CalculateResolution`): mean over tracks of the per-track resolution RMS(theta_c)/sqrt(N_photons), where N_photons is the sum of the photon weights (the physical photon count, so `PhotonMultiplier` and the boost below `LowMomentumLimit` only improve the statistics of the resolution estimate, as in `RunARC`), plus a pixel-size term, a penalty proportional to the fraction of tracks whose photons fail to reach the detector (wall or mirror miss, or fewer than two simulated photons detected) and, during the fit, a penalty on the mean distance of the photon hits from the detector centre.

## Requirements and build

- CMake 3.17 or newer, a C++17 compiler with OpenMP.
- ROOT 6.22 or newer with the Physics, RIO, Tree, Gpad, MathMore, GenVector and Minuit2 components.

At CERN an LCG view provides everything:

```bash
source /cvmfs/sft.cern.ch/lcg/views/LCG_107/x86_64-el9-gcc13-opt/setup.sh
cmake -S . -B build
cmake --build build -j8
```

The executables are `build/apps/RunARC` and `build/apps/OptimiseARC`. The project is compiled with `-Werror`; ROOT's CMake configuration selects the C++ standard ROOT was built with, so a newer compiler or ROOT can surface new warnings as errors.

## Settings files

Both programs take settings as pairs `<Name> <file>` on the command line. `<Name>` becomes the prefix of every key in that file, so `Seed 42` in the file passed as `General` is read as `General/Seed`. File syntax: one `Key Value` per line, `#` starts a comment, anything after the second token is ignored. Booleans are the literal `true`; duplicate keys are an error. Six blocks are used; the committed files in `options/` are a working barrel setup.

| Block | Keys (committed value) |
|---|---|
| `General` | `Seed` (42), `FullArray` (true: whole array with symmetry mapping; false: a single cell), `NumberTracks` (20000), `TrackToDraw` (comma-separated track numbers shown in the event display), `DrawAllTracks`, `ChromaticDispersion`, `RandomEmissionPoint`, `GasOrAerogel` (Gas), `PhotonMultiplier` (scale on the photon yield), `BarrelOrEndcap` (Barrel), `LowMomentumLimit` (below it the gas yield is boosted 20x, for studies at low momentum), `MassHypothesis1`/`2` (PDG codes for the significance, 211 and 321) |
| `Particle` | `ID` (PDG code, 211), `ConstantMomentum` (true) with `Momentum` (100), otherwise log-uniform between `Momentum_min` and `Momentum_max`; `RandomPhi` (true) or uniform in `Phi_min`–`Phi_max`; barrel tracks are generated uniformly in `z_min`–`z_max` on the cylinder; `CosTheta` and `Phi` are used only by `RunARC SingleTrack` |
| `ARCGeometry` | `Radius` (1.91), `Length` (4.362), `CellsPerRow` (9), `FieldStrength` (0.0, tesla), `BarrelZ` (2.01), `EndCapInnerRadius` (0.30), `EndCapOuterRadius` (1.89), `CosTheta_boundary` (0.74) |
| `RadiatorCell` | `RadiatorThickness` (0.20), `VesselThickness` (0.01), `CoolingThickness` (0.005), `AerogelThickness` (0.01), `MirrorCurvature` (0.37), `DetectorSize` (0.08), then one optimised 5-tuple per cell, `Radiator_c<col>_r<row>_{Curvature,XPosition,ZPosition,DetPosition,DetTilt}` for the barrel and `EndCapRadiator_...` for the end cap |
| `Optimisation` | `NumberAgents` (50), `Iterations` (700), `NumberThreads` (8, OpenMP threads in the track loop; defaults to 8 if absent), `Seed` (42, seed of the differential-evolution search; defaults to `General/Seed` if absent), `DoFit`, `PlotProjections`, `SinglePoints`, `Filename` (FitResults.txt); per parameter `<P>_IsFixed`, `<P>_value`, `<P>_min`, `<P>_max` and `<P>Plot_min`/`<P>Plot_max` for the five parameters `MirrorCurvature`, `MirrorXPosition`, `MirrorZPosition`, `DetectorPosition`, `DetectorTilt` |
| `EventDisplay` | `RowToDraw` (1), `CanvasWidth` (1200), `CanvasHeight` (900); required by `RunARC`, not required by `OptimiseARC` |

The keys `ARCGeometry/MaxEndCapRadius`, `Particle/FromOrigin` and `General/DrawMissPhoton` appear in the committed files but are not read by the code. `General/BarrelOrEndcap`, `General/GasOrAerogel`, `Particle/ID` and the momentum settings must be changed together; the commented alternatives in the files show the aerogel/kaon setup.

## OptimiseARC: per-cell geometry optimisation

```bash
mkdir run_c0_r1 && cd run_c0_r1
../build/apps/OptimiseARC 0 1 \
    General ../options/General.txt Particle ../options/Particle.txt \
    ARCGeometry ../options/ARCGeometry.txt RadiatorCell ../options/RadiatorCell.txt \
    Optimisation ../options/Optimisation.txt
```

The first two arguments are the cell column and row. Valid barrel cells are row 1 with columns 0–8 and row 2 with columns 1–9; valid end-cap cells are listed in `include/EndCapRadiatorCell.h`. The five parameters are the mirror radius of curvature, the mirror centre shift along the local x and z axes, the detector shift along x and the detector tilt (radians), all relative to the defaults built from `RadiatorCell`. The programme generates `NumberTracks` tracks once and then, depending on the `Optimisation` booleans:

- `DoFit true`: runs differential evolution (`include/DifferentialEvolution.h`) with `NumberAgents` agents for `Iterations` generations, printing the best cost and parameters per generation, and writes `Filename` with one line per parameter, for example `Radiator_c0_r1_Curvature 0.369`, ready to paste into `options/RadiatorCell.txt`. Parameters with `_IsFixed true` are held at `_value`.
- `PlotProjections true`: reads `Filename` back and draws the cost as a function of each parameter over `Plot_min`–`Plot_max` to five PDF files.
- `SinglePoints true`: reads five parameter values from standard input, prints the cost, and asks whether to continue.

All outputs go to the current directory under fixed names, so run each cell in its own directory. The track loop in the cost function runs on `Optimisation/NumberThreads` OpenMP threads; results do not depend on the thread count. One cost evaluation with 20000 tracks takes about 0.09 s on one thread of a 64-core EL9 node, 0.011 s on 8 threads and 0.005 s on 16, so a fit with the committed settings (50 agents, 700 iterations) takes a few minutes per cell on 8 to 16 threads. Two knobs control randomness: `General/Seed` fixes the tracks and photons, `Optimisation/Seed` fixes the search path; runs with identical settings and seeds are bit-identical. Optimising the full detector means one run per cell and merging the resulting lines into `options/RadiatorCell.txt`; the committed values were obtained with 20000 tracks.

## RunARC: simulation and reconstruction

```bash
../build/apps/RunARC CherenkovAngleResolution \
    General ../options/General.txt Particle ../options/Particle.txt \
    ARCGeometry ../options/ARCGeometry.txt RadiatorCell ../options/RadiatorCell.txt \
    EventDisplay ../options/EventDisplay.txt
```

- `CherenkovAngleResolution`: generates `NumberTracks` tracks, traces and reconstructs their photons and writes `CherenkovFile.root` containing `CherenkovTree`, one entry per track: momentum and direction, entrance and mirror-hit points, initial and final cell indices, per-photon true and reconstructed Cherenkov angles (with true and assumed emission point), energies, hit positions, migration flags and status codes, and the per-track single-photon resolution, total resolution and the separation significance between the two mass hypotheses. It also writes an event display PDF of the row selected by `EventDisplay/RowToDraw` with the tracks listed in `General/TrackToDraw`.
- `SingleTrack`: propagates one track defined by `Particle/Momentum`, `Particle/CosTheta`, `Particle/Phi` and draws its photon hits on the SiPM of the cell it reaches to `PhotonHits.pdf`.

## Known limitations

- `CherenkovTree` stores a fixed maximum number of photons per track (`CherenkovFile::MaxPhotons` in `apps/RunARC.cpp`, currently 2000); further photons are dropped with a warning and do not enter that track's resolution.

## Repository layout

- `apps/`: the two executables.
- `include/`, `src/`: the `ARC_Simulation_Reconstruction` library (geometry, tracking, photon generation and mapping, reconstruction, optimisation interface, event display).
- `options/`: settings files.
- `documentation/`: Doxygen output for the current sources, `html/index.html` and the PDF manual `latex/refman.pdf`. Regenerate from the repository root with `doxygen documentation/Doxyfile` (needs Doxygen and Graphviz), then `make` in `documentation/latex` for the PDF; the LaTeX style bundled with Doxygen 1.13 needs TeX Live 2022 or older. The vendored quartic solver is excluded from the documentation.
- `include/DifferentialEvolution.h` (differential evolution, by Milos Stojanovic) and `include/Quartic.h`, `src/Quartic.cpp` (quartic solver, by Saša Milenković, GPL) are third-party code.
