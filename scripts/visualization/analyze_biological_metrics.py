#!/usr/bin/env python3
"""
Biological Simulation Analysis for SimuCell3D node_node_10x Benchmark
Compares Static vs Adaptive OpenMP scheduling modes

Analyzes:
1. Volume, pressure, contact area distributions
2. Energy conservation and stability
3. Volume-pressure phase space
4. Division dynamics
5. Cell growth curves
"""

import pandas as pd
import numpy as np
import matplotlib.pyplot as plt
import matplotlib.gridspec as gridspec
from pathlib import Path
import sys

# Constants from problem description
DIVISION_VOLUME = 1.4e-14  # m³ (~30 μm diameter)
GROWTH_RATE = 2e-10  # m³/s
BULK_MODULUS = 2500  # Pa
SURFACE_TENSION_MIN = 0.8e-3  # N/m
SURFACE_TENSION_MAX = 1.0e-3  # N/m
TIME_STEP = 50e-9  # s (50 ns)

# Physiological ranges for epithelial tissue
PRESSURE_RANGE = (100, 500)  # Pa
EXPECTED_VOLUME_RATIO = 0.5  # Pre-division volume ~ 50% of division volume

def load_data(csv_path, sample_rate=10):
    """Load CSV data with sampling to reduce memory usage"""
    print(f"Loading {csv_path}...")

    # Read header
    df = pd.read_csv(csv_path, nrows=0)

    # Count total rows
    with open(csv_path, 'r') as f:
        total_rows = sum(1 for _ in f) - 1  # Exclude header

    # Sample every Nth row
    skip_rows = [i for i in range(1, total_rows) if i % sample_rate != 0]

    df = pd.read_csv(csv_path, skiprows=skip_rows)

    print(f"  Loaded {len(df)} rows (sampled from {total_rows})")
    return df

