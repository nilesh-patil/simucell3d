# SimuCell3D Documentation

Welcome to the comprehensive documentation for **SimuCell3D** - a high-performance 3D cell simulation framework based on the Subcellular Element Model.

---

## 🚀 Quick Start by Role

Choose your path based on your goals:

| **I want to...** | **Start here** | **Time** |
|------------------|---------------|----------|
| Run my first simulation | [Quick Start Guide](./getting-started/quick-start.md) | 15 min |
| Understand the physics | [Physics Model](./scientific/physics-model.md) | 30 min |
| Reproduce paper results | [Paper Results Guide](./scientific/reproducing-paper-results.md) | 1-2 hours |
| Tune parameters for my system | [Parameter Guide](./scientific/parameter-guide.md) | 30 min |
| Optimize performance | [Performance Tuning](./developer/performance-tuning.md) | 1 hour |
| Use Python bindings | [Python API](./user-guide/python-bindings.md) | 20 min |
| Contribute code | [Contributing Guide](./getting-started/contributing.md) | 30 min |
| Debug common issues | [FAQ](./getting-started/faq.md) | 10 min |

---

## 📁 Documentation Structure

```
doc/
├── README.md (you are here)
│
├── getting-started/          # New user onboarding
│   ├── quick-start.md        # First simulation in 15 minutes
│   ├── faq.md                # Common questions and troubleshooting
│   └── contributing.md       # How to contribute code
│
├── user-guide/               # Day-to-day usage
│   ├── parameter-reference.md   # All XML parameters explained
│   ├── input-mesh-format.md     # Mesh file requirements
│   ├── mesh-generation.md       # Creating input meshes
│   ├── output-format.md         # Understanding simulation outputs
│   ├── python-bindings.md       # Python API usage
│   ├── visualization.md         # Visualization tools
│   └── parameter-screening.md   # Parameter optimization
│
├── scientific/               # Research and validation
│   ├── physics-model.md          # Mathematical formulation
│   ├── parameter-guide.md        # Physical parameter meanings
│   └── reproducing-paper-results.md  # Validate your installation
│
├── developer/                # Advanced development
│   ├── explainer.md              # Codebase architecture overview
│   ├── performance-tuning.md     # OpenMP optimization
│   └── architecture-diagrams.md  # System design *(coming soon)*
│
├── benchmarking/             # Performance analysis
│   └── openmp-benchmarks.md      # Benchmarking framework
│
├── v1.0_vs_current_comparison.md  # What's new in this branch
│
└── docs-old/                 # v1.0 historical archive
    └── README.md             # v1.0 documentation mapping
```

---

## 🆕 What's New in version-cpp-next?

This branch is a **performance-enhanced superset** of v1.0 with:

- ✅ **3 critical bug fixes** (semi-implicit Euler, bulk_modulus=0 crashes, degenerate faces)
- ✅ **2.07x average speedup** (adaptive OpenMP scheduling)
- ✅ **100% backward compatibility** (all v1.0 parameter files work)
- ✅ **56% more tests** (39 vs 25 test files)
- ✅ **New features**: SAP collision detection, performance diagnostics, visualization suite

**📖 Full comparison**: [v1.0 vs Current Branch](./v1.0_vs_current_comparison.md)

---

## 📚 Documentation by Topic

### Installation & Setup

- [Quick Start](./getting-started/quick-start.md) - Build and run your first simulation
- [FAQ](./getting-started/faq.md) - Platform-specific installation issues
- [Contributing](./getting-started/contributing.md) - Development environment setup

### Running Simulations

- [Parameter Reference](./user-guide/parameter-reference.md) - Complete XML parameter documentation
- [Input Mesh Format](./user-guide/input-mesh-format.md) - Requirements for initial mesh files
- [Mesh Generation](./user-guide/mesh-generation.md) - Creating meshes with MeshLab/Blender
- [Output Format](./user-guide/output-format.md) - Understanding VTK and CSV outputs
- [Quick Start](./getting-started/quick-start.md) - CLI flags and workflow

### Understanding the Physics

- [Physics Model](./scientific/physics-model.md) - Mathematical formulation and equations
- [Parameter Guide](./scientific/parameter-guide.md) - Physical meaning of parameters
- [Reproducing Paper Results](./scientific/reproducing-paper-results.md) - Validation workflows

### Performance Optimization

- [Performance Tuning](./developer/performance-tuning.md) - OpenMP scheduling and thread configuration
- [Benchmarking Guide](./benchmarking/openmp-benchmarks.md) - Performance measurement framework
- [FAQ](./getting-started/faq.md) - Common performance issues

