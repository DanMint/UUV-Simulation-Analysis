# Contributing to UUV-Simulation-Analysis

## Getting Started

1. Clone the repository
2. Install dependencies (see README.md for platform-specific instructions)
3. Configure with CMake: `cmake -B build`
4. Build: `cmake --build build --config Release`

## Development Workflow

1. Create a feature branch from `main`
2. Make your changes
3. Add tests for new functionality
4. Run the test suite: `ctest --test-dir build --output-on-failure`
5. Ensure the code compiles without warnings
6. Update documentation as needed
7. Submit a pull request

## Code Standards

- C++20 standard
- Use existing code style (see `src/` for conventions)
- No compiler warnings (`/W4` on MSVC, `-Wall -Wextra` on GCC/Clang)
- All new features must have tests
- Use include guards (`#ifndef` / `#define`) in all headers

## Testing

```powershell
# Run all tests
ctest --test-dir build --output-on-failure

# Run a specific test
ctest --test-dir build -R test_simulation --output-on-failure
```

## Python Scripts

- Use `numpy` and `matplotlib` for data analysis
- Follow existing script conventions in `scripts/`
- Add `--help` support to all new CLI tools

## Questions?

Open an issue on GitHub or reach out to the team.