def analyze_biological_metrics(df_static, df_adaptive, output_dir):
    """Generate biological_metrics.png - Volume, pressure, contact area distributions"""
    print("\n=== Biological Metrics Analysis ===")

    fig = plt.figure(figsize=(18, 12))
    gs = gridspec.GridSpec(3, 3, figure=fig, hspace=0.35, wspace=0.3)

    # Get latest snapshots for each mode
    static_latest = df_static[df_static['iteration'] == df_static['iteration'].max()]
    adaptive_latest = df_adaptive[df_adaptive['iteration'] == df_adaptive['iteration'].max()]

    print(f"Static: {len(static_latest)} cells at iteration {static_latest['iteration'].iloc[0]}")
    print(f"Adaptive: {len(adaptive_latest)} cells at iteration {adaptive_latest['iteration'].iloc[0]}")

    # Row 1: Volume distributions
    ax1 = fig.add_subplot(gs[0, 0])
    ax1.hist(static_latest['volume'] / DIVISION_VOLUME, bins=50, alpha=0.6,
             label='Static', color='blue', density=True)
    ax1.axvline(EXPECTED_VOLUME_RATIO, color='red', linestyle='--',
                label=f'Expected pre-division (~{EXPECTED_VOLUME_RATIO:.1f})')
    ax1.set_xlabel('Volume / Division Volume')
    ax1.set_ylabel('Probability Density')
    ax1.set_title('Volume Distribution - Static')
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    ax2 = fig.add_subplot(gs[0, 1])
    ax2.hist(adaptive_latest['volume'] / DIVISION_VOLUME, bins=50, alpha=0.6,
             label='Adaptive', color='green', density=True)
    ax2.axvline(EXPECTED_VOLUME_RATIO, color='red', linestyle='--',
                label=f'Expected pre-division (~{EXPECTED_VOLUME_RATIO:.1f})')
    ax2.set_xlabel('Volume / Division Volume')
    ax2.set_ylabel('Probability Density')
    ax2.set_title('Volume Distribution - Adaptive')
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    ax3 = fig.add_subplot(gs[0, 2])
    ax3.hist(static_latest['volume'] / DIVISION_VOLUME, bins=50, alpha=0.5,
             label='Static', color='blue', density=True)
    ax3.hist(adaptive_latest['volume'] / DIVISION_VOLUME, bins=50, alpha=0.5,
             label='Adaptive', color='green', density=True)
    ax3.axvline(EXPECTED_VOLUME_RATIO, color='red', linestyle='--',
                label=f'Expected (~{EXPECTED_VOLUME_RATIO:.1f})')
    ax3.set_xlabel('Volume / Division Volume')
    ax3.set_ylabel('Probability Density')
    ax3.set_title('Volume Distribution - Overlay')
    ax3.legend()
    ax3.grid(True, alpha=0.3)

    # Row 2: Pressure distributions
    ax4 = fig.add_subplot(gs[1, 0])
    ax4.hist(static_latest['pressure'], bins=50, alpha=0.6, color='blue', density=True)
    ax4.axvspan(PRESSURE_RANGE[0], PRESSURE_RANGE[1], alpha=0.2, color='red',
                label=f'Physiological range ({PRESSURE_RANGE[0]}-{PRESSURE_RANGE[1]} Pa)')
    ax4.set_xlabel('Pressure (Pa)')
    ax4.set_ylabel('Probability Density')
    ax4.set_title('Pressure Distribution - Static')
    ax4.legend()
    ax4.grid(True, alpha=0.3)

    ax5 = fig.add_subplot(gs[1, 1])
    ax5.hist(adaptive_latest['pressure'], bins=50, alpha=0.6, color='green', density=True)
    ax5.axvspan(PRESSURE_RANGE[0], PRESSURE_RANGE[1], alpha=0.2, color='red',
                label=f'Physiological range ({PRESSURE_RANGE[0]}-{PRESSURE_RANGE[1]} Pa)')
    ax5.set_xlabel('Pressure (Pa)')
    ax5.set_ylabel('Probability Density')
    ax5.set_title('Pressure Distribution - Adaptive')
    ax5.legend()
    ax5.grid(True, alpha=0.3)

    ax6 = fig.add_subplot(gs[1, 2])
    ax6.hist(static_latest['pressure'], bins=50, alpha=0.5,
             label='Static', color='blue', density=True)
    ax6.hist(adaptive_latest['pressure'], bins=50, alpha=0.5,
             label='Adaptive', color='green', density=True)
    ax6.axvspan(PRESSURE_RANGE[0], PRESSURE_RANGE[1], alpha=0.2, color='red',
                label=f'Physiological ({PRESSURE_RANGE[0]}-{PRESSURE_RANGE[1]} Pa)')
    ax6.set_xlabel('Pressure (Pa)')
    ax6.set_ylabel('Probability Density')
    ax6.set_title('Pressure Distribution - Overlay')
    ax6.legend()
    ax6.grid(True, alpha=0.3)

    # Row 3: Contact area fraction distributions
    ax7 = fig.add_subplot(gs[2, 0])
    ax7.hist(static_latest['cell_contact_area_fraction'], bins=50, alpha=0.6,
             color='blue', density=True)
    ax7.set_xlabel('Contact Area Fraction')
    ax7.set_ylabel('Probability Density')
    ax7.set_title('Contact Fraction - Static')
    ax7.grid(True, alpha=0.3)

    ax8 = fig.add_subplot(gs[2, 1])
    ax8.hist(adaptive_latest['cell_contact_area_fraction'], bins=50, alpha=0.6,
             color='green', density=True)
    ax8.set_xlabel('Contact Area Fraction')
    ax8.set_ylabel('Probability Density')
    ax8.set_title('Contact Fraction - Adaptive')
    ax8.grid(True, alpha=0.3)

    ax9 = fig.add_subplot(gs[2, 2])
    ax9.hist(static_latest['cell_contact_area_fraction'], bins=50, alpha=0.5,
             label='Static', color='blue', density=True)
    ax9.hist(adaptive_latest['cell_contact_area_fraction'], bins=50, alpha=0.5,
             label='Adaptive', color='green', density=True)
    ax9.set_xlabel('Contact Area Fraction')
    ax9.set_ylabel('Probability Density')
    ax9.set_title('Contact Fraction - Overlay')
    ax9.legend()
    ax9.grid(True, alpha=0.3)

    plt.suptitle('Biological Metrics: Volume, Pressure, Contact Area Distributions',
                 fontsize=16, fontweight='bold', y=0.995)

    plots_dir = output_dir / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'biological_metrics.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved: {output_path}")

    # Print statistics
    print("\n--- Volume Statistics ---")
    print(f"Static:   mean={static_latest['volume'].mean()/DIVISION_VOLUME:.3f}V_div, "
          f"std={static_latest['volume'].std()/DIVISION_VOLUME:.3f}V_div")
    print(f"Adaptive: mean={adaptive_latest['volume'].mean()/DIVISION_VOLUME:.3f}V_div, "
          f"std={adaptive_latest['volume'].std()/DIVISION_VOLUME:.3f}V_div")

    print("\n--- Pressure Statistics ---")
    print(f"Static:   mean={static_latest['pressure'].mean():.1f} Pa, "
          f"std={static_latest['pressure'].std():.1f} Pa")
    print(f"Adaptive: mean={adaptive_latest['pressure'].mean():.1f} Pa, "
          f"std={adaptive_latest['pressure'].std():.1f} Pa")

    print("\n--- Contact Fraction Statistics ---")
    print(f"Static:   mean={static_latest['cell_contact_area_fraction'].mean():.3f}, "
          f"std={static_latest['cell_contact_area_fraction'].std():.3f}")
    print(f"Adaptive: mean={adaptive_latest['cell_contact_area_fraction'].mean():.3f}, "
          f"std={adaptive_latest['cell_contact_area_fraction'].std():.3f}")

    plt.close()