### Advanced Usage

- [Python Bindings](./user-guide/python-bindings.md) - Python API for batch simulations
- [Parameter Screening](./user-guide/parameter-screening.md) - Systematic parameter space exploration
- [Visualization Suite](./user-guide/visualization.md) - 12-plot statistical visualization package
- [Developer Explainer](./developer/explainer.md) - Codebase architecture

### Development & Contributing

- [Contributing Guide](./getting-started/contributing.md) - Code style, testing, PR workflow
- [Developer Explainer](./developer/explainer.md) - System architecture and design patterns
- [FAQ](./getting-started/faq.md) - Debugging techniques

### Historical Reference

- [v1.0 Documentation Archive](./docs-old/README.md) - Original v1.0 documentation
- [Version Comparison](./v1.0_vs_current_comparison.md) - What changed and why

---

## 🎯 Documentation by Use Case

### "I want to simulate epithelial tissue"

1. Start with [Quick Start](./getting-started/quick-start.md) to build the code
2. Read [Physics Model](./scientific/physics-model.md) section on cell-cell adhesion
3. Check [Parameter Guide](./scientific/parameter-guide.md) for typical epithelial values
4. Modify a [parameter file](./user-guide/parameter-reference.md)
5. If performance is slow, see [Performance Tuning](./developer/performance-tuning.md)

### "I want to match experimental data"

1. Understand parameters with [Parameter Guide](./scientific/parameter-guide.md)
2. Use [Python Bindings](./user-guide/python-bindings.md) for batch screening
3. Follow [Parameter Screening](./user-guide/parameter-screening.md) workflow
4. Validate with [Output Format](./user-guide/output-format.md) guide
5. Visualize results with [Visualization Suite](./user-guide/visualization.md)

### "I want to extend the code"

1. Read [Developer Explainer](./developer/explainer.md) for architecture overview
2. Follow [Contributing Guide](./getting-started/contributing.md) for setup
3. Check **Architecture Diagrams** *(coming soon)* for module structure
4. Write tests following patterns in `test/` directory
5. Benchmark changes with [Benchmarking Guide](./benchmarking/openmp-benchmarks.md)

### "I'm migrating from v1.0"

1. Read [Version Comparison](./v1.0_vs_current_comparison.md) to understand changes
2. Check [v1.0 Archive](./docs-old/README.md) for file mappings
3. Verify your parameter files still work (they should - 100% compatible!)
4. Optionally enable new features (adaptive scheduling, diagnostics)
5. See [FAQ](./getting-started/faq.md) for migration issues

---

## 🔍 Finding Information

**Can't find what you need?** Try these strategies:

1. **Search documentation**: Use `grep -r "keyword" doc/` to search all files
2. **Check FAQ**: [Common questions](./getting-started/faq.md) covers 80% of issues
3. **Browse code**: Well-commented headers in `include/` directory
4. **Check tests**: `test/` directory shows usage examples
5. **Ask for help**: Open an issue on GitHub with the `documentation` label

---

## 📝 Documentation Status

| Category | Status | Completion |
|----------|--------|------------|
| Getting Started | ✅ Complete | 3/3 guides |
| User Guide | ✅ Complete | 7/7 guides |
| Scientific | ✅ Complete | 3/3 guides |
| Developer | 🚧 In Progress | 2/3 guides |
| Benchmarking | ✅ Complete | 1/1 guides |

**Legend**: ✅ Complete | 🚧 In Progress | ⏳ Planned

---

## 🤝 Contributing to Documentation

Documentation improvements are highly valued! To contribute:

1. Check the [Contributing Guide](./getting-started/contributing.md)
2. Markdown files are in `doc/` directory
3. Images go in `doc/assets/img/`
4. Follow existing formatting conventions
5. Build and test before submitting PR

**Common documentation tasks**:
- Fix typos or broken links (always appreciated!)
- Add examples to existing guides
- Create tutorials for common workflows
- Document undocumented features
- Translate documentation (contact maintainers first)

---

## 📞 Getting Help

If this documentation doesn't answer your question:

- **Bug reports**: Open an issue on GitHub
- **Feature requests**: Open an issue with the `enhancement` label
- **Security issues**: See SECURITY.md *(if available)*
- **General questions**: Start a GitHub Discussion

---

**Welcome to SimuCell3D!** 🧬

Start your journey with the [Quick Start Guide](./getting-started/quick-start.md) →
