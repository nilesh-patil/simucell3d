Benchmark data behind doc/benchmarks/
=====================================

These small CSV files were extracted from the run logs, END files, statistics files and mesh
snapshots of the November 2026 cross-version benchmark (243 runs, GCP c2-standard-16, 8 threads).
The raw runs are not in the repository. doc/benchmarks/make_figures.py reads these files and draws
the figures in doc/benchmarks/figures/. The method is described in doc/benchmarks/method.md.
Lines starting with "#" are comments.


Build codes (column "build", column names in completion.csv, "-bN" in run ids)
-------------------------------------------------------------------------------

  Code  Name in the docs              Commit
  B0    1.0                           38af451 (tag v1.0)
  B1    1.0 with two crash fixes      38af451 plus the local-id resync and the face-winding fix
  B2    2.0                           343bd98 (tag v2.0)
  B3    intermediate build            9acfe36
  B4    intermediate build            ba74eed
  B5    intermediate build            f08df16
  B6    intermediate build            a331169
  B7    2.1.0                         f824dfd (tag v2.1.0)


Case codes (column "case")
--------------------------

  Code                  Tissue and setting                                  Input file
  T1_dt1e-07            growth benchmark, time step 1e-7 s (published)      parameters_benchmarck.xml (tag v1.0 only)
  T1_dt5e-08            growth benchmark, 5e-8 s                            same file, time_step edited
  T1_dt2.5e-08          growth benchmark, 2.5e-8 s                          same file, time_step edited
  T1_dt1.25e-08         growth benchmark, 1.25e-8 s                         same file, time_step edited
  T2_eps1.0 ... 4.0     vesicle sweep, eps = apical / lateral tension       parameters/benchmarks/fig3e_monolayer.xml,
                                                                            apical tension = eps x 4e-4 N/m, growth 0
  T3_monolayer          monolayer vesicle, paper protocol (to 1.736e-4 s)   parameters/benchmarks/fig3e_monolayer.xml
  T3_multilayer         multilayer vesicle, paper protocol                  parameters/benchmarks/fig3e_multilayer.xml
  T3_monolayer_long     monolayer vesicle, shipped file (1e-2 s)            parameters/benchmarks/fig3e_monolayer.xml
  T3_long               multilayer vesicle, shipped file (1e-2 s, 48 h cap) parameters/benchmarks/fig3e_multilayer.xml
  T4_tube               tube                                                parameters/core/parameters_tube.xml
  T5_sphere             single cell                                         test/test_scientific_benchmarks/params_single_sphere.xml
  T5_laplace            single cell, Laplace pressure                       test/test_scientific_benchmarks/params_laplace_pressure.xml
  T5_2sph_stable        two cells, apart                                    test/test_scientific_benchmarks/params_two_spheres_stable.xml
  T5_2sph_contact       two cells, in contact                               test/test_scientific_benchmarks/params_two_spheres_contact.xml
  ..._l15               the same file with min_edge_length 1.5e-7 m

Run ids: "at" = paper parameters, "bt" = second parameter set, then tissue, setting and build
(for example at1-dt25-b7-full-r2 = growth benchmark, 2.5e-8 s, 2.1.0, full-length run, replicate 2).
"smoke" runs are short start-up checks; "-60k" runs stop at 60,000 cells.


Parameter set (column "arm")
----------------------------

  a   the paper's parameters (the shipped files, with the edits listed in method.md)
  b   a second, literature-based parameter set, run on four builds (B1, B3, B6, B7)


Outcome values (column "outcome", and the build columns of completion.csv)
--------------------------------------------------------------------------

  clean               reached its last iteration (or target cell count), exit status 0, no cell removed
  clean-at-cap        stopped by its planned wall-time cap, no cell removed
  degraded-removals   reached its end or cap, but the solver removed at least one cell as unstable
  tissue-lost         as degraded-removals, and the final cell count is at most half the peak
                      (for the two-cell files: half the two input cells)
  abort               exit status 1 (in 1.0: the mesh-refinement error)
  crash               killed by a signal (SIGSEGV or SIGABRT)
  hang                no progress; stopped by the harness
  excluded            harness error (missing input file); rerun and left out of all counts

completion.csv gives the worst outcome over a case's runs (order: crash, abort, hang, tissue-lost,
degraded-removals, clean-at-cap, clean). Deaths at the minimum volume and refused divisions do not
change the outcome.


Columns per file
----------------

completion.csv      case; B0..B7 = outcome of the case in that build; B0_runs_clean..B7_runs_clean =
                    clean runs / runs of the case
runs.csv            run; build; arm; case; dt (time step, s); outcome; elapsed_s (GNU time elapsed,
                    s); max_cells (peak cells in the run log)
growth_runs.csv     paper-parameter growth runs: run; build; dt (s); outcome; max_cells (peak);
                    removals (cells removed as unstable); first_removal_at_cells (cells when the
                    first removal happened, empty if none); wall_h (h); refused (refused divisions)
growth_wall.csv     run; build; wall_h (hours from iteration 0); cells (running maximum); iteration.
                    Thinned to about 40 points per decade of cells; the milestone tables on the
                    pages were computed from the full per-iteration logs
memory.csv          run; build; max_cells (peak); maxrss_gib (peak resident memory, GiB = 2^30 bytes,
                    from GNU time); outcome
packing.csv         run; build; n_cells (cells in the snapshot); frac_superposed (share of cells
                    whose nearest other cell centroid lies within 0.25 of the cell's equivalent
                    radius); phi_core (summed volume of cells with centroid inside half the tissue
                    radius, divided by the volume of that sphere); n_core (cells in that sphere)
vesicle_sensitivity.csv
                    2.1.0, eps 1.0-4.0, 3 runs each, final state. readout; pooled_sd (pooled
                    replicate standard deviation); min_step_over_sd (smallest change between
                    neighbouring eps values / pooled_sd); range_over_sd (largest minus smallest mean /
                    pooled_sd); mean_eps* (mean per eps). Readouts: sphericity, contact_area_frac,
                    V_mean (mean cell volume, m^3), V_over_Vtarget, P_mean (mean cell pressure, Pa),
                    frac_touching_neither (share of cells touching neither lumen nor shell), AR_ab
                    (apico-basal aspect ratio), AR_obb (bounding-box aspect ratio), layer_N (layer
                    number), neighbours (contact neighbours per cell)
