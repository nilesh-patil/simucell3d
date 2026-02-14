#!/usr/bin/env python3
"""
Energy Conservation and Stability Analysis.
Validates numerical stability across scheduling modes.

Panels:
- A: Energy Drift Relative to Initial State (semi-log)
- B: Energy Fluctuation Spectrum (log-log FFT)
- C: Hamiltonian Violation Metric (dE/dt on log scale)
- D: Energy Component Ratios Over Time

Usage:
    python plot_energy_stability.py /path/to/benchmark_directory
"""
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from pathlib import Path
import sys


def load_energy_timeseries(csv_path: Path) -> pd.DataFrame:
    """Load simulation statistics and aggregate energy by iteration.

    Args:
        csv_path: Path to simulation_statistics.csv

    Returns:
        DataFrame with mean energy values per iteration
    """
    df = pd.read_csv(csv_path)

    # Aggregate by iteration (mean across all cells)
    grouped = df.groupby('iteration').agg({
        'kinetic_energy': 'sum',  # Total system KE
        'surface_tension_energy': 'sum',
        'pressure_energy': 'sum',
        'total_potential_energy': 'sum',
        'membrane_elasticity_energy': 'sum',
        'bending_energy': 'sum'
    }).reset_index()

    # Compute total energy (Hamiltonian)
    grouped['total_energy'] = grouped['kinetic_energy'] + grouped['total_potential_energy']

    return grouped


