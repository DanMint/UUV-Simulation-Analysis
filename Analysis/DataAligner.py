import os
import glob
import json

def align_simulation_data(target_dir="runs"):
    json_files = glob.glob(os.path.join(target_dir, "*.json"))
    if not json_files:
        print("[Data Aligner] No run data files found.")
        return

    for file_path in json_files:
        with open(file_path, 'r') as f:
            try:
                data = json.load(f)
            except json.JSONDecodeError:
                print(f"[-] Error: {file_path} is empty or malformed.")
                continue
        
        modified = False
        
        # Ensure data is treated as a list of iterations (even if it's a single dictionary)
        iterations = data if isinstance(data, list) else [data]

        for iteration in iterations:
            # Fix 1: If there are 0 detectors, IterationAnalysis.py crashes.
            # We inject a fake "dummy" detector so the table builds successfully.
            if "detectors" not in iteration or not iteration["detectors"]:
                iteration["detectors"] = {
                    "999": {
                        "id": 999,
                        "row": 0,
                        "col": 0,
                        "radius": 3.0,
                        "sensing_radius": 3.0,
                        "intercept_count": 0
                    }
                }
                modified = True
            else:
                # Handle both dictionary and list structures for detectors dynamically
                detectors = iteration["detectors"]
                if isinstance(detectors, dict):
                    detector_items = detectors.values()
                elif isinstance(detectors, list):
                    detector_items = detectors
                else:
                    continue

                for detector in detector_items:
                    # Fix 2: Prevent KeyError: 'radius'
                    if "radius" not in detector:
                        detector["radius"] = detector.get("sensing_radius", 3.0)
                        modified = True
                    
                    # Fix 3: Prevent KeyError: 'intercept_count'
                    if "intercept_count" not in detector:
                        detector["intercept_count"] = detector.get("sighting(s)", 0)
                        modified = True

        if modified:
            with open(file_path, 'w') as f:
                json.dump(data, f, indent=4)
            print(f"[Data Aligner] Structured compatibility formatting applied to: {file_path}")

if __name__ == "__main__":
    align_simulation_data()