def analyze_energy_stability(df_static, df_adaptive, output_dir):
    """Generate energy_stability_analysis.png - Energy conservation over time"""
    print("\n=== Energy Stability Analysis ===")

    # Aggregate by iteration (sum over all cells)
    static_energy = df_static.groupby('iteration').agg({
        'simulation_time': 'first',
        'kinetic_energy': 'sum',
        'total_potential_energy': 'sum'
    }).reset_index()

    adaptive_energy = df_adaptive.groupby('iteration').agg({
        'simulation_time': 'first',
        'kinetic_energy': 'sum',
        'total_potential_energy': 'sum'
    }).reset_index()

    static_energy['total_energy'] = static_energy['kinetic_energy'] + static_energy['total_potential_energy']
    static_energy['ke_pe_ratio'] = static_energy['kinetic_energy'] / static_energy['total_potential_energy']

    adaptive_energy['total_energy'] = adaptive_energy['kinetic_energy'] + adaptive_energy['total_potential_energy']
    adaptive_energy['ke_pe_ratio'] = adaptive_energy['kinetic_energy'] / adaptive_energy['total_potential_energy']

    fig, axes = plt.subplots(2, 2, figsize=(16, 10))

    # Total energy over time
    ax1 = axes[0, 0]
    ax1.plot(static_energy['simulation_time'], static_energy['total_energy'],
             label='Static', color='blue', alpha=0.7)
    ax1.plot(adaptive_energy['simulation_time'], adaptive_energy['total_energy'],
             label='Adaptive', color='green', alpha=0.7)
    ax1.set_xlabel('Simulation Time (s)')
    ax1.set_ylabel('Total Energy (J)')
    ax1.set_title('Total Energy Evolution')
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # KE/PE ratio (quasi-static indicator)
    ax2 = axes[0, 1]
    ax2.semilogy(static_energy['simulation_time'], static_energy['ke_pe_ratio'],
                 label='Static', color='blue', alpha=0.7)
    ax2.semilogy(adaptive_energy['simulation_time'], adaptive_energy['ke_pe_ratio'],
                 label='Adaptive', color='green', alpha=0.7)
    ax2.axhline(0.01, color='red', linestyle='--', label='Quasi-static threshold (0.01)')
    ax2.set_xlabel('Simulation Time (s)')
    ax2.set_ylabel('KE/PE Ratio')
    ax2.set_title('Kinetic to Potential Energy Ratio (Quasi-static Test)')
    ax2.legend()
    ax2.grid(True, alpha=0.3, which='both')

    # Kinetic energy over time
    ax3 = axes[1, 0]
    ax3.semilogy(static_energy['simulation_time'], static_energy['kinetic_energy'],
                 label='Static', color='blue', alpha=0.7)
    ax3.semilogy(adaptive_energy['simulation_time'], adaptive_energy['kinetic_energy'],
                 label='Adaptive', color='green', alpha=0.7)
    ax3.set_xlabel('Simulation Time (s)')
    ax3.set_ylabel('Kinetic Energy (J)')
    ax3.set_title('Kinetic Energy Evolution')
    ax3.legend()
    ax3.grid(True, alpha=0.3, which='both')

    # Potential energy over time
    ax4 = axes[1, 1]
    ax4.plot(static_energy['simulation_time'], static_energy['total_potential_energy'],
             label='Static', color='blue', alpha=0.7)
    ax4.plot(adaptive_energy['simulation_time'], adaptive_energy['total_potential_energy'],
             label='Adaptive', color='green', alpha=0.7)
    ax4.set_xlabel('Simulation Time (s)')
    ax4.set_ylabel('Potential Energy (J)')
    ax4.set_title('Potential Energy Evolution')
    ax4.legend()
    ax4.grid(True, alpha=0.3)

    plt.suptitle('Energy Stability Analysis: Conservation and Quasi-static Regime',
                 fontsize=14, fontweight='bold')
    plt.tight_layout()

    plots_dir = output_dir / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'energy_stability_analysis.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved: {output_path}")

    # Print statistics
    print(f"\n--- Quasi-static Regime Check (KE/PE << 1) ---")
    print(f"Static:   mean KE/PE = {static_energy['ke_pe_ratio'].mean():.6f}, "
          f"max = {static_energy['ke_pe_ratio'].max():.6f}")
    print(f"Adaptive: mean KE/PE = {adaptive_energy['ke_pe_ratio'].mean():.6f}, "
          f"max = {adaptive_energy['ke_pe_ratio'].max():.6f}")

    plt.close()

