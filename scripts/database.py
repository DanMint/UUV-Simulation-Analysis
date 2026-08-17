"""
database.py — SQLite database layer for UUV simulation results.

Stores:
  - Simulation runs (metadata, config, results)
  - Agent states per step (for replay)
  - GA generations (fitness, chromosomes, statistics)
  - Analysis results (cost breakdowns, Pareto fronts)
"""

import sqlite3
import json
import os
from datetime import datetime
from pathlib import Path
from typing import Optional, List, Dict, Any


class Database:
    """SQLite database manager for simulation data."""

    def __init__(self, db_path: str = "data/uuv_sim.db"):
        self.db_path = db_path
        os.makedirs(os.path.dirname(db_path), exist_ok=True)
        self.conn = sqlite3.connect(db_path, check_same_thread=False)
        self.conn.row_factory = sqlite3.Row
        self._init_tables()

    def _init_tables(self) -> None:
        """Create database tables if they don't exist."""
        cursor = self.conn.cursor()

        # Simulation runs
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS runs (
                run_id INTEGER PRIMARY KEY AUTOINCREMENT,
                scenario_name TEXT NOT NULL,
                seed INTEGER,
                max_steps INTEGER,
                noise_level REAL,
                wall_time_ms REAL,
                start_time TEXT,
                total_targets INTEGER,
                targets_destroyed INTEGER,
                total_steps INTEGER,
                all_targets_destroyed BOOLEAN,
                probability_detected REAL,
                probability_killed REAL,
                blue_cost REAL,
                red_cost REAL,
                loss_exchange_ratio REAL,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP
            )
        """)

        # Agent states per step (for replay)
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS agent_states (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                run_id INTEGER NOT NULL,
                step INTEGER NOT NULL,
                agent_type TEXT NOT NULL,
                agent_id INTEGER NOT NULL,
                x REAL NOT NULL,
                y REAL NOT NULL,
                alive BOOLEAN,
                detected BOOLEAN,
                state TEXT,
                target_id INTEGER,
                FOREIGN KEY (run_id) REFERENCES runs(run_id)
            )
        """)

        # GA generations
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS ga_generations (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                run_id INTEGER NOT NULL,
                generation INTEGER NOT NULL,
                best_fitness REAL,
                avg_fitness REAL,
                diversity REAL,
                best_chromosome TEXT,
                created_at TEXT DEFAULT CURRENT_TIMESTAMP,
                FOREIGN KEY (run_id) REFERENCES runs(run_id)
            )
        """)

        # GA individuals
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS ga_individuals (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                generation_id INTEGER NOT NULL,
                chromosome TEXT NOT NULL,
                fitness REAL,
                cost REAL,
                effectiveness REAL,
                FOREIGN KEY (generation_id) REFERENCES ga_generations(id)
            )
        """)

        # Cost breakdowns
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS cost_breakdowns (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                run_id INTEGER NOT NULL,
                agent_type TEXT NOT NULL,
                count INTEGER,
                total_cost REAL,
                avg_cost REAL,
                FOREIGN KEY (run_id) REFERENCES runs(run_id)
            )
        """)

        # Events (detections, intercepts, etc.)
        cursor.execute("""
            CREATE TABLE IF NOT EXISTS events (
                id INTEGER PRIMARY KEY AUTOINCREMENT,
                run_id INTEGER NOT NULL,
                step INTEGER NOT NULL,
                event_type TEXT NOT NULL,
                agent_a_id INTEGER,
                agent_b_id INTEGER,
                metadata TEXT,
                FOREIGN KEY (run_id) REFERENCES runs(run_id)
            )
        """)

        # Create indices for common queries
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_agent_states_run_step ON agent_states(run_id, step)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_ga_generations_run ON ga_generations(run_id)")
        cursor.execute("CREATE INDEX IF NOT EXISTS idx_events_run_step ON events(run_id, step)")

        self.conn.commit()

    def insert_run(self, metadata: Dict[str, Any], results: Dict[str, Any]) -> int:
        """Insert a simulation run and return its ID."""
        cursor = self.conn.cursor()
        cursor.execute("""
            INSERT INTO runs (
                scenario_name, seed, max_steps, noise_level, wall_time_ms,
                start_time, total_targets, targets_destroyed, total_steps,
                all_targets_destroyed, probability_detected, probability_killed,
                blue_cost, red_cost, loss_exchange_ratio
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, (
            metadata.get("scenario_name", ""),
            metadata.get("seed"),
            metadata.get("maxSteps"),
            metadata.get("noiseLevel"),
            metadata.get("wallTimeMs"),
            metadata.get("startTime"),
            results.get("total_targets"),
            results.get("targets_destroyed"),
            results.get("total_steps"),
            results.get("all_targets_destroyed"),
            results.get("probability_detected"),
            results.get("probability_killed"),
            results.get("blue_cost"),
            results.get("red_cost"),
            results.get("loss_exchange_ratio"),
        ))
        self.conn.commit()
        return cursor.lastrowid

    def insert_agent_states(self, run_id: int, step: int, states: List[Dict[str, Any]]) -> None:
        """Insert agent states for a single step."""
        cursor = self.conn.cursor()
        cursor.executemany("""
            INSERT INTO agent_states (
                run_id, step, agent_type, agent_id, x, y, alive, detected, state, target_id
            ) VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?)
        """, [
            (
                run_id,
                step,
                s.get("agent_type", ""),
                s.get("agent_id", 0),
                s.get("x", 0.0),
                s.get("y", 0.0),
                s.get("alive", True),
                s.get("detected", False),
                s.get("state", ""),
                s.get("target_id", -1),
            )
            for s in states
        ])
        self.conn.commit()

    def insert_ga_generation(self, run_id: int, generation: int,
                             best_fitness: float, avg_fitness: float,
                             diversity: float, best_chromosome: str) -> int:
        """Insert a GA generation record."""
        cursor = self.conn.cursor()
        cursor.execute("""
            INSERT INTO ga_generations (
                run_id, generation, best_fitness, avg_fitness, diversity, best_chromosome
            ) VALUES (?, ?, ?, ?, ?, ?)
        """, (run_id, generation, best_fitness, avg_fitness, diversity, best_chromosome))
        self.conn.commit()
        return cursor.lastrowid

    def insert_event(self, run_id: int, step: int, event_type: str,
                     agent_a: int = None, agent_b: int = None,
                     metadata: Dict = None) -> None:
        """Insert an event (detection, intercept, etc.)."""
        cursor = self.conn.cursor()
        cursor.execute("""
            INSERT INTO events (run_id, step, event_type, agent_a_id, agent_b_id, metadata)
            VALUES (?, ?, ?, ?, ?, ?)
        """, (run_id, step, event_type, agent_a, agent_b,
              json.dumps(metadata) if metadata else None))
        self.conn.commit()

    def get_run(self, run_id: int) -> Optional[Dict[str, Any]]:
        """Get a simulation run by ID."""
        cursor = self.conn.cursor()
        cursor.execute("SELECT * FROM runs WHERE run_id = ?", (run_id,))
        row = cursor.fetchone()
        return dict(row) if row else None

    def get_agent_states(self, run_id: int, step: Optional[int] = None) -> List[Dict]:
        """Get agent states for a run, optionally filtered by step."""
        cursor = self.conn.cursor()
        if step is not None:
            cursor.execute("""
                SELECT * FROM agent_states WHERE run_id = ? AND step = ?
            """, (run_id, step))
        else:
            cursor.execute("""
                SELECT * FROM agent_states WHERE run_id = ? ORDER BY step
            """, (run_id,))
        return [dict(row) for row in cursor.fetchall()]

    def get_ga_history(self, run_id: int) -> List[Dict]:
        """Get GA generation history for a run."""
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT * FROM ga_generations WHERE run_id = ? ORDER BY generation
        """, (run_id,))
        return [dict(row) for row in cursor.fetchall()]

    def get_events(self, run_id: int, step: Optional[int] = None) -> List[Dict]:
        """Get events for a run."""
        cursor = self.conn.cursor()
        if step is not None:
            cursor.execute("""
                SELECT * FROM events WHERE run_id = ? AND step = ?
            """, (run_id, step))
        else:
            cursor.execute("""
                SELECT * FROM events WHERE run_id = ? ORDER BY step
            """, (run_id,))
        return [dict(row) for row in cursor.fetchall()]

    def get_cost_breakdown(self, run_id: int) -> List[Dict]:
        """Get cost breakdown for a run."""
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT * FROM cost_breakdowns WHERE run_id = ?
        """, (run_id,))
        return [dict(row) for row in cursor.fetchall()]

    def list_runs(self, limit: int = 100) -> List[Dict]:
        """List recent simulation runs."""
        cursor = self.conn.cursor()
        cursor.execute("""
            SELECT * FROM runs ORDER BY created_at DESC LIMIT ?
        """, (limit,))
        return [dict(row) for row in cursor.fetchall()]

    def close(self) -> None:
        self.conn.close()


