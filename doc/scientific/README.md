# Scientific Documentation

This section covers the scientific foundations and validation of SimuCell3D.

## Documents

| Document | Description |
|----------|-------------|
| [physics-model.md](physics-model.md) | Mathematical formulation of the Deformable Cell Model |
| [parameter-guide.md](parameter-guide.md) | Physical meaning and typical values for simulation parameters |
| [reproducing-paper-results.md](reproducing-paper-results.md) | Guide to reproduce figures from the Nature paper |

## Quick Reference

### The Deformable Cell Model (DCM)

SimuCell3D implements a DCM where cells are represented as deformable mesh surfaces. The key physics include:

1. **Cell mechanics**: Pressure, surface tension, bending rigidity
2. **Contact mechanics**: Adhesion and repulsion between cells
3. **Growth and division**: Volume-dependent cell division
4. **Polarization**: Apical/basal/lateral membrane differentiation

### Key Equations

**Cell pressure** (Equation 2 in paper):
```
P = K * (V - V₀) / V₀
```
Where K is bulk modulus, V is current volume, V₀ is target volume.

**Surface tension force** (Equation 6):
```
F_surface = -γ * κ * n
```
Where γ is surface tension, κ is mean curvature, n is surface normal.

### Parameter Validation

All default parameters are validated against experimental measurements. See [Table 2 in the paper](../SimuCell3D.pdf) for:
- Bulk modulus: ~2250 Pa (AFM measurements)
- Surface tension: 0.5-2.5 mN/m (micropipette aspiration)
- Bending stiffness: 1-2 × 10⁻¹⁸ J (membrane fluctuation analysis)

## Citation

If you use SimuCell3D in your research, please cite:

```bibtex
@article{runser2024simucell3d,
  title={SimuCell3D: A framework for simulating tissues in 3D with high geometrical resolution},
  author={Runser, Steve and others},
  journal={Nature Computational Science},
  year={2024},
  publisher={Nature Publishing Group}
}
```