def main(bench_dir: str) -> None:
    """Generate energy stability analysis plots.

    Args:
        bench_dir: Path to benchmark directory
    """
    bench_path = Path(bench_dir)

    # Create 2x2 grid for energy stability analysis
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))

    csv_paths = {
        'Static': ('sim_static/simulation_statistics.csv', '#4CAF50'),
        'Adaptive': ('sim_adaptive/simulation_statistics.csv', '#F44336')
    }

    energy_data = {}

    for mode, (csv_rel, color) in csv_paths.items():
        csv_path = bench_path / csv_rel
        if not csv_path.exists():
            print(f"Warning: {csv_path} not found, skipping {mode}")
            continue

        df = load_energy_timeseries(csv_path)
        energy_data[mode] = (df, color)
        print(f"Loaded {len(df)} iterations from: {csv_path}")

    if not energy_data:
        print(f"No simulation_statistics.csv found in {bench_path}")
        return

    # =========================================================================
    # Panel A: Energy Drift Relative to Initial State (semi-log)
    # =========================================================================
    ax_drift = axes[0, 0]

    for mode, (df, color) in energy_data.items():
        # Calculate drift: E(t) - E(0)
        E0 = df['total_energy'].iloc[0]
        drift = np.abs(df['total_energy'] - E0)

        # Filter zeros for log scale
        valid = drift > 0
        if valid.sum() > 10:
            ax_drift.semilogy(df.loc[valid, 'iteration'], drift[valid],
                             label=mode, color=color, alpha=0.7, linewidth=1.5)

            # Calculate relative drift percentage
            max_drift = drift.max()
            rel_drift = max_drift / abs(E0) * 100 if E0 != 0 else 0
            ax_drift.annotate(f'{mode}: {rel_drift:.2e}% max drift',
                             xy=(0.95, 0.95 - 0.08 * list(energy_data.keys()).index(mode)),
                             xycoords='axes fraction', ha='right',
                             fontsize=9, color=color,
                             bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))

    ax_drift.set_xlabel('Iteration', fontsize=10)
    ax_drift.set_ylabel('|E(t) - E(0)| (J, log scale)', fontsize=10)
    ax_drift.set_title('Energy Drift from Initial State', fontsize=12)
    ax_drift.legend(loc='upper left')
    ax_drift.grid(True, alpha=0.3, which='both')
    ax_drift.annotate('Lower drift → better energy conservation\n'
                     'Monotonic increase may indicate numerical dissipation',
                     xy=(0.02, 0.15), xycoords='axes fraction',
                     fontsize=8, alpha=0.7,
                     bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.5))

    # =========================================================================
    # Panel B: Energy Fluctuation Spectrum (log-log FFT)
    # =========================================================================
    ax_fft = axes[0, 1]

    for mode, (df, color) in energy_data.items():
        # Use total energy for FFT
        E = df['total_energy'].values
        n = len(E)

        if n > 64:  # Need sufficient samples for meaningful FFT
            # Detrend by subtracting linear fit
            t = np.arange(n)
            coeffs = np.polyfit(t, E, 1)
            E_detrend = E - np.polyval(coeffs, t)

            # Compute power spectral density
            fft_vals = np.fft.rfft(E_detrend)
            psd = np.abs(fft_vals)**2 / n

            # Frequency axis (normalized)
            freq = np.fft.rfftfreq(n)

            # Filter for plotting (exclude DC, keep positive frequencies)
            valid = freq > 0
            freq_valid = freq[valid]
            psd_valid = psd[valid]

            if len(freq_valid) > 5:
                # Smooth with rolling average for clarity
                window = min(5, len(psd_valid) // 10)
                if window > 1:
                    psd_smooth = pd.Series(psd_valid).rolling(window, center=True).mean().values
                else:
                    psd_smooth = psd_valid

                ax_fft.loglog(freq_valid, psd_smooth,
                             label=mode, color=color, alpha=0.7, linewidth=1.5)

    ax_fft.set_xlabel('Normalized Frequency', fontsize=10)
    ax_fft.set_ylabel('Power Spectral Density', fontsize=10)
    ax_fft.set_title('Energy Fluctuation Spectrum (FFT)', fontsize=12)
    ax_fft.legend(loc='upper right')
    ax_fft.grid(True, alpha=0.3, which='both')
    ax_fft.annotate('Flat spectrum → white noise (good)\n'
                   '1/f slope → temporal correlation (physics)\n'
                   'Spikes → periodic artifacts',
                   xy=(0.02, 0.05), xycoords='axes fraction',
                   fontsize=8, alpha=0.7,
                   bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.5))

    # =========================================================================
    # Panel C: Hamiltonian Violation Metric (dE/dt on log scale)
    # =========================================================================
    ax_dedt = axes[1, 0]

    for mode, (df, color) in energy_data.items():
        E = df['total_energy'].values
        iterations = df['iteration'].values

        # Numerical derivative (central difference where possible)
        if len(E) > 2:
            dE = np.gradient(E)
            dt = np.gradient(iterations)
            dE_dt = np.abs(dE / dt)

            # Filter zeros for log scale
            valid = dE_dt > 0
            if valid.sum() > 10:
                # Sample for clarity
                sample_rate = max(1, len(df) // 500)
                iterations_s = iterations[::sample_rate]
                dE_dt_s = dE_dt[::sample_rate]
                valid_s = dE_dt_s > 0

                ax_dedt.semilogy(iterations_s[valid_s], dE_dt_s[valid_s],
                                label=mode, color=color, alpha=0.6, linewidth=1)

                # Report mean violation rate
                mean_rate = np.mean(dE_dt[valid])
                ax_dedt.annotate(f'{mode}: mean |dE/dt| = {mean_rate:.2e} J/iter',
                                xy=(0.95, 0.95 - 0.08 * list(energy_data.keys()).index(mode)),
                                xycoords='axes fraction', ha='right',
                                fontsize=9, color=color,
                                bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))

    ax_dedt.set_xlabel('Iteration', fontsize=10)
    ax_dedt.set_ylabel('|dE/dt| (J/iteration, log scale)', fontsize=10)
    ax_dedt.set_title('Hamiltonian Violation Rate', fontsize=12)
    ax_dedt.legend(loc='upper left')
    ax_dedt.grid(True, alpha=0.3, which='both')
    ax_dedt.annotate('Spikes correlate with division events\n'
                    'Steady baseline → stable integration',
                    xy=(0.02, 0.05), xycoords='axes fraction',
                    fontsize=8, alpha=0.7,
                    bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    # =========================================================================
    # Panel D: Energy Component Ratios Over Time
    # =========================================================================
    ax_ratio = axes[1, 1]

    for mode, (df, color) in energy_data.items():
        # Sample for clarity
        sample_rate = max(1, len(df) // 300)
        df_s = df.iloc[::sample_rate]

        # KE/PE ratio (most important stability indicator)
        with np.errstate(divide='ignore', invalid='ignore'):
            ke_pe_ratio = np.where(df_s['total_potential_energy'] > 0,
                                   df_s['kinetic_energy'] / df_s['total_potential_energy'] * 100,
                                   0)

        valid = ke_pe_ratio > 0
        if valid.sum() > 5:
            ax_ratio.semilogy(df_s.loc[valid, 'iteration'], ke_pe_ratio[valid],
                             label=f'{mode} KE/PE', color=color, alpha=0.7, linewidth=1.5)

        # Surface tension / Pressure ratio (mechanical balance)
        with np.errstate(divide='ignore', invalid='ignore'):
            st_p_ratio = np.where(df_s['pressure_energy'] > 0,
                                  df_s['surface_tension_energy'] / df_s['pressure_energy'] * 100,
                                  0)

        valid_st = st_p_ratio > 0
        if valid_st.sum() > 5:
            ax_ratio.semilogy(df_s.loc[valid_st, 'iteration'], st_p_ratio[valid_st],
                             label=f'{mode} ST/P', color=color, alpha=0.4,
                             linestyle='--', linewidth=1)

    # Reference lines
    ax_ratio.axhline(y=1.0, color='orange', linestyle=':', alpha=0.7, label='1% threshold')
    ax_ratio.axhline(y=0.1, color='gray', linestyle=':', alpha=0.5, label='0.1% quasi-static')

    ax_ratio.set_xlabel('Iteration', fontsize=10)
    ax_ratio.set_ylabel('Energy Ratio (%, log scale)', fontsize=10)
    ax_ratio.set_title('Energy Component Ratios', fontsize=12)
    ax_ratio.legend(loc='upper right', fontsize=8)
    ax_ratio.grid(True, alpha=0.3, which='both')
    ax_ratio.annotate('KE/PE < 0.1% → quasi-static equilibrium\n'
                     'KE/PE > 1% → dynamic regime\n'
                     'ST/P tracks mechanical balance',
                     xy=(0.02, 0.05), xycoords='axes fraction',
                     fontsize=8, alpha=0.7,
                     bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.5))

    plt.suptitle('Energy Conservation & Stability Analysis\nSimuCell3D Benchmark',
                fontsize=14, fontweight='bold')
    plt.tight_layout()

    plots_dir = bench_path / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'energy_stability_analysis.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved plot to: {output_path}")

    plt.close()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_energy_stability.py /path/to/benchmark_dir")
        print("\nExample:")
        print("  python plot_energy_stability.py ../doc/working/64k_growth_benchmark_20251130/")
        sys.exit(1)

    main(sys.argv[1])