def analyze_phase_space(df_static, df_adaptive, output_dir):
    """Generate phase_space_analysis.png - Volume-pressure correlations"""
    print("\n=== Phase Space Analysis ===")

    # Get latest snapshots
    static_latest = df_static[df_static['iteration'] == df_static['iteration'].max()]
    adaptive_latest = df_adaptive[df_adaptive['iteration'] == df_adaptive['iteration'].max()]

    fig, axes = plt.subplots(2, 2, figsize=(16, 10))

    # Static: Volume vs Pressure scatter
    ax1 = axes[0, 0]
    scatter1 = ax1.scatter(static_latest['volume']/DIVISION_VOLUME, static_latest['pressure'],
                          c=static_latest['cell_contact_area_fraction'], cmap='viridis',
                          alpha=0.6, s=20)
    ax1.set_xlabel('Volume / Division Volume')
    ax1.set_ylabel('Pressure (Pa)')
    ax1.set_title('Static: Volume-Pressure Phase Space')
    ax1.grid(True, alpha=0.3)
    cbar1 = plt.colorbar(scatter1, ax=ax1)
    cbar1.set_label('Contact Fraction')

    # Adaptive: Volume vs Pressure scatter
    ax2 = axes[0, 1]
    scatter2 = ax2.scatter(adaptive_latest['volume']/DIVISION_VOLUME, adaptive_latest['pressure'],
                          c=adaptive_latest['cell_contact_area_fraction'], cmap='viridis',
                          alpha=0.6, s=20)
    ax2.set_xlabel('Volume / Division Volume')
    ax2.set_ylabel('Pressure (Pa)')
    ax2.set_title('Adaptive: Volume-Pressure Phase Space')
    ax2.grid(True, alpha=0.3)
    cbar2 = plt.colorbar(scatter2, ax=ax2)
    cbar2.set_label('Contact Fraction')

    # Volume vs Target Volume (growth tracking)
    ax3 = axes[1, 0]
    ax3.scatter(static_latest['target_volume']/DIVISION_VOLUME,
               static_latest['volume']/DIVISION_VOLUME,
               alpha=0.5, label='Static', color='blue', s=15)
    ax3.scatter(adaptive_latest['target_volume']/DIVISION_VOLUME,
               adaptive_latest['volume']/DIVISION_VOLUME,
               alpha=0.5, label='Adaptive', color='green', s=15)
    ax3.plot([0, 1.2], [0, 1.2], 'r--', label='Perfect tracking')
    ax3.set_xlabel('Target Volume / Division Volume')
    ax3.set_ylabel('Actual Volume / Division Volume')
    ax3.set_title('Volume Tracking: Actual vs Target')
    ax3.legend()
    ax3.grid(True, alpha=0.3)

    # Contact fraction vs Pressure
    ax4 = axes[1, 1]
    ax4.scatter(static_latest['cell_contact_area_fraction'], static_latest['pressure'],
               alpha=0.5, label='Static', color='blue', s=15)
    ax4.scatter(adaptive_latest['cell_contact_area_fraction'], adaptive_latest['pressure'],
               alpha=0.5, label='Adaptive', color='green', s=15)
    ax4.set_xlabel('Contact Area Fraction')
    ax4.set_ylabel('Pressure (Pa)')
    ax4.set_title('Contact Fraction vs Pressure')
    ax4.legend()
    ax4.grid(True, alpha=0.3)

    plt.suptitle('Phase Space Analysis: Volume-Pressure Correlations and Growth Tracking',
                 fontsize=14, fontweight='bold')
    plt.tight_layout()

    plots_dir = output_dir / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'phase_space_analysis.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved: {output_path}")

    # Correlation analysis
    static_corr = np.corrcoef(static_latest['volume'], static_latest['pressure'])[0, 1]
    adaptive_corr = np.corrcoef(adaptive_latest['volume'], adaptive_latest['pressure'])[0, 1]

    print(f"\n--- Volume-Pressure Correlation ---")
    print(f"Static:   r = {static_corr:.4f} (expected: negative for smaller cells = higher pressure)")
    print(f"Adaptive: r = {adaptive_corr:.4f}")

    plt.close()

