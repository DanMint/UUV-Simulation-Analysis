# UUV Simulation Analysis - Web Dashboard

Real-time web dashboard for monitoring and analyzing UUV simulation runs and Genetic Algorithm optimization progress.

## Features

- **Real-time Monitoring**: Live simulation metrics and agent tracking
- **GA Progress**: Real-time genetic algorithm optimization progress via WebSocket
- **Interactive Visualizations**: Chart.js powered charts for positions, fitness trends, and costs
- **Event Timeline**: Detection, interception, and target destruction events
- **Multi-format Export**: JSON, CSV, HTML, and PDF reports
- **Replay**: Step-by-step simulation replay from recorded data

## Quick Start

### 1. Install Dependencies

```bash
pip install -r scripts/requirements_dashboard.txt
```

### 2. Start the API Server

```bash
cd scripts
python dashboard_api.py
```

The API will be available at `http://localhost:8000`.

### 3. Open the Dashboard

Navigate to `http://localhost:8000` in your browser.

## API Endpoints

| Method | Endpoint | Description |
|--------|----------|-------------|
| GET | `/api/runs` | List all simulation runs |
| GET | `/api/runs/{run_id}` | Get run details |
| GET | `/api/runs/{run_id}/steps` | Get all steps for a run |
| GET | `/api/runs/{run_id}/steps/{step}` | Get specific step |
| GET | `/api/runs/{run_id}/events` | Get events for a run |
| GET | `/api/ga/{run_id}/history` | Get GA history |
| GET | `/api/ga/{run_id}/pareto` | Get Pareto front data |
| GET | `/api/analysis/fitness` | Get fitness trends |
| GET | `/api/analysis/costs` | Get cost analysis |
| GET | `/api/analysis/pareto` | Get Pareto analysis |
| POST | `/api/import` | Import recording JSON |
| WS | `/ws/ga/{run_id}` | WebSocket for GA progress |

## Recording Simulations

To record simulation runs for the dashboard:

```bash
# Run simulation with recording enabled
./uuv_sim.exe --scenario scenarios/diveld_baseline_complete.json \
              --repeat 1 --seed 42 --no-prompt --record

# Recordings are saved to runs/recording_0.json
```

Then import to the database:

```bash
curl -X POST http://localhost:8000/api/import \
  -H "Content-Type: application/json" \
  -d '{"recording_path": "runs/recording_0.json"}'
```

## GA Integration

To stream GA progress to the dashboard:

```python
from scripts.ga_stream import SyncGAStreamer

streamer = SyncGAStreamer(api_url="http://localhost:8000", run_id=1)
streamer.start()

# In your GA loop:
for gen in range(generations):
    # ... evaluate generation ...
    streamer.progress(
        generation=gen,
        best_fitness=best_fitness,
        avg_fitness=avg_fitness,
        diversity=diversity,
    )

streamer.complete(best_fitness, best_chromosome)
streamer.stop()
```

## Exporting Data

```python
from scripts.exporter import export_recording

# Export a recording to all formats
paths = export_recording("runs/recording_0.json", "exports")
# Returns: {'json': '...', 'csv': '...', 'html': '...', 'pdf': '...'}
```

## Architecture

```
┌─────────────────┐     WebSocket      ┌──────────────────┐
│  C++ Simulator  │────────────────────▶│  FastAPI Backend │
│  (uuv_sim.exe)  │  JSON recordings   │  (dashboard_api) │
└─────────────────┘                    └────────┬─────────┘
                                               │
                                               │ REST API
                                               ▼
                                        ┌──────────────┐
                                        │   Dashboard  │
                                        │   (HTML/JS)  │
                                        └──────────────┘
                                               │
                                        ┌──────▼──────┐
                                        │  SQLite DB  │
                                        │  (uuv_sim)  │
                                        └─────────────┘
```

## Data Flow

1. **C++ Simulator** runs simulation with `--record` flag
2. **SimulationRecorder** captures step-by-step state to JSON
3. **FastAPI Backend** ingests recordings into SQLite
4. **Web Dashboard** visualizes data via REST API and WebSocket
5. **GA Streamer** broadcasts optimization progress in real-time

## Configuration

Environment variables:
- `UUV_DB_PATH`: Path to SQLite database (default: `data/uuv_sim.db`)
- `UUV_API_URL`: API base URL (default: `http://localhost:8000`)
