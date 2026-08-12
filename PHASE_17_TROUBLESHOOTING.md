# Phase 17 Developer Troubleshooting & FAQ

---

## Compilation Issues

### Issue: "vehicleSpecs.cpp not found"
**Symptoms**: CMake configuration fails  
**Cause**: test_diveld_scenario target references wrong path  
**Fix**:
```cmake
# Correct path (from windows_build/CMakeLists.txt)
add_executable(test_diveld_scenario
    ${CMAKE_SOURCE_DIR}/../tests/test_diveld_scenario.cpp
    ${CMAKE_SOURCE_DIR}/../src/agents/vehicleSpecs.cpp
)
```

---

### Issue: "undefined reference to `getVehicleSpecs`"
**Symptoms**: Linker error during test build  
**Cause**: vehicleSpecs.cpp not linked  
**Fix**:
```cmake
# Add to target_link_libraries
target_link_libraries(test_diveld_scenario PRIVATE
    vehicleSpecs  # or list src files directly
)
```

---

### Issue: "SFML headers not found"
**Symptoms**: `#include <SFML/...>` fails  
**Cause**: SFML not installed or path misconfigured  
**Fix**:
```bash
cd vcpkg
./vcpkg install sfml:x64-windows
# Then update CMakeLists.txt with:
set(SFML_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../vcpkg/installed/x64-windows/share/SFML")
find_package(SFML REQUIRED)
```

---

## Runtime Issues

### Issue: "diveld_baseline_complete.json not found"
**Symptoms**: Error loading scenario  
**Cause**: Working directory mismatch  
**Fix**:
```bash
# Run from root directory
cd "c:\Users\youar\OneDrive\Desktop\.vscode\UUV Fresh\UUV-Simulation-Analysis"
.\windows_build\build\Release\uuv_sim.exe --scenario scenarios/diveld_baseline_complete.json
```

Or use absolute path:
```bash
.\uuv_sim.exe --scenario C:\...\scenarios\diveld_baseline_complete.json
```

---

### Issue: "test_diveld_scenario.exe returns exit code 1"
**Symptoms**: Test fails  
**Cause**: One or more validation checks failed  
**Fix**:
1. Check Dive-LD specs in `src/agents/vehicleSpecs.cpp`
   - Verify: speedKnotsMin, speedKnotsMax, stepDelay, costs
2. Check scenario JSON syntax
   - Use: `python -m json.tool scenarios/diveld_baseline_complete.json`
3. Check critical target field
   - Should have: `"is_critical": true`
4. Verify attacker count
   - Should have ≥3 Dive-LD entries

---

### Issue: "Simulation runs but all runs have 0 steps"
**Symptoms**: run_0.json shows `"total_steps": 0`  
**Cause**: Attackers may be invalid or not spawning  
**Fix**:
1. Check attacker spawn positions are water cells (not land)
2. Verify target is reachable via A* pathfinding
3. Check JSON syntax with validator
4. Run with `--visualize` to debug placement

---

## Test Validation

### Issue: Test passes locally but fails in CI
**Symptoms**: GitHub Actions shows test failure  
**Cause**: Path differences between local and CI  
**Fix**:
```powershell
# Local: Use relative paths from workspace root
cd "c:\Users\youar\OneDrive\Desktop\.vscode\UUV Fresh\UUV-Simulation-Analysis"
.\windows_build\build\Release\test_diveld_scenario.exe

# CI: Adjust working directory in workflow
- name: Run tests
  working-directory: ./UUV-Simulation-Analysis
  run: |
    .\windows_build\build\Release\test_diveld_scenario.exe
```

---

### Issue: "Scenario loads but 0 detectors found"
**Symptoms**: Test fails on defender validation step  
**Cause**: JSON structure mismatch  
**Fix**: Verify JSON structure:
```json
{
  "defenders": [
    {
      "type": "detector",  // Must be exactly "detector"
      "row": 30,
      "col": 45,
      "radius": 4
    }
  ]
}
```

---

## Performance Issues

### Issue: Simulation runs very slowly
**Symptoms**: Each step takes >100ms  
**Cause**: Environmental noise calculation or visualization overhead  
**Fix**:
```bash
# Use headless mode (no visualizer)
uuv_sim --scenario diveld_baseline_complete.json --iterations 5 --no-prompt

# Reduce noise if testing
# Modify scenario: "max_noise_level": 0.0

# Check CPU usage in Task Manager
# If >80%, reduce attacker count or grid size
```

---

### Issue: GA series takes too long
**Symptoms**: 10 iterations + 5 noise steps = 50 total runs takes >10 minutes  
**Cause**: Each simulation runs to completion (up to 2000 steps)  
**Fix**:
```bash
# Reduce iteration count for quick testing
uuv_sim --scenario diveld_baseline_complete.json --iterations 2 --noise-step 0.2

# Or limit max steps in code:
# simulation.h: const int MAX_STEPS = 500; (reduce from 2000)

# For production, use parallelization:
# python: multiprocessing.Pool(8) to run 8 simulations in parallel
```

