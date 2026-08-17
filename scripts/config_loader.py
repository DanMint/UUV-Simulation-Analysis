"""
config_loader.py — Unified configuration loader for UUV Simulation Analysis.

Supports loading scenarios from:
  - JSON (native format)
  - YAML (human-friendly, with comments)
  - TOML (modern config format)

All formats map to the same internal dictionary structure used by the
simulation engine and GA tools.
"""

import json
import os
from pathlib import Path
from typing import Any, Dict, Optional

try:
    import yaml
    HAS_YAML = True
except ImportError:
    HAS_YAML = False

try:
    import tomllib  # Python 3.11+
    HAS_TOML = True
except ImportError:
    try:
        import tomli as tomllib  # backport
        HAS_TOML = True
    except ImportError:
        HAS_TOML = False


class ConfigLoader:
    """Unified configuration loader supporting JSON, YAML, and TOML."""

    @staticmethod
    def load(path: str) -> Dict[str, Any]:
        """Load a configuration file. Format is detected from extension."""
        p = Path(path)
        ext = p.suffix.lower()

        if ext in ('.yaml', '.yml'):
            return ConfigLoader.load_yaml(path)
        elif ext == '.toml':
            return ConfigLoader.load_toml(path)
        elif ext == '.json':
            return ConfigLoader.load_json(path)
        else:
            # Try JSON first, then YAML
            try:
                return ConfigLoader.load_json(path)
            except (json.JSONDecodeError, FileNotFoundError):
                if HAS_YAML:
                    return ConfigLoader.load_yaml(path)
                raise ValueError(f"Unknown config format: {path}")

    @staticmethod
    def load_json(path: str) -> Dict[str, Any]:
        """Load JSON configuration."""
        with open(path, 'r', encoding='utf-8') as f:
            return json.load(f)

    @staticmethod
    def save_json(data: Dict[str, Any], path: str, indent: int = 2) -> None:
        """Save configuration as JSON."""
        with open(path, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=indent)

    @staticmethod
    def load_yaml(path: str) -> Dict[str, Any]:
        """Load YAML configuration. Requires PyYAML."""
        if not HAS_YAML:
            raise ImportError("PyYAML is required for YAML support. Install with: pip install pyyaml")
        with open(path, 'r', encoding='utf-8') as f:
            return yaml.safe_load(f)

    @staticmethod
    def save_yaml(data: Dict[str, Any], path: str) -> None:
        """Save configuration as YAML. Requires PyYAML."""
        if not HAS_YAML:
            raise ImportError("PyYAML is required for YAML support. Install with: pip install pyyaml")
        with open(path, 'w', encoding='utf-8') as f:
            yaml.dump(data, f, default_flow_style=False, sort_keys=False)

    @staticmethod
    def load_toml(path: str) -> Dict[str, Any]:
        """Load TOML configuration. Requires Python 3.11+ or tomli."""
        if not HAS_TOML:
            raise ImportError("TOML support requires Python 3.11+ or tomli. Install with: pip install tomli")
        with open(path, 'rb') as f:
            return tomllib.load(f)

    @staticmethod
    def convert(input_path: str, output_path: str) -> None:
        """Convert between configuration formats."""
        data = ConfigLoader.load(input_path)

        out_ext = Path(output_path).suffix.lower()
        if out_ext == '.json':
            ConfigLoader.save_json(data, output_path)
        elif out_ext in ('.yaml', '.yml'):
            ConfigLoader.save_yaml(data, output_path)
        elif out_ext == '.toml':
            raise NotImplementedError("TOML output not yet implemented")
        else:
            raise ValueError(f"Unsupported output format: {output_path}")

        print(f"Converted {input_path} -> {output_path}")


def main():
    import argparse

    parser = argparse.ArgumentParser(description="Configuration format converter")
    parser.add_argument("input", help="Input configuration file")
    parser.add_argument("output", help="Output configuration file")
    args = parser.parse_args()

    ConfigLoader.convert(args.input, args.output)


if __name__ == '__main__':
    main()