def analyze_division_dynamics(df_static, df_adaptive, output_dir):
    """Generate division_dynamics_analysis.png - Cell division patterns"""
    print("\n=== Division Dynamics Analysis ===")

    # Track cell count over time
    static_counts = df_static.groupby('iteration').agg({
        'simulation_time': 'first',
        'cell_id': 'count'
    }).reset_index()
    static_counts.columns = ['iteration', 'simulation_time', 'cell_count']

    adaptive_counts = df_adaptive.groupby('iteration').agg({
        'simulation_time': 'first',
        'cell_id': 'count'
    }).reset_index()
    adaptive_counts.columns = ['iteration', 'simulation_time', 'cell_count']

    # Detect division events (cell count increases)
    static_counts['divisions'] = static_counts['cell_count'].diff().fillna(0)
    adaptive_counts['divisions'] = adaptive_counts['cell_count'].diff().fillna(0)

    # Division times
    static_div_times = static_counts[static_counts['divisions'] > 0]['simulation_time'].values
    adaptive_div_times = adaptive_counts[adaptive_counts['divisions'] > 0]['simulation_time'].values

    # Inter-division times
    static_inter_div = np.diff(static_div_times) if len(static_div_times) > 1 else np.array([])
    adaptive_inter_div = np.diff(adaptive_div_times) if len(adaptive_div_times) > 1 else np.array([])

    fig, axes = plt.subplots(2, 2, figsize=(16, 10))

    # Division events over time
    ax1 = axes[0, 0]
    ax1.plot(static_counts['simulation_time'], static_counts['divisions'],
             label='Static', color='blue', alpha=0.7, marker='o', markersize=3)
    ax1.plot(adaptive_counts['simulation_time'], adaptive_counts['divisions'],
             label='Adaptive', color='green', alpha=0.7, marker='o', markersize=3)
    ax1.set_xlabel('Simulation Time (s)')
    ax1.set_ylabel('Division Events per Timestep')
    ax1.set_title('Division Event Rate Over Time')
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # Cumulative divisions
    ax2 = axes[0, 1]
    static_counts['cumulative_divisions'] = static_counts['divisions'].cumsum()
    adaptive_counts['cumulative_divisions'] = adaptive_counts['divisions'].cumsum()
    ax2.plot(static_counts['simulation_time'], static_counts['cumulative_divisions'],
             label='Static', color='blue', alpha=0.7)
    ax2.plot(adaptive_counts['simulation_time'], adaptive_counts['cumulative_divisions'],
             label='Adaptive', color='green', alpha=0.7)
    ax2.set_xlabel('Simulation Time (s)')
    ax2.set_ylabel('Cumulative Division Events')
    ax2.set_title('Cumulative Division Events')
    ax2.legend()
    ax2.grid(True, alpha=0.3)

    # Inter-division time distribution
    ax3 = axes[1, 0]
    if len(static_inter_div) > 0:
        ax3.hist(static_inter_div, bins=30, alpha=0.5, label='Static',
                color='blue', density=True)
    if len(adaptive_inter_div) > 0:
        ax3.hist(adaptive_inter_div, bins=30, alpha=0.5, label='Adaptive',
                color='green', density=True)
    ax3.set_xlabel('Inter-division Time (s)')
    ax3.set_ylabel('Probability Density')
    ax3.set_title('Inter-division Time Distribution')
    ax3.legend()
    ax3.grid(True, alpha=0.3)

    # Division synchronization (divisions per iteration)
    ax4 = axes[1, 1]
    static_div_hist = static_counts[static_counts['divisions'] > 0]['divisions']
    adaptive_div_hist = adaptive_counts[adaptive_counts['divisions'] > 0]['divisions']

    bins = np.arange(0, max(static_div_hist.max(), adaptive_div_hist.max()) + 2, 1)
    ax4.hist(static_div_hist, bins=bins, alpha=0.5, label='Static', color='blue')
    ax4.hist(adaptive_div_hist, bins=bins, alpha=0.5, label='Adaptive', color='green')
    ax4.set_xlabel('Number of Simultaneous Divisions')
    ax4.set_ylabel('Frequency')
    ax4.set_title('Division Synchronization (cells dividing per event)')
    ax4.legend()
    ax4.grid(True, alpha=0.3)

    plt.suptitle('Division Dynamics: Timing, Synchronization, and Event Patterns',
                 fontsize=14, fontweight='bold')
    plt.tight_layout()

    plots_dir = output_dir / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'division_dynamics_analysis.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved: {output_path}")

    # Print statistics
    print(f"\n--- Division Statistics ---")
    print(f"Static:   Total divisions = {int(static_counts['cumulative_divisions'].iloc[-1])}, "
          f"Mean inter-division time = {static_inter_div.mean():.6f} s" if len(static_inter_div) > 0 else "N/A")
    print(f"Adaptive: Total divisions = {int(adaptive_counts['cumulative_divisions'].iloc[-1])}, "
          f"Mean inter-division time = {adaptive_inter_div.mean():.6f} s" if len(adaptive_inter_div) > 0 else "N/A")

    plt.close()

