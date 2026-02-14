# Contributing to SimuCell3D

Thank you for your interest in contributing to SimuCell3D! This document provides guidelines for contributing code, documentation, and bug reports.

## Table of Contents

- [Getting Started](#getting-started)
- [Development Workflow](#development-workflow)
- [Code Style Guide](#code-style-guide)
- [Testing Requirements](#testing-requirements)
- [Pull Request Process](#pull-request-process)
- [Documentation](#documentation)
- [Reporting Issues](#reporting-issues)

---

## Getting Started

### 1. Set Up Development Environment

```bash
# Clone the repository
git clone https://github.com/nilesh-patil/simucell3d.git
cd SimuCell3D

# Create a feature branch
git checkout -b feature/your-feature-name

# Build in Debug mode for development
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Run tests to verify setup
ctest
```

### 2. Understand the Architecture

Before contributing, familiarize yourself with:

- **[explainer.md](../developer/explainer.md)** - Technical overview
- **[Parameter Reference](../user-guide/parameter-reference.md)** - Parameter system

### Key Directories

| Directory | Purpose |
|-----------|---------|
| `include/` | Header files (public interfaces) |
| `src/` | Implementation files |
| `test/` | Unit tests organized by module |
| `parameters/` | XML configuration files |
| `scripts/` | Build and benchmark scripts |

---

## Development Workflow

### Branch Naming

Use descriptive branch names with prefixes:

| Prefix | Purpose | Example |
|--------|---------|---------|
| `feature/` | New functionality | `feature/gpu-acceleration` |
| `fix/` | Bug fixes | `fix/contact-force-calculation` |
| `docs/` | Documentation only | `docs/parameter-guide` |
| `perf/` | Performance improvements | `perf/openmp-tuning` |
| `refactor/` | Code restructuring | `refactor/mesh-module` |

### Commit Messages

Follow conventional commit format:

```
type(scope): brief description

Longer explanation if needed.

Refs: #issue-number
```

**Types:** `feat`, `fix`, `docs`, `perf`, `refactor`, `test`, `chore`

**Examples:**
```
feat(contact): add face-face friction forces

Implements tangential friction forces for the face-face coupling
contact model. Uses Coulomb friction with configurable coefficient.

Refs: #123
```

```
fix(mesh): prevent edge collapse creating degenerate triangles

Adds validation check before edge collapse to ensure resulting
faces have non-zero area.
```

---

## Code Style Guide

### C++ Formatting

**General:**
- C++17 standard
- 4 spaces for indentation (no tabs)
- 100 character line limit
- Opening braces on same line

**Naming Conventions:**
```cpp
// Classes: PascalCase
class CellDivider { ... };

// Functions: snake_case
void compute_contact_forces();

// Variables: snake_case
double cell_volume = 0.0;

// Member variables: trailing underscore
double volume_;
Vec3 position_;

// Constants: UPPER_SNAKE_CASE
const double MAX_EDGE_LENGTH = 1e-5;

// Macros: UPPER_SNAKE_CASE
#define CONTACT_MODEL_INDEX 2
```

**Headers:**
```cpp
#ifndef SIMUCELL3D_MODULE_NAME_HPP
#define SIMUCELL3D_MODULE_NAME_HPP

#include <standard_library>    // Standard library first
#include "project_headers.hpp" // Project headers second

namespace simucell3d {
    // Code here
}

#endif // SIMUCELL3D_MODULE_NAME_HPP
```

### Example: Well-Formatted Function

```cpp
/**
 * @brief Compute pressure force on a cell node.
 *
 * Calculates the internal pressure force based on deviation from
 * target volume using the logarithmic equation of state.
 *
 * @param cell The cell containing this node
 * @param node_index Index of the node within the cell
 * @return Force vector in Newtons
 */
Vec3 compute_pressure_force(const Cell& cell, size_t node_index) {
    // Calculate pressure from volume deviation
    double volume_ratio = cell.volume() / cell.target_volume();
    double pressure = -cell.bulk_modulus() * std::log(volume_ratio);

    // Get face contribution for this node
    Vec3 force(0.0, 0.0, 0.0);
    for (const Face* face : cell.faces_containing_node(node_index)) {
        force += pressure * face->area() * face->normal() / 3.0;
    }

    return force;
}
```

### OpenMP Guidelines

- Use `#pragma omp parallel for` for parallelizable loops
- Prefer `schedule(runtime)` to allow runtime selection
- Document thread-safety assumptions in comments
- Test with `OMP_NUM_THREADS=1` to verify correctness

```cpp
// Good: Runtime scheduling allows user control
#pragma omp parallel for schedule(runtime)
for (size_t i = 0; i < cells.size(); ++i) {
    cells[i].update_forces();
}
```

---

## Testing Requirements

### Running Tests

```bash
cd build
ctest                    # Run all tests
ctest -V                 # Verbose output
ctest -R test_mesh       # Run specific test
ctest -j$(nproc)         # Parallel execution
```

### Writing Tests

- Add tests to `test/test_<module>/`
- Follow existing test patterns
- Test edge cases and error conditions

**Test Template:**
```cpp
#include <gtest/gtest.h>
#include "module_to_test.hpp"

class ModuleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup code
    }
};

TEST_F(ModuleTest, FunctionDoesExpectedBehavior) {
    // Arrange
    MyClass obj;

    // Act
    auto result = obj.function_under_test();

    // Assert
    EXPECT_DOUBLE_EQ(expected, result);
}
```

### Test Coverage Requirements

- New features must include unit tests
- Bug fixes should include regression tests
- Performance-critical code should have benchmarks

---

## Pull Request Process

### Before Submitting

1. **Rebase on main:** `git fetch origin && git rebase origin/main`
2. **Run tests:** `cd build && ctest`
3. **Check formatting:** Review code style compliance
4. **Update documentation:** If adding new features

### PR Template

```markdown
## Summary
Brief description of changes.

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Performance improvement
- [ ] Documentation
- [ ] Refactoring

## Testing
- [ ] All existing tests pass
- [ ] Added new tests for new functionality
- [ ] Tested manually with: [describe scenario]

## Checklist
- [ ] Code follows style guide
- [ ] Documentation updated
- [ ] No new compiler warnings
- [ ] Performance impact considered
```

### Review Process

1. Create PR with clear description
2. CI pipeline runs tests automatically
3. Maintainer reviews code
4. Address feedback, update PR
5. Merge when approved

---

## Documentation

### What to Document

- **New features:** Add to relevant doc file
- **API changes:** Update developer docs and header comments
- **Bug fixes:** Note in CHANGELOG.md (if significant)

### Documentation Style

- Use Markdown for all documentation
- Include code examples for complex features
- Link to related documentation

---

## Reporting Issues

### Bug Reports

Include:
1. **SimuCell3D version** (git commit or release)
2. **Operating system** and compiler version
3. **Steps to reproduce**
4. **Expected vs actual behavior**
5. **Relevant logs/output** (use code blocks)
6. **Parameter file** (if applicable)

### Feature Requests

Include:
1. **Use case description**
2. **Proposed solution**
3. **Alternatives considered**
4. **Impact on existing functionality**

---

## Questions?

- Check [faq.md](./faq.md) for common questions
- Review existing issues for similar topics
- Open a discussion for general questions

Thank you for contributing to SimuCell3D!
