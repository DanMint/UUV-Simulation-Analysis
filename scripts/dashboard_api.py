"""
main.py — FastAPI backend for UUV Simulation Analysis dashboard.

Endpoints:
  GET  /api/runs              - List all simulation runs
  GET  /api/runs/{run_id}     - Get run details
  GET  /api/runs/{run_id}/steps - Get all steps for a run
  GET  /api/runs/{run_id}/steps/{step} - Get specific step state
  GET  /api/runs/{run_id}/events - Get events for a run
  GET  /api/ga/{run_id}       - Get GA history for a run
  GET  /api/analysis/fitness  - Get fitness trend data
  GET  /api/analysis/costs    - Get cost breakdown
  GET  /api/analysis/pareto   - Get Pareto front data
  POST /api/import            - Import a recording JSON file
  WS   /ws/ga/{run_id}        - WebSocket for real-time GA progress
"""

import os
import sys
import json
import asyncio
from pathlib import Path
from typing import Optional, List, Dict, Any
from collections import defaultdict

from fastapi import FastAPI, WebSocket, WebSocketDisconnect, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from fastapi.responses import FileResponse
from contextlib import asynccontextmanager
from pydantic import BaseModel

# Add scripts directory to path
sys.path.insert(0, str(Path(__file__).parent))

from database import Database, import_recording

# ════════════════════════════════════════════════════════════════════════════════
#  APP SETUP
# ════════════════════════════════════════════════════════════════════════════════

# Database instance
db = Database(os.environ.get("UUV_DB_PATH", "data/uuv_sim.db"))

# WebSocket connection manager
class ConnectionManager:
    def __init__(self):
        self.active_connections: Dict[int, List[WebSocket]] = defaultdict(list)

    async def connect(self, run_id: int, websocket: WebSocket):
        await websocket.accept()
        self.active_connections[run_id].append(websocket)

    def disconnect(self, run_id: int, websocket: WebSocket):
        if run_id in self.active_connections:
            self.active_connections[run_id].remove(websocket)

    async def broadcast(self, run_id: int, message: Dict[str, Any]):
        if run_id in self.active_connections:
            for connection in self.active_connections[run_id]:
                try:
                    await connection.send_json(message)
                except Exception:
                    pass

manager = ConnectionManager()

@asynccontextmanager
async def lifespan(app: FastAPI):
    yield
    db.close()

app = FastAPI(
    title="UUV Simulation Analysis API",
    description="REST API for simulation results, GA progress, and analysis",
    version="1.0.0",
    lifespan=lifespan,
)

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_credentials=True,
    allow_methods=["*"],
    allow_headers=["*"],
)

# ════════════════════════════════════════════════════════════════════════════════
#  MODELS
# ════════════════════════════════════════════════════════════════════════════════

class RunSummary(BaseModel):
    run_id: int
    scenario_name: str
    seed: Optional[int]
    total_steps: int
    targets_destroyed: int
    total_targets: int
    all_targets_destroyed: bool
    probability_detected: float
    probability_killed: float
    blue_cost: float
    red_cost: float
    loss_exchange_ratio: float
    created_at: str

class StepState(BaseModel):
    step: int
    agents: List[Dict[str, Any]]

class Event(BaseModel):
    id: int
    step: int
    event_type: str
    agent_a_id: Optional[int]
    agent_b_id: Optional[int]
    metadata: Optional[str]

class GAGeneration(BaseModel):
    generation: int
    best_fitness: float
    avg_fitness: float
    diversity: float
    best_chromosome: str

class ImportRequest(BaseModel):
    recording_path: str

# ════════════════════════════════════════════════════════════════════════════════
#  RUN ENDPOINTS
# ════════════════════════════════════════════════════════════════════════════════

@app.get("/api/runs", response_model=List[RunSummary])
async def list_runs(limit: int = 100):
    """List all simulation runs."""
    runs = db.list_runs(limit)
    return [
        RunSummary(
            run_id=r["run_id"],
            scenario_name=r["scenario_name"],
            seed=r["seed"],
            total_steps=r["total_steps"],
            targets_destroyed=r["targets_destroyed"],
            total_targets=r["total_targets"],
            all_targets_destroyed=bool(r["all_targets_destroyed"]),
            probability_detected=r["probability_detected"] or 0.0,
            probability_killed=r["probability_killed"] or 0.0,
            blue_cost=r["blue_cost"] or 0.0,
            red_cost=r["red_cost"] or 0.0,
            loss_exchange_ratio=r["loss_exchange_ratio"] or 0.0,
            created_at=r["created_at"],
        )
        for r in runs
    ]

@app.get("/api/runs/{run_id}")
async def get_run(run_id: int):
    """Get detailed information about a specific run."""
    run = db.get_run(run_id)
    if not run:
        raise HTTPException(status_code=404, detail="Run not found")

    # Get cost breakdown
    costs = db.get_cost_breakdown(run_id)

    return {
        "run": run,
        "cost_breakdown": costs,
    }