def analyze_cell_growth(df_static, df_adaptive, output_dir):
    """Generate cell_growth_comparison.png - Growth curves"""
    print("\n=== Cell Growth Comparison ===")

    # Track cell count over time
    static_counts = df_static.groupby('iteration').agg({
        'simulation_time': 'first',
        'cell_id': 'count'
    }).reset_index()
    static_counts.columns = ['iteration', 'simulation_time', 'cell_count']

    adaptive_counts = df_adaptive.groupby('iteration').agg({
        'simulation_time': 'first',
        'cell_id': 'count'
    }).reset_index()
    adaptive_counts.columns = ['iteration', 'simulation_time', 'cell_count']

    fig, axes = plt.subplots(1, 3, figsize=(18, 5))

    # Linear scale
    ax1 = axes[0]
    ax1.plot(static_counts['simulation_time'], static_counts['cell_count'],
             label='Static', color='blue', alpha=0.7, linewidth=2)
    ax1.plot(adaptive_counts['simulation_time'], adaptive_counts['cell_count'],
             label='Adaptive', color='green', alpha=0.7, linewidth=2)
    ax1.set_xlabel('Simulation Time (s)')
    ax1.set_ylabel('Cell Count')
    ax1.set_title('Cell Growth (Linear Scale)')
    ax1.legend()
    ax1.grid(True, alpha=0.3)

    # Semi-log (time vs log(count))
    ax2 = axes[1]
    ax2.semilogy(static_counts['simulation_time'], static_counts['cell_count'],
                 label='Static', color='blue', alpha=0.7, linewidth=2)
    ax2.semilogy(adaptive_counts['simulation_time'], adaptive_counts['cell_count'],
                 label='Adaptive', color='green', alpha=0.7, linewidth=2)
    ax2.set_xlabel('Simulation Time (s)')
    ax2.set_ylabel('Cell Count (log scale)')
    ax2.set_title('Cell Growth (Semi-log: exponential test)')
    ax2.legend()
    ax2.grid(True, alpha=0.3, which='both')

    # Log-log (check power law)
    ax3 = axes[2]
    ax3.loglog(static_counts['simulation_time'], static_counts['cell_count'],
               label='Static', color='blue', alpha=0.7, linewidth=2)
    ax3.loglog(adaptive_counts['simulation_time'], adaptive_counts['cell_count'],
               label='Adaptive', color='green', alpha=0.7, linewidth=2)
    ax3.set_xlabel('Simulation Time (s, log scale)')
    ax3.set_ylabel('Cell Count (log scale)')
    ax3.set_title('Cell Growth (Log-log: power law test)')
    ax3.legend()
    ax3.grid(True, alpha=0.3, which='both')

    plt.suptitle('Cell Growth Curves: Linear, Semi-log, and Log-log Representations',
                 fontsize=14, fontweight='bold')
    plt.tight_layout()

    plots_dir = output_dir / 'plots'
    plots_dir.mkdir(exist_ok=True)
    output_path = plots_dir / 'cell_growth_comparison.png'
    plt.savefig(output_path, dpi=150, bbox_inches='tight')
    print(f"Saved: {output_path}")

    # Calculate growth rates
    static_final = static_counts.iloc[-1]
    adaptive_final = adaptive_counts.iloc[-1]

    print(f"\n--- Growth Statistics ---")
    print(f"Static:   {static_final['cell_count']:.0f} cells at t={static_final['simulation_time']:.6f} s "
          f"(iteration {static_final['iteration']:.0f})")
    print(f"Adaptive: {adaptive_final['cell_count']:.0f} cells at t={adaptive_final['simulation_time']:.6f} s "
          f"(iteration {adaptive_final['iteration']:.0f})")
    print(f"Difference: Adaptive has {(adaptive_final['cell_count'] - static_final['cell_count']):.0f} more cells "
          f"({100*(adaptive_final['cell_count']/static_final['cell_count'] - 1):.1f}% increase)")

    plt.close()

