"""
exporter.py — Export simulation results to various formats.

Supported formats:
  - JSON (native)
  - CSV (spreadsheet)
  - PDF (via reportlab, optional)
  - HTML (self-contained report)
"""

import json
import csv
import os
from pathlib import Path
from typing import Dict, List, Any, Optional
from datetime import datetime


class Exporter:
    """Export simulation data to various formats."""

    def __init__(self, data: Dict[str, Any], output_dir: str = "exports"):
        self.data = data
        self.output_dir = Path(output_dir)
        self.output_dir.mkdir(exist_ok=True)

    def to_json(self, filename: str) -> str:
        """Export to JSON format."""
        path = self.output_dir / filename
        with open(path, 'w') as f:
            json.dump(self.data, f, indent=2, default=str)
        return str(path)

    def to_csv(self, filename: str) -> str:
        """Export to CSV format (flattened)."""
        path = self.output_dir / filename

        with open(path, 'w', newline='') as f:
            writer = csv.writer(f)

            # Write header
            writer.writerow([
                'run_id', 'scenario', 'step', 'agent_type', 'agent_id',
                'x', 'y', 'alive', 'detected', 'state'
            ])

            # Write agent states
            for step in self.data.get('steps', []):
                step_num = step.get('step', 0)
                for agent_type in ['seekers', 'targets', 'detectors', 'interceptors', 'attackers']:
                    for agent in step.get(agent_type, []):
                        writer.writerow([
                            self.data.get('metadata', {}).get('runId', 0),
                            self.data.get('metadata', {}).get('scenarioName', ''),
                            step_num,
                            agent_type.rstrip('s'),
                            agent.get('id', 0),
                            agent.get('x', 0),
                            agent.get('y', 0),
                            agent.get('alive', True),
                            agent.get('detected', False),
                            agent.get('state', ''),
                        ])

        return str(path)

    def to_html(self, filename: str) -> str:
        """Export to self-contained HTML report."""
        path = self.output_dir / filename

        html = f"""<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>UUV Simulation Report</title>
    <style>
        body {{ font-family: Arial, sans-serif; margin: 40px; background: #f5f5f5; }}
        .container {{ max-width: 1200px; margin: 0 auto; background: white; padding: 30px; border-radius: 8px; }}
        h1 {{ color: #1e293b; border-bottom: 2px solid #3b82f6; padding-bottom: 10px; }}
        h2 {{ color: #475569; margin-top: 30px; }}
        .stats {{ display: grid; grid-template-columns: repeat(auto-fit, minmax(200px, 1fr)); gap: 20px; margin: 20px 0; }}
        .stat {{ background: #f8fafc; padding: 15px; border-radius: 6px; border-left: 4px solid #3b82f6; }}
        .stat-label {{ font-size: 0.875rem; color: #64748b; }}
        .stat-value {{ font-size: 1.5rem; font-weight: bold; color: #1e293b; }}
        table {{ width: 100%; border-collapse: collapse; margin-top: 10px; }}
        th, td {{ padding: 10px; text-align: left; border-bottom: 1px solid #e2e8f0; }}
        th {{ background: #f8fafc; font-weight: 600; }}
        .timestamp {{ color: #94a3b8; font-size: 0.875rem; }}
    </style>
</head>
<body>
    <div class="container">
        <h1>UUV Simulation Report</h1>
        <p class="timestamp">Generated: {datetime.now().strftime('%Y-%m-%d %H:%M:%S')}</p>

        <h2>Run Summary</h2>
        <div class="stats">
            <div class="stat">
                <div class="stat-label">Scenario</div>
                <div class="stat-value">{self.data.get('metadata', {}).get('scenarioName', 'N/A')}</div>
            </div>
            <div class="stat">
                <div class="stat-label">Total Steps</div>
                <div class="stat-value">{self.data.get('metadata', {}).get('maxSteps', 0)}</div>
            </div>
            <div class="stat">
                <div class="stat-label">Seed</div>
                <div class="stat-value">{self.data.get('metadata', {}).get('seed', 'N/A')}</div>
            </div>
        </div>

        <h2>Results</h2>
        <table>
            <tr><th>Metric</th><th>Value</th></tr>
            <tr><td>Targets Destroyed</td><td>{self.data.get('results', {}).get('targets_destroyed', 0)} / {self.data.get('results', {}).get('total_targets', 0)}</td></tr>
            <tr><td>Detection Rate</td><td>{(self.data.get('results', {}).get('probability_detected', 0) * 100):.1f}%</td></tr>
            <tr><td>Kill Rate</td><td>{(self.data.get('results', {}).get('probability_killed', 0) * 100):.1f}%</td></tr>
            <tr><td>Blue Cost</td><td>${self.data.get('results', {}).get('blue_cost', 0):,.2f}</td></tr>
            <tr><td>Red Cost</td><td>${self.data.get('results', {}).get('red_cost', 0):,.2f}</td></tr>
            <tr><td>Loss Exchange Ratio</td><td>{self.data.get('results', {}).get('loss_exchange_ratio', 0):.2f}</td></tr>
        </table>

        <h2>Steps</h2>
        <p>Total steps recorded: {len(self.data.get('steps', []))}</p>
    </div>
</body>
</html>"""

        with open(path, 'w') as f:
            f.write(html)
        return str(path)

    def to_pdf(self, filename: str) -> Optional[str]:
        """Export to PDF (requires reportlab)."""
        try:
            from reportlab.lib import colors
            from reportlab.lib.pagesizes import letter
            from reportlab.platypus import SimpleDocTemplate, Table, TableStyle, Paragraph
            from reportlab.lib.styles import getSampleStyleSheet
        except ImportError:
            print("reportlab not installed. Install with: pip install reportlab")
            return None

        path = self.output_dir / filename
        doc = SimpleDocTemplate(str(path), pagesize=letter)

        styles = getSampleStyleSheet()
        elements = []

        elements.append(Paragraph("UUV Simulation Report", styles['Title']))
        elements.append(Paragraph(f"Generated: {datetime.now()}", styles['Normal']))

        # Results table
        results = self.data.get('results', {})
        data = [
            ['Metric', 'Value'],
            ['Targets Destroyed', f"{results.get('targets_destroyed', 0)} / {results.get('total_targets', 0)}"],
            ['Detection Rate', f"{(results.get('probability_detected', 0) * 100):.1f}%"],
            ['Kill Rate', f"{(results.get('probability_killed', 0) * 100):.1f}%"],
            ['Blue Cost', f"${results.get('blue_cost', 0):,.2f}"],
            ['Red Cost', f"${results.get('red_cost', 0):,.2f}"],
        ]

        table = Table(data)
        table.setStyle(TableStyle([
            ('BACKGROUND', (0, 0), (-1, 0), colors.grey),
            ('TEXTCOLOR', (0, 0), (-1, 0), colors.whitesmoke),
            ('ALIGN', (0, 0), (-1, -1), 'LEFT'),
            ('FONTNAME', (0, 0), (-1, 0), 'Helvetica-Bold'),
            ('FONTSIZE', (0, 0), (-1, 0), 14),
            ('BOTTOMPADDING', (0, 0), (-1, 0), 12),
            ('BACKGROUND', (0, 1), (-1, -1), colors.beige),
            ('GRID', (0, 0), (-1, -1), 1, colors.black),
        ]))

        elements.append(table)
        doc.build(elements)
        return str(path)

    def export_all(self, base_name: str) -> Dict[str, str]:
        """Export to all available formats."""
        results = {}

        results['json'] = self.to_json(f"{base_name}.json")
        results['csv'] = self.to_csv(f"{base_name}.csv")
        results['html'] = self.to_html(f"{base_name}.html")

        pdf_path = self.to_pdf(f"{base_name}.pdf")
        if pdf_path:
            results['pdf'] = pdf_path

        return results


def export_recording(recording_path: str, output_dir: str = "exports") -> Dict[str, str]:
    """Export a recording file to all formats."""
    with open(recording_path, 'r') as f:
        data = json.load(f)

    exporter = Exporter(data, output_dir)
    base_name = Path(recording_path).stem
    return exporter.export_all(base_name)


if __name__ == '__main__':
    import argparse

    parser = argparse.ArgumentParser(description="Export simulation data")
    parser.add_argument("input", help="Input JSON file")
    parser.add_argument("--output-dir", default="exports", help="Output directory")
    args = parser.parse_args()

    paths = export_recording(args.input, args.output_dir)
    print("Exported to:")
    for fmt, path in paths.items():
        print(f"  {fmt}: {path}")
