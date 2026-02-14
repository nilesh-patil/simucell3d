#!/usr/bin/env python3
"""
Pressure-Volume Phase Space Analysis.
Validates thermodynamic consistency and mechanical equilibrium.

Panels:
- A: Cell-Level P-V Trajectories (2D histogram density)
- B: Pressure Distribution Evolution (violin/box plots by iteration)
- C: Volume Regulation Error (log scale)
- D: P-V Scatter with Thermodynamic Reference Lines

Usage:
    python plot_phase_space.py /path/to/benchmark_directory
"""
import matplotlib.pyplot as plt
import pandas as pd
import numpy as np
from pathlib import Path
import sys


def main(bench_dir: str) -> None:
    """Generate pressure-volume phase space analysis plots.

    Args:
        bench_dir: Path to benchmark directory
    """
    bench_path = Path(bench_dir)

    # Create 2x2 grid for P-V analysis
    fig, axes = plt.subplots(2, 2, figsize=(14, 12))

    csv_paths = {
        'Static': ('sim_static/simulation_statistics.csv', '#4CAF50'),
        'Adaptive': ('sim_adaptive/simulation_statistics.csv', '#F44336')
    }

    all_data = {}

    for mode, (csv_rel, color) in csv_paths.items():
        csv_path = bench_path / csv_rel
        if not csv_path.exists():
            print(f"Warning: {csv_path} not found, skipping {mode}")
            continue

        df = pd.read_csv(csv_path)
        all_data[mode] = (df, color)
        print(f"Loaded {len(df)} cell-iteration records from: {csv_path}")

    if not all_data:
        print(f"No simulation_statistics.csv found in {bench_path}")
        return

    # =========================================================================
    # Panel A: Cell-Level P-V Trajectories (2D histogram density)
    # =========================================================================
    ax_pv = axes[0, 0]

    for mode, (df, color) in all_data.items():
        # Sample for computational efficiency
        sample_size = min(50000, len(df))
        df_sample = df.sample(sample_size, random_state=42) if len(df) > sample_size else df

        # Filter valid data
        valid = (df_sample['volume'] > 0) & (df_sample['pressure'] > 0)
        volumes = df_sample.loc[valid, 'volume'] * 1e9  # Convert to nm³ scale
        pressures = df_sample.loc[valid, 'pressure']

        if len(volumes) > 100:
            # 2D histogram (density plot)
            h = ax_pv.hist2d(volumes, pressures, bins=50,
                           cmap='YlOrRd' if mode == 'Adaptive' else 'YlGnBu',
                           alpha=0.7, density=True)

            # Add contour overlay
            try:
                from scipy import stats
                # KDE for smooth contours
                xmin, xmax = volumes.min(), volumes.max()
                ymin, ymax = pressures.min(), pressures.max()
                xx, yy = np.mgrid[xmin:xmax:50j, ymin:ymax:50j]
                positions = np.vstack([xx.ravel(), yy.ravel()])
                values = np.vstack([volumes, pressures])
                kernel = stats.gaussian_kde(values)
                f = np.reshape(kernel(positions).T, xx.shape)
                ax_pv.contour(xx, yy, f, colors=color, alpha=0.6, linewidths=1)
            except ImportError:
                pass  # scipy not available, skip contours

    ax_pv.set_xlabel('Volume (×10⁻⁹ m³)', fontsize=10)
    ax_pv.set_ylabel('Pressure (Pa)', fontsize=10)
    ax_pv.set_title('P-V Phase Space Density', fontsize=12)
    ax_pv.grid(True, alpha=0.3)

    # Add ideal gas reference line (P ∝ 1/V)
    if all_data:
        mode, (df, color) = list(all_data.items())[0]
        valid = (df['volume'] > 0) & (df['pressure'] > 0)
        if valid.sum() > 0:
            V_mean = df.loc[valid, 'volume'].mean()
            P_mean = df.loc[valid, 'pressure'].mean()
            PV_const = P_mean * V_mean
            V_range = np.linspace(df.loc[valid, 'volume'].min(), df.loc[valid, 'volume'].max(), 50) * 1e9
            P_ideal = PV_const / (V_range / 1e9)
            ax_pv.plot(V_range, P_ideal, 'k--', alpha=0.5, linewidth=1.5, label='PV = const (ideal)')
            ax_pv.legend(loc='upper right', fontsize=8)

    ax_pv.annotate('Dense clusters → stable equilibria\n'
                  'Scattered points → transition states',
                  xy=(0.02, 0.95), xycoords='axes fraction',
                  fontsize=8, va='top', alpha=0.7,
                  bbox=dict(boxstyle='round', facecolor='lightblue', alpha=0.5))

    # =========================================================================
    # Panel B: Pressure Distribution Evolution (box plots by iteration window)
    # =========================================================================
    ax_pressure = axes[0, 1]

    for mode, (df, color) in all_data.items():
        # Bin iterations into windows
        n_windows = 10
        df['iter_window'] = pd.cut(df['iteration'], bins=n_windows, labels=False)

        # Compute statistics per window
        pressure_stats = df.groupby('iter_window')['pressure'].agg(['mean', 'std', 'min', 'max'])
        window_centers = df.groupby('iter_window')['iteration'].mean()

        # Plot mean with error band
        ax_pressure.plot(window_centers, pressure_stats['mean'],
                        label=f'{mode} mean', color=color, linewidth=2)
        ax_pressure.fill_between(window_centers,
                                pressure_stats['mean'] - pressure_stats['std'],
                                pressure_stats['mean'] + pressure_stats['std'],
                                color=color, alpha=0.2)

        # Show range as thin lines
        ax_pressure.plot(window_centers, pressure_stats['min'],
                        color=color, alpha=0.4, linestyle=':', linewidth=1)
        ax_pressure.plot(window_centers, pressure_stats['max'],
                        color=color, alpha=0.4, linestyle=':', linewidth=1)

    ax_pressure.set_xlabel('Iteration', fontsize=10)
    ax_pressure.set_ylabel('Pressure (Pa)', fontsize=10)
    ax_pressure.set_title('Pressure Distribution Evolution', fontsize=12)
    ax_pressure.legend(loc='upper left', fontsize=9)
    ax_pressure.grid(True, alpha=0.3)
    ax_pressure.annotate('Shaded: ±1 std\nDotted: min/max range',
                        xy=(0.95, 0.05), xycoords='axes fraction',
                        ha='right', fontsize=8, alpha=0.7,
                        bbox=dict(boxstyle='round', facecolor='lightyellow', alpha=0.5))

    # =========================================================================
    # Panel C: Volume Regulation Error (log scale)
    # =========================================================================
    ax_vol_error = axes[1, 0]

    for mode, (df, color) in all_data.items():
        # Calculate relative volume error: |V - V_target| / V_target
        valid = (df['volume'] > 0) & (df['target_volume'] > 0)
        vol_error = np.abs(df.loc[valid, 'volume'] - df.loc[valid, 'target_volume']) / df.loc[valid, 'target_volume'] * 100

        iterations = df.loc[valid, 'iteration']

        # Aggregate by iteration (mean error across cells)
        error_by_iter = pd.DataFrame({'iteration': iterations, 'error': vol_error})
        error_mean = error_by_iter.groupby('iteration')['error'].mean()

        # Sample for clarity
        sample_rate = max(1, len(error_mean) // 500)
        error_sampled = error_mean.iloc[::sample_rate]

        # Filter zeros for log scale
        valid_e = error_sampled > 0
        if valid_e.sum() > 10:
            ax_vol_error.semilogy(error_sampled.index[valid_e], error_sampled[valid_e],
                                 label=mode, color=color, alpha=0.7, linewidth=1.5)

            # Report final error
            final_error = error_sampled.iloc[-1] if len(error_sampled) > 0 else 0
            ax_vol_error.annotate(f'{mode} final: {final_error:.2f}%',
                                 xy=(0.95, 0.95 - 0.08 * list(all_data.keys()).index(mode)),
                                 xycoords='axes fraction', ha='right',
                                 fontsize=9, color=color,
                                 bbox=dict(boxstyle='round', facecolor='white', alpha=0.8))

    # Reference lines
    ax_vol_error.axhline(y=10, color='orange', linestyle='--', alpha=0.7, label='10% threshold')
    ax_vol_error.axhline(y=1, color='gray', linestyle='--', alpha=0.5, label='1% target')

    ax_vol_error.set_xlabel('Iteration', fontsize=10)
    ax_vol_error.set_ylabel('|V - V_target| / V_target (%, log)', fontsize=10)
    ax_vol_error.set_title('Volume Regulation Error', fontsize=12)
    ax_vol_error.legend(loc='upper right', fontsize=8)
    ax_vol_error.grid(True, alpha=0.3, which='both')
    ax_vol_error.annotate('< 1% → excellent volume homeostasis\n'
                         '> 10% → mechanical stress',
                         xy=(0.02, 0.05), xycoords='axes fraction',
                         fontsize=8, alpha=0.7,
                         bbox=dict(boxstyle='round', facecolor='wheat', alpha=0.5))

    # =========================================================================
    # Panel D: P-V Scatter with Cell Types/States
    # =========================================================================
    ax_scatter = axes[1, 1]

    for mode, (df, color) in all_data.items():
        # Get last iteration for final state analysis
        last_iter = df['iteration'].max()
        df_last = df[df['iteration'] == last_iter]

        # Sample for plotting
        sample_size = min(2000, len(df_last))
        df_sample = df_last.sample(sample_size, random_state=42) if len(df_last) > sample_size else df_last

        valid = (df_sample['volume'] > 0) & (df_sample['pressure'] > 0)
        volumes = df_sample.loc[valid, 'volume'] * 1e9
        pressures = df_sample.loc[valid, 'pressure']

        # Color by contact fraction if available
        if 'cell_contact_area_fraction' in df_sample.columns:
            contact = df_sample.loc[valid, 'cell_contact_area_fraction']
            scatter = ax_scatter.scatter(volumes, pressures, c=contact, cmap='coolwarm',
                                        alpha=0.5, s=15, label=mode, edgecolors='none')
            if mode == list(all_data.keys())[-1]:  # Add colorbar for last mode
                cbar = plt.colorbar(scatter, ax=ax_scatter, shrink=0.8)
                cbar.set_label('Contact Fraction', fontsize=9)
        else:
            ax_scatter.scatter(volumes, pressures, c=color, alpha=0.4, s=15, label=mode)

    # Add iso-PV lines (constant work curves)
    if all_data:
        mode, (df, color) = list(all_data.items())[0]
        last_iter = df['iteration'].max()
        df_last = df[df['iteration'] == last_iter]
        valid = (df_last['volume'] > 0) & (df_last['pressure'] > 0)

        if valid.sum() > 10:
            V_range = np.linspace(df_last.loc[valid, 'volume'].min() * 0.8,
                                 df_last.loc[valid, 'volume'].max() * 1.2, 50) * 1e9
            for pv_const in [1e-8, 5e-8, 1e-7]:  # Different PV products
                P_iso = pv_const / (V_range / 1e9)
                valid_p = (P_iso > df_last.loc[valid, 'pressure'].min() * 0.5) & \
                         (P_iso < df_last.loc[valid, 'pressure'].max() * 1.5)
                if valid_p.sum() > 5:
                    ax_scatter.plot(V_range[valid_p], P_iso[valid_p], 'k--', alpha=0.3, linewidth=0.8)

    ax_scatter.set_xlabel('Volume (×10⁻⁹ m³)', fontsize=10)
    ax_scatter.set_ylabel('Pressure (Pa)', fontsize=10)
    ax_scatter.set_title('Final State P-V Distribution', fontsize=12)
    ax_scatter.legend(loc='upper right', fontsize=9)
    ax_scatter.grid(True, alpha=0.3)
    ax_scatter.annotate('Color: contact fraction\n'
                       'Dashed: iso-PV curves (constant work)\n'
                       'Tight cluster → mechanical equilibrium',
                       xy=(0.02, 0.05), xycoords='axes fraction',
                       fontsize=8, alpha=0.7,
                       bbox=dict(boxstyle='round', facecolor='lightgreen', alpha=0.5))

    plt.suptitle('Pressure-Volume Phase Space Analysis\nSimuCell3D Benchmark',
                fontsize=14, fontweight='bold')
    plt.tight_layout()

    plots_dir = bench_path / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'phase_space_analysis.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved plot to: {output_path}")

    plt.close()


if __name__ == '__main__':
    if len(sys.argv) < 2:
        print("Usage: python plot_phase_space.py /path/to/benchmark_dir")
        print("\nExample:")
        print("  python plot_phase_space.py ../doc/working/64k_growth_benchmark_20251130/")
        sys.exit(1)

    main(sys.argv[1])