def main():
    if len(sys.argv) < 3:
        print("Usage: python analyze_biological_metrics.py <static_csv> <adaptive_csv> [output_dir]")
        sys.exit(1)

    static_csv = Path(sys.argv[1])
    adaptive_csv = Path(sys.argv[2])
    output_dir = Path(sys.argv[3]) if len(sys.argv) > 3 else Path.cwd()

    if not static_csv.exists():
        print(f"Error: {static_csv} not found")
        sys.exit(1)
    if not adaptive_csv.exists():
        print(f"Error: {adaptive_csv} not found")
        sys.exit(1)

    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"=== SimuCell3D Biological Simulation Analysis ===")
    print(f"Static CSV:   {static_csv}")
    print(f"Adaptive CSV: {adaptive_csv}")
    print(f"Output Dir:   {output_dir}")

    # Load data with sampling (every 10th row to manage memory)
    df_static = load_data(static_csv, sample_rate=10)
    df_adaptive = load_data(adaptive_csv, sample_rate=10)

    # Generate all analyses
    analyze_biological_metrics(df_static, df_adaptive, output_dir)
    analyze_energy_stability(df_static, df_adaptive, output_dir)
    analyze_phase_space(df_static, df_adaptive, output_dir)
    analyze_division_dynamics(df_static, df_adaptive, output_dir)
    analyze_cell_growth(df_static, df_adaptive, output_dir)

    print(f"\n=== Analysis Complete ===")
    print(f"All plots saved to: {output_dir}")

if __name__ == '__main__':
    main()