---

## Output Issues

### Issue: "runs/summary.csv is empty or only has header"
**Symptoms**: CSV file exists but no data rows  
**Cause**: Simulations may not have completed  
**Fix**:
```bash
# Check run JSON files exist
ls runs/run_*.json

# Check run_0.json for errors
cat runs/run_0.json | python -m json.tool | head -30

# Verify simulation completed (check total_steps > 0)
# If total_steps = 0, check attacker positions and target reachability
```

---

### Issue: "CSV has strange characters or encoding issues"
**Symptoms**: Excel shows gibberish, Python can't parse  
**Cause**: Line ending mismatch (CRLF vs LF)  
**Fix**:
```bash
# Convert to consistent format
dos2unix runs/summary.csv  # On Linux/Mac
python -c "
import pandas as pd
df = pd.read_csv('runs/summary.csv')
df.to_csv('runs/summary.csv', index=False, lineterminator='\n')
"
```

---

## Data Analysis Issues

### Issue: Loss-exchange-ratio is negative or infinite
**Symptoms**: LER calculation broken  
**Cause**: Division by zero (no blue cost, or red cost is 0)  
**Fix**:
```python
def calculate_ler(red_cost, blue_cost):
    if blue_cost == 0:
        return float('inf')  # Perfect defense
    if red_cost == 0:
        return 0.0  # Invalid
    return red_cost / blue_cost
```

---

### Issue: Visualize script fails on large result set
**Symptoms**: `python scripts/visualize.py runs/` crashes  
**Cause**: Memory exhaustion with 100+ result files  
**Fix**:
```bash
# Process subset
python scripts/visualize.py runs/run_0.json runs/run_1.json

# Or limit results
ls runs/run_*.json | head -10 | xargs python scripts/visualize.py
```

---

## Integration Issues

### Issue: "Can't import diveld_baseline in Python script"
**Symptoms**: Python can't load scenario JSON  
**Cause**: File path not found from script location  
**Fix**:
```python
import json
import os

# From root directory
scenario_path = os.path.join(
    os.path.dirname(__file__),
    '..', 'scenarios', 'diveld_baseline_complete.json'
)

with open(scenario_path, 'r') as f:
    scenario = json.load(f)
```

---

### Issue: "Git shows gitignore changes but coordination file still tracked"
**Symptoms**: .gitignore updated but COORDINATION_MESSAGE_FOR_DAN.txt still in git  
**Cause**: File was already tracked before gitignore added  
**Fix**:
```bash
git rm --cached COORDINATION_MESSAGE_FOR_DAN.txt
git rm --cached .INTERNAL_COORDINATION_NOTES.txt
git add .gitignore
git commit -m "Remove coordination files from tracking"
```

---

## Best Practices

### 1. Always Use Seed for Reproducibility
```bash
# Good: reproducible
uuv_sim --scenario diveld_baseline_complete.json --seed 42

# Bad: random each time
uuv_sim --scenario diveld_baseline_complete.json
```

### 2. Track Results with Metadata
```json
{
  "run_metadata": {
    "date": "2026-08-12",
    "version": "Phase 17",
    "scenario": "diveld_baseline_complete.json",
    "command": "uuv_sim --iterations 5 --noise-step 0.1 --seed 42"
  }
}
```

### 3. Document Scenario Changes
```bash
# When modifying scenario for testing
git branch feature/test-defender-placement
# Make modifications
# Don't commit to main branch until validated
```

### 4. Validate Before Large Runs
```bash
# Quick validation
uuv_sim --scenario modified.json --iterations 1 --seed 42 --no-prompt

# Only if successful, run full series
uuv_sim --scenario modified.json --iterations 10 --noise-step 0.05 --seed 42 --no-prompt
```

---

## Quick Diagnostic Checklist

When things aren't working, check in order:

- [ ] Scenario file exists: `ls scenarios/diveld_baseline_complete.json`
- [ ] JSON is valid: `python -m json.tool scenarios/diveld_baseline_complete.json`
- [ ] Test compiles: `cmake --build build --target test_diveld_scenario`
- [ ] Test runs: `.\build\Release\test_diveld_scenario.exe`
- [ ] Simulator compiles: `cmake --build build --target uuv_sim`
- [ ] Simulator runs: `uuv_sim --scenario diveld_baseline_complete.json --iterations 1 --seed 42 --no-prompt`
- [ ] Output exists: `ls runs/run_0.json`
- [ ] CSV updated: `tail runs/summary.csv`

If all pass, full system is functional.

---

## Getting Help

1. **Check existing issues**: [GitHub Issues](https://github.com/DanMint/UUV-Simulation-Analysis/issues)
2. **Review logs**: `build/CMakeOutput.log`, terminal output
3. **Run with verbose**: Use `--verbose` flag (if implemented)
4. **Check memory**: If crashes on large scenarios, may be out of memory
5. **Validate JSON**: Use online JSON validators or `python -m json.tool`

---