@app.get("/api/runs/{run_id}/steps")
async def get_run_steps(run_id: int, step: Optional[int] = None):
    """Get agent states for a run, optionally filtered by step."""
    states = db.get_agent_states(run_id, step)

    # Group by step
    steps = defaultdict(list)
    for s in states:
        steps[s["step"]].append({
            "agent_type": s["agent_type"],
            "agent_id": s["agent_id"],
            "x": s["x"],
            "y": s["y"],
            "alive": bool(s["alive"]),
            "detected": bool(s["detected"]),
            "state": s["state"],
            "target_id": s["target_id"],
        })

    return [
        {"step": step_num, "agents": agents}
        for step_num, agents in sorted(steps.items())
    ]

@app.get("/api/runs/{run_id}/steps/{step}")
async def get_step(run_id: int, step: int):
    """Get a specific step's agent states."""
    states = db.get_agent_states(run_id, step)
    if not states:
        raise HTTPException(status_code=404, detail="Step not found")

    return {
        "step": step,
        "agents": [
            {
                "agent_type": s["agent_type"],
                "agent_id": s["agent_id"],
                "x": s["x"],
                "y": s["y"],
                "alive": bool(s["alive"]),
                "detected": bool(s["detected"]),
                "state": s["state"],
                "target_id": s["target_id"],
            }
            for s in states
        ],
    }

@app.get("/api/runs/{run_id}/events", response_model=List[Event])
async def get_run_events(run_id: int, step: Optional[int] = None):
    """Get events for a run."""
    events = db.get_events(run_id, step)
    return [
        Event(
            id=e["id"],
            step=e["step"],
            event_type=e["event_type"],
            agent_a_id=e["agent_a_id"],
            agent_b_id=e["agent_b_id"],
            metadata=e["metadata"],
        )
        for e in events
    ]

@app.get("/api/runs/{run_id}/export")
async def export_run(run_id: int, format: str = "json"):
    """Export run data in various formats."""
    run = db.get_run(run_id)
    if not run:
        raise HTTPException(status_code=404, detail="Run not found")

    run_dir = os.path.join("runs")
    base_name = f"run_{run_id}"

    if format == "json":
        path = os.path.join(run_dir, f"{base_name}.json")
        if not os.path.exists(path):
            raise HTTPException(status_code=404, detail="Run JSON not found")
        return FileResponse(path, media_type="application/json", filename=f"{base_name}.json")

    if format == "csv":
        path = os.path.join(run_dir, "summary.csv")
        if not os.path.exists(path):
            raise HTTPException(status_code=404, detail="Summary CSV not found")
        return FileResponse(path, media_type="text/csv", filename=f"{base_name}.csv")

    if format == "png":
        path = os.path.join("paths", f"run_{run_id}.png")
        if not os.path.exists(path):
            raise HTTPException(status_code=404, detail="Run plot not found")
        return FileResponse(path, media_type="image/png", filename=f"{base_name}.png")

    raise HTTPException(status_code=400, detail="Unsupported format. Use json, csv, or png.")

# ════════════════════════════════════════════════════════════════════════════════
#  GA ENDPOINTS
# ════════════════════════════════════════════════════════════════════════════════

@app.get("/api/ga/{run_id}/history", response_model=List[GAGeneration])
async def get_ga_history(run_id: int):
    """Get GA generation history for a run."""
    history = db.get_ga_history(run_id)
    return [
        GAGeneration(
            generation=h["generation"],
            best_fitness=h["best_fitness"],
            avg_fitness=h["avg_fitness"],
            diversity=h["diversity"],
            best_chromosome=h["best_chromosome"],
        )
        for h in history
    ]

@app.get("/api/ga/{run_id}/pareto")
async def get_ga_pareto(run_id: int):
    """Get Pareto front data for a run."""
    history = db.get_ga_history(run_id)

    # Extract fitness vs cost for Pareto analysis
    pareto_points = []
    for h in history:
        if h["best_chromosome"]:
            pareto_points.append({
                "generation": h["generation"],
                "best_fitness": h["best_fitness"],
                "diversity": h["diversity"],
            })

    return {
        "run_id": run_id,
        "pareto_points": pareto_points,
        "generations": len(history),
    }

# ════════════════════════════════════════════════════════════════════════════════
#  ANALYSIS ENDPOINTS
# ════════════════════════════════════════════════════════════════════════════════

@app.get("/api/analysis/fitness")
async def get_fitness_trends(run_id: Optional[int] = None):
    """Get fitness trend data across runs."""
    if run_id:
        history = db.get_ga_history(run_id)
        return {
            "run_id": run_id,
            "trends": [
                {
                    "generation": h["generation"],
                    "best_fitness": h["best_fitness"],
                    "avg_fitness": h["avg_fitness"],
                }
                for h in history
            ],
        }
    else:
        runs = db.list_runs(50)
        return {
            "runs": [
                {
                    "run_id": r["run_id"],
                    "fitness": r.get("probability_detected", 0.0)
                    * r.get("probability_killed", 0.0),
                    "created_at": r["created_at"],
                }
                for r in runs
            ],
        }