def import_recording(db: Database, recording_path: str) -> int:
    """Import a simulation recording JSON file into the database."""
    with open(recording_path, 'r') as f:
        data = json.load(f)

    meta = data.get("metadata", {})
    run_id = db.insert_run(
        metadata={
            "scenario_name": meta.get("scenarioName", ""),
            "seed": meta.get("seed"),
            "maxSteps": meta.get("maxSteps"),
            "noiseLevel": meta.get("noiseLevel"),
            "wallTimeMs": meta.get("wallTimeMs"),
            "startTime": meta.get("startTime"),
        },
        results={
            "total_targets": meta.get("totalTargets", 0),
            "targets_destroyed": 0,
            "total_steps": len(data.get("steps", [])),
            "all_targets_destroyed": False,
            "probability_detected": 0.0,
            "probability_killed": 0.0,
            "blue_cost": 0.0,
            "red_cost": 0.0,
            "loss_exchange_ratio": 0.0,
        }
    )

    # Import steps
    for step_data in data.get("steps", []):
        step = step_data.get("step", 0)
        states = []

        for agent_type, agents in [
            ("seeker", step_data.get("seekers", {}).items()),
            ("target", step_data.get("targets", {}).items()),
            ("detector", step_data.get("detectors", {}).items()),
            ("interceptor", step_data.get("interceptors", {}).items()),
            ("attacker", step_data.get("attackers", {}).items()),
        ]:
            for aid, astate in agents:
                states.append({
                    "agent_type": agent_type,
                    "agent_id": int(aid),
                    "x": astate.get("x", 0),
                    "y": astate.get("y", 0),
                    "alive": astate.get("alive", True),
                    "detected": astate.get("detected", False),
                    "state": str(astate.get("state", "")),
                    "target_id": astate.get("target_id", -1),
                })

        db.insert_agent_states(run_id, step, states)

    # Import events
    for event in data.get("eventStream", []):
        if len(event) >= 4:
            db.insert_event(run_id, event[0], event[1], event[2], event[3])

    return run_id
