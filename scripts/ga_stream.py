"""
ga_stream.py — Real-time GA progress streaming for the web dashboard.

Integrates with the genetic_algorithm.py to broadcast progress
via WebSocket to the dashboard API.
"""

import asyncio
import json
import os
import sys
from pathlib import Path
from typing import Optional, Dict, Any

# Add scripts directory to path
sys.path.insert(0, str(Path(__file__).parent))

try:
    import httpx
    HAS_HTTPX = True
except ImportError:
    HAS_HTTPX = False


class GAStreamer:
    """Stream GA progress to the dashboard API."""

    def __init__(self, api_url: str = "http://localhost:8000", run_id: int = 1):
        self.api_url = api_url.rstrip("/")
        self.run_id = run_id
        self.ws = None
        self.connected = False

    async def connect(self) -> bool:
        """Connect to the WebSocket endpoint."""
        try:
            import websockets
            uri = f"{self.api_url.replace('http', 'ws')}/ws/ga/{self.run_id}"
            self.ws = await websockets.connect(uri)
            self.connected = True
            print(f"[GAStreamer] Connected to {uri}")
            return True
        except Exception as e:
            print(f"[GAStreamer] Connection failed: {e}")
            self.connected = False
            return False

    async def disconnect(self) -> None:
        """Disconnect from WebSocket."""
        if self.ws:
            await self.ws.close()
            self.connected = False

    async def send_progress(self, generation: int, best_fitness: float,
                           avg_fitness: float, diversity: float,
                           best_chromosome: str = "") -> None:
        """Send GA progress update."""
        if not self.connected or not self.ws:
            return

        try:
            message = {
                "type": "ga_update",
                "generation": generation,
                "best_fitness": best_fitness,
                "avg_fitness": avg_fitness,
                "diversity": diversity,
                "best_chromosome": best_chromosome,
            }
            await self.ws.send(json.dumps(message))

            # Also send via REST API as backup
            if HAS_HTTPX:
                await self._send_rest_progress(message)
        except Exception as e:
            print(f"[GAStreamer] Send failed: {e}")
            self.connected = False

    async def _send_rest_progress(self, message: Dict[str, Any]) -> None:
        """Send progress via REST API as backup."""
        try:
            async with httpx.AsyncClient() as client:
                await client.post(
                    f"{self.api_url}/api/ga/{self.run_id}/progress",
                    json=message,
                    timeout=5.0,
                )
        except Exception:
            pass

    async def send_complete(self, best_fitness: float, best_chromosome: str) -> None:
        """Send GA completion notification."""
        if not self.connected or not self.ws:
            return

        try:
            message = {
                "type": "ga_complete",
                "best_fitness": best_fitness,
                "best_chromosome": best_chromosome,
            }
            await self.ws.send(json.dumps(message))
        except Exception as e:
            print(f"[GAStreamer] Send complete failed: {e}")


# Synchronous wrapper for use in non-async code
class SyncGAStreamer:
    """Synchronous wrapper for GA streaming."""

    def __init__(self, api_url: str = "http://localhost:8000", run_id: int = 1):
        self.streamer = GAStreamer(api_url, run_id)
        self._loop = None

    def start(self) -> None:
        """Start the event loop and connect."""
        try:
            self._loop = asyncio.get_event_loop()
        except RuntimeError:
            self._loop = asyncio.new_event_loop()
            asyncio.set_event_loop(self._loop)
        self._loop.run_until_complete(self.streamer.connect())

    def stop(self) -> None:
        """Stop the streamer."""
        if self._loop and self.streamer.connected:
            self._loop.run_until_complete(self.streamer.disconnect())

    def progress(self, generation: int, best_fitness: float,
                 avg_fitness: float, diversity: float,
                 best_chromosome: str = "") -> None:
        """Send progress update (synchronous)."""
        if self._loop and self.streamer.connected:
            self._loop.run_until_complete(
                self.streamer.send_progress(
                    generation, best_fitness, avg_fitness, diversity, best_chromosome
                )
            )

    def complete(self, best_fitness: float, best_chromosome: str) -> None:
        """Send completion notification (synchronous)."""
        if self._loop and self.streamer.connected:
            self._loop.run_until_complete(
                self.streamer.send_complete(best_fitness, best_chromosome)
            )


# Example usage
if __name__ == '__main__':
    async def demo():
        streamer = GAStreamer(run_id=1)
        await streamer.connect()

        for gen in range(10):
            await streamer.send_progress(
                generation=gen,
                best_fitness=0.5 + gen * 0.05,
                avg_fitness=0.3 + gen * 0.03,
                diversity=0.8 - gen * 0.02,
                best_chromosome="[1,2,3,...]",
            )
            await asyncio.sleep(0.5)

        await streamer.send_complete(0.95, "[1,2,3,...]")
        await streamer.disconnect()

    asyncio.run(demo())