@app.get("/api/analysis/costs")
async def get_cost_analysis(run_id: int):
    """Get cost breakdown for a run."""
    run = db.get_run(run_id)
    if not run:
        raise HTTPException(status_code=404, detail="Run not found")

    costs = db.get_cost_breakdown(run_id)

    return {
        "run_id": run_id,
        "summary": {
            "blue_cost": run["blue_cost"],
            "red_cost": run["red_cost"],
            "loss_exchange_ratio": run["loss_exchange_ratio"],
        },
        "breakdown": costs,
    }

@app.get("/api/analysis/pareto")
async def get_pareto_front(run_id: int):
    """Get Pareto front for multi-objective analysis."""
    history = db.get_ga_history(run_id)

    # Extract objective values (fitness vs cost)
    objectives = []
    for h in history:
        if h["best_chromosome"]:
            objectives.append({
                "fitness": h["best_fitness"],
                "diversity": h["diversity"],
                "generation": h["generation"],
            })

    # Simple Pareto filtering (maximize fitness, maximize diversity)
    pareto = []
    for p in objectives:
        dominated = False
        for q in objectives:
            if (q["fitness"] >= p["fitness"] and
                q["diversity"] >= p["diversity"] and
                (q["fitness"] > p["fitness"] or q["diversity"] > p["diversity"])):
                dominated = True
                break
        if not dominated:
            pareto.append(p)

    return {
        "run_id": run_id,
        "pareto_front": pareto,
        "all_points": objectives,
    }

# ════════════════════════════════════════════════════════════════════════════════
#  IMPORT ENDPOINT
# ════════════════════════════════════════════════════════════════════════════════

@app.post("/api/import")
async def import_recording_file(request: ImportRequest):
    """Import a simulation recording JSON file into the database."""
    if not os.path.exists(request.recording_path):
        raise HTTPException(status_code=404, detail="Recording file not found")

    try:
        run_id = import_recording(db, request.recording_path)
        return {"run_id": run_id, "message": "Recording imported successfully"}
    except Exception as e:
        raise HTTPException(status_code=400, detail=str(e))

# ════════════════════════════════════════════════════════════════════════════════
#  WEBSOCKET FOR REAL-TIME GA PROGRESS
# ════════════════════════════════════════════════════════════════════════════════

@app.websocket("/ws/ga/{run_id}")
async def websocket_ga_progress(websocket: WebSocket, run_id: int):
    """WebSocket endpoint for real-time GA progress updates."""
    await manager.connect(run_id, websocket)
    try:
        while True:
            # Keep connection alive and handle client messages
            data = await websocket.receive_text()
            message = json.loads(data)

            if message.get("type") == "ping":
                await websocket.send_json({"type": "pong"})
            elif message.get("type") == "subscribe":
                # Send current GA state
                history = db.get_ga_history(run_id)
                if history:
                    latest = history[-1]
                    await websocket.send_json({
                        "type": "ga_update",
                        "generation": latest["generation"],
                        "best_fitness": latest["best_fitness"],
                        "avg_fitness": latest["avg_fitness"],
                        "diversity": latest["diversity"],
                    })
    except WebSocketDisconnect:
        manager.disconnect(run_id, websocket)

# Broadcast function for GA tools to call
async def broadcast_ga_update(run_id: int, generation: int,
                              best_fitness: float, avg_fitness: float,
                              diversity: float):
    """Broadcast GA progress to connected WebSocket clients."""
    await manager.broadcast(run_id, {
        "type": "ga_update",
        "generation": generation,
        "best_fitness": best_fitness,
        "avg_fitness": avg_fitness,
        "diversity": diversity,
    })

# ════════════════════════════════════════════════════════════════════════════════
#  STATIC FILES
# ════════════════════════════════════════════════════════════════════════════════

# Serve dashboard static files
dashboard_dir = Path(__file__).parent.parent / "dashboard"
if dashboard_dir.exists():
    app.mount("/static", StaticFiles(directory=str(dashboard_dir)), name="static")

@app.get("/")
async def serve_dashboard():
    """Serve the dashboard HTML."""
    index = Path(__file__).parent.parent / "dashboard" / "index.html"
    if index.exists():
        return FileResponse(str(index))
    return {"message": "UUV Simulation Analysis API", "docs": "/docs"}

# ════════════════════════════════════════════════════════════════════════════════
#  LIFECYCLE
# ════════════════════════════════════════════════════════════════════════════════

if __name__ == "__main__":
    import uvicorn
    uvicorn.run(app, host="0.0.0.0", port=8000)
