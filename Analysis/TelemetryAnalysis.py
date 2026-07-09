import os
import glob
import json
from dataclasses import dataclass
from typing import Dict, Any

@dataclass
class UUVTelemetry:
    battery_percentage: float      # 0.0 to 100.0
    target_probability: float      # 0.0 to 1.0 
    cross_sensor_hits: int         # Number of auxiliary sensors flagging something
    ambient_noise_level: float     # Decibels
    distance_to_base_km: float     # For emergency energy calculations

class SensorSchedulistAgent:
    def __init__(self, critical_battery_threshold: float = 25.0):
        self.critical_battery_threshold = critical_battery_threshold

    def evaluate_cost_and_schedule(self, telemetry: UUVTelemetry) -> Dict[str, Any]:
        battery = telemetry.battery_percentage
        prob = telemetry.target_probability
        
        energy_scarcity_multiplier = 1.0 + (100.0 - battery) / 20.0
        
        sensor_mode = "Passive Acoustic (Low Power)"
        ping_interval_seconds = 60  
        estimated_energy_cost_weight = 1.0 * energy_scarcity_multiplier

        if battery < self.critical_battery_threshold:
            if prob < 0.7:
                sensor_mode = "Emergency Passive Only"
                ping_interval_seconds = -1  
                estimated_energy_cost_weight = 0.1
            else:
                sensor_mode = "Intermittent Active Sonar"
                ping_interval_seconds = 30 
                estimated_energy_cost_weight = 5.0 * energy_scarcity_multiplier
                
        elif prob >= 0.75:
            sensor_mode = "Continuous Active Sonar (High Power)"
            ping_interval_seconds = 5
            estimated_energy_cost_weight = 10.0 * energy_scarcity_multiplier
            
        elif prob >= 0.4:
            sensor_mode = "Standard Active Sonar"
            ping_interval_seconds = 15
            estimated_energy_cost_weight = 4.0 * energy_scarcity_multiplier

        return {
            "agent": "Sensor Schedulist",
            "actionable_mode": sensor_mode,
            "ping_interval_seconds": ping_interval_seconds,
            "energy_cost_severity_score": round(estimated_energy_cost_weight, 2),
            "recommendation": f"Set sensor array to {sensor_mode}."
        }

class RiskArbitratorAgent:
    def __init__(self, response_cost_usd: float = 50000.0):
        self.base_response_cost = response_cost_usd 

    def arbitrate_detection(self, telemetry: UUVTelemetry) -> Dict[str, Any]:
        primary_prob = telemetry.target_probability
        cross_hits = telemetry.cross_sensor_hits
        
        if cross_hits == 0:
            false_alarm_probability = max(0.1, 1.0 - (primary_prob * 0.5))
        elif cross_hits == 1:
            false_alarm_probability = max(0.05, 1.0 - (primary_prob * 0.8))
        else:
            false_alarm_probability = 0.02 
            
        expected_waste_cost = self.base_response_cost * false_alarm_probability
        
        if false_alarm_probability > 0.60:
            action = "IGNORE_AND_MONITOR"
            confidence = "Low (High Risk of Waste)"
        elif false_alarm_probability > 0.25:
            action = "REQUEST_CROSS_VERIFICATION"
            confidence = "Medium (Risk Deflected to Auxiliary Sensors)"
        else:
            action = "DEPLOY_TACTICAL_RESPONSE"
            confidence = "High (Cost Justified)"

        return {
            "agent": "Risk Arbitrator",
            "tactical_action": action,
            "confidence_level": confidence,
            "expected_false_alarm_probability": round(false_alarm_probability, 2),
            "financial_risk_exposure": f"${round(expected_waste_cost, 2)}",
            "reasoning": f"Cross-sensor verification is at {cross_hits} hits. Risk of wasted capital is {'unacceptable' if action != 'DEPLOY_TACTICAL_RESPONSE' else 'acceptable'}."
        }

# --- Dynamic JSON Data Parser ---
def extract_telemetry_from_latest_run(target_dir="runs") -> UUVTelemetry:
    """Parses the fresh simulation JSON to build real telemetry metrics."""
    json_files = glob.glob(os.path.join(target_dir, "*.json"))
    
    # Fallback if no simulation data exists yet
    if not json_files:
        return UUVTelemetry(100.0, 0.0, 0, 40.0, 0.0)

    # Grab the most recent run
    latest_file = max(json_files, key=os.path.getctime)
    
    with open(latest_file, 'r') as f:
        data = json.load(f)

    # 1. Translate Simulation Noise to Decibels
    sim_noise = float(data.get("noise_level", 0.0))
    ambient_decibels = 40.0 + (sim_noise * 15.0)

    # 2. Translate Seeker Success to Target Probability
    metrics = data.get("metrics", {})
    total_seekers = metrics.get("total_seekers", 1)
    seekers_detected = metrics.get("total_detected", 0)
    target_prob = min(1.0, seekers_detected / max(1, total_seekers))

    # 3. Translate Sensor Sightings to Cross-Hits
    cross_hits = 0
    detectors = data.get("detectors", [])
    if detectors:
        # Some JSON formats log "sighting_count" or keep it in a nested list
        for d in detectors:
            cross_hits += d.get("sighting_count", d.get("intercept_count", 0))
            
    # 4. Proxy Battery Drain from Average Path Steps
    avg_steps = metrics.get("avg_steps_to_target", 50)
    # Assume 200 steps is a max drain (0% battery)
    battery_level = max(5.0, 100.0 - (avg_steps / 200.0 * 100.0))
    
    # 5. Distance based on node path costs
    dist_km = avg_steps * 0.5 

    return UUVTelemetry(
        battery_percentage=round(battery_level, 1),
        target_probability=round(target_prob, 2),
        cross_sensor_hits=int(cross_hits),
        ambient_noise_level=round(ambient_decibels, 1),
        distance_to_base_km=round(dist_km, 1)
    )

if __name__ == "__main__":
    print("\n[Telemetry Parser] Extracting live data from latest simulation run...")
    current_telemetry = extract_telemetry_from_latest_run()
    
    print(f"  -> Extracted Battery: {current_telemetry.battery_percentage}%")
    print(f"  -> Extracted Target Prob: {current_telemetry.target_probability}")
    print(f"  -> Extracted Cross-Hits: {current_telemetry.cross_sensor_hits}")
    print(f"  -> Extracted Noise (dB): {current_telemetry.ambient_noise_level}")

    print("\n--- MULTI-AGENT TELEMETRY EVALUATION ---")
    
    schedulist = SensorSchedulistAgent()
    schedule_decision = schedulist.evaluate_cost_and_schedule(current_telemetry)
    print(f"\n[{schedule_decision['agent']}]")
    for key, value in schedule_decision.items():
        if key != "agent": print(f"  {key}: {value}")

    arbitrator = RiskArbitratorAgent()
    arbitration_decision = arbitrator.arbitrate_detection(current_telemetry)
    print(f"\n[{arbitration_decision['agent']}]")
    for key, value in arbitration_decision.items():
        if key != "agent": print(f"  {key}: {value}")
    print("\n========================================\n")