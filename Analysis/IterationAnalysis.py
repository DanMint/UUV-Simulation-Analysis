#!/usr/bin/env python3
"""
UUV Simulation Iteration Analyzer
==================================
Analyzes defense effectiveness across simulation iterations.

Focus areas:
  1. Seeker interception rate  — what % of seekers are stopped
  2. Target destruction rate   — what % of targets survive
  3. Noise impact              — how environmental noise affects defense
  4. Detector utilization      — which detectors contribute vs sit idle

Outputs:
  - analysis_dashboard.png    (annotated multi-panel visualization)
  - analysis_report.md        (full markdown report with tables)
  - iteration_summaries.csv, seeker_details.csv, detector_utilization.csv,
    chokepoints.csv, noise_impact.csv

Usage:
  python analyze_iterations.py <json_dir> [output_dir]
"""

import json, glob, os, sys
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from matplotlib.gridspec import GridSpec
from collections import defaultdict

# ═══════════════════════════════════════════════════════════════════
# DATA LOADING
# ═══════════════════════════════════════════════════════════════════

def load_iterations(input_dir):
    files = sorted(glob.glob(os.path.join(input_dir, '*.json')))
    if not files:
        print(f"ERROR: No JSON files found in {input_dir}")
        sys.exit(1)
    iterations = []
    for f in files:
        with open(f) as fh:
            iterations.append(json.load(fh))
    print(f"Loaded {len(iterations)} iteration(s) from {input_dir}")
    return iterations

# ═══════════════════════════════════════════════════════════════════
# TABLE BUILDERS
# ═══════════════════════════════════════════════════════════════════

def build_summary_table(iterations):
    rows = []
    for i, it in enumerate(iterations):
        s = it['summary']
        intercept_steps = [sk['intercepted_at_step'] for sk in it['seekers'] if sk['intercepted']]
        path_costs = [sk['path_cost'] for sk in it['seekers']]
        nodes = [sk['nodes_expanded'] for sk in it['seekers']]
        steps = [sk['steps_taken'] for sk in it['seekers']]
        move_counts = [len(sk['move_history']) for sk in it['seekers']]

        rows.append({
            'iteration': i,
            'noise_level': s['max_noise_level'],
            'total_seekers': s['total_seekers'],
            'seekers_intercepted': s['seekers_intercepted'],
            'seekers_reached_target': s['seekers_that_reached'],
            'targets_destroyed': s['targets_destroyed'],
            'total_targets': s['total_targets'],
            'total_detectors': s['total_detectors'],
            'total_steps': s['total_steps'],
            'intercept_rate': s['seekers_intercepted'] / max(s['total_seekers'], 1),
            'target_destruction_rate': s['targets_destroyed'] / max(s['total_targets'], 1),
            'target_survival_rate': 1 - s['targets_destroyed'] / max(s['total_targets'], 1),
            'breach_rate': s['seekers_that_reached'] / max(s['total_seekers'], 1),
            'avg_intercept_step': np.mean(intercept_steps) if intercept_steps else np.nan,
            'min_intercept_step': min(intercept_steps) if intercept_steps else np.nan,
            'max_intercept_step': max(intercept_steps) if intercept_steps else np.nan,
            'avg_path_cost': np.mean(path_costs),
            'avg_nodes_expanded': np.mean(nodes),
            'avg_steps_taken': np.mean(steps),
            'avg_move_count': np.mean(move_counts),
            'move_overhead': np.mean(move_counts) / max(np.mean(steps), 1),
        })
    return pd.DataFrame(rows)


def build_seeker_table(iterations):
    rows = []
    for i, it in enumerate(iterations):
        for sk in it['seekers']:
            moves = sk['move_history']
            direction_changes = 0
            backtrack_count = 0
            if len(moves) > 2:
                for j in range(2, len(moves)):
                    dr_prev = moves[j-1][0] - moves[j-2][0]
                    dc_prev = moves[j-1][1] - moves[j-2][1]
                    dr_curr = moves[j][0] - moves[j-1][0]
                    dc_curr = moves[j][1] - moves[j-1][1]
                    if dr_curr != dr_prev or dc_curr != dc_prev:
                        direction_changes += 1
                    if dc_curr > 0:
                        backtrack_count += 1

            total_dist = 0.0
            for j in range(1, len(moves)):
                dr = moves[j][0] - moves[j-1][0]
                dc = moves[j][1] - moves[j-1][1]
                total_dist += np.sqrt(dr*dr + dc*dc)

            sld = 0
            if len(moves) >= 2:
                sld = np.sqrt((moves[-1][0]-moves[0][0])**2 + (moves[-1][1]-moves[0][1])**2)
            straightness = sld / max(total_dist, 1e-9)

            rows.append({
                'iteration': i,
                'noise_level': it['summary']['max_noise_level'],
                'seeker_id': sk['id'],
                'steps_taken': sk['steps_taken'],
                'move_count': len(moves),
                'path_cost': sk['path_cost'],
                'nodes_expanded': sk['nodes_expanded'],
                'intercepted': sk['intercepted'],
                'intercepted_by': sk.get('intercepted_by_detector', -1),
                'intercepted_at_step': sk.get('intercepted_at_step', -1),
                'reached_target': sk['reached_target'],
                'direction_changes': direction_changes,
                'backtrack_moves': backtrack_count,
                'straightness_ratio': round(straightness, 4),
                'path_efficiency': round(sk['steps_taken'] / max(len(moves), 1), 4),
            })
    return pd.DataFrame(rows)


def build_detector_table(iterations):
    det_stats = defaultdict(lambda: {
        'row': 0,
        'col': 0,
        'sensing_radius': 0,
        'total_sightings': 0,
        'active_iterations': 0,
        'unique_seekers_detected': set(),
    })

    for it in iterations:
        for d in it.get('detectors', []):
            did = d['id']

            det_stats[did]['row'] = d['row']
            det_stats[did]['col'] = d['col']
            det_stats[did]['sensing_radius'] = d.get(
                'sensing_radius',
                d.get('radius', 0),
            )

            sightings = d.get('sightings', [])
            sighting_count = d.get(
                'sighting_count',
                len(sightings),
            )

            det_stats[did]['total_sightings'] += sighting_count

            if sighting_count > 0:
                det_stats[did]['active_iterations'] += 1

            for sighting in sightings:
                seeker_id = sighting.get('seeker_id')

                if seeker_id is not None:
                    det_stats[did]['unique_seekers_detected'].add(
                        seeker_id
                    )

    n_iterations = len(iterations)
    rows = []

    for did in sorted(det_stats):
        ds = det_stats[did]

        rows.append({
            'detector_id': did,
            'row': ds['row'],
            'col': ds['col'],
            'sensing_radius': ds['sensing_radius'],
            'total_sightings': ds['total_sightings'],
            'unique_seekers_detected': len(
                ds['unique_seekers_detected']
            ),
            'active_iterations': ds['active_iterations'],
            'utilization_rate': round(
                ds['active_iterations'] /
                max(n_iterations, 1),
                4,
            ),
            'avg_sightings_per_iter': round(
                ds['total_sightings'] /
                max(n_iterations, 1),
                2,
            ),
        })

    columns = [
        'detector_id',
        'row',
        'col',
        'sensing_radius',
        'total_sightings',
        'unique_seekers_detected',
        'active_iterations',
        'utilization_rate',
        'avg_sightings_per_iter',
    ]

    return pd.DataFrame(rows, columns=columns)


def chokepoint_analysis(iterations):
    cell_visits = defaultdict(int)
    for it in iterations:
        for sk in it['seekers']:
            visited = set()
            for pos in sk['move_history']:
                key = (pos[0], pos[1])
                if key not in visited:
                    cell_visits[key] += 1
                    visited.add(key)
    total_paths = sum(len(it['seekers']) for it in iterations)
    hotspots = [(r, c, cnt, round(cnt / max(total_paths, 1), 4))
                for (r, c), cnt in cell_visits.items()]
    hotspots.sort(key=lambda x: -x[2])
    return pd.DataFrame(hotspots[:50], columns=['row', 'col', 'visit_count', 'visit_fraction'])


def noise_impact_table(summary_df):
    if summary_df['noise_level'].nunique() <= 1:
        return None
    grouped = summary_df.groupby('noise_level').agg({
        'intercept_rate': 'mean',
        'target_destruction_rate': 'mean',
        'breach_rate': 'mean',
        'avg_intercept_step': 'mean',
        'avg_path_cost': 'mean',
        'avg_nodes_expanded': 'mean',
        'move_overhead': 'mean',
    }).reset_index()
    for col in grouped.columns[1:]:
        grouped[col] = grouped[col].round(4)
    return grouped


def compute_aggregate_stats(summary_df, seeker_df):
    intercepted = seeker_df[seeker_df['intercepted']]
    reached = seeker_df[seeker_df['reached_target']]
    return {
        'n_iterations': len(summary_df),
        'n_seekers_total': len(seeker_df),
        'n_seekers_intercepted': len(intercepted),
        'n_seekers_reached': len(reached),
        'overall_intercept_rate': seeker_df['intercepted'].mean(),
        'overall_breach_rate': seeker_df['reached_target'].mean(),
        'overall_target_destruction_rate': summary_df['target_destruction_rate'].mean(),
        'overall_target_survival_rate': summary_df['target_survival_rate'].mean(),
        'mean_intercept_step': intercepted['intercepted_at_step'].mean() if len(intercepted) > 0 else np.nan,
        'std_intercept_step': intercepted['intercepted_at_step'].std() if len(intercepted) > 0 else np.nan,
        'min_intercept_step': intercepted['intercepted_at_step'].min() if len(intercepted) > 0 else np.nan,
        'max_intercept_step': intercepted['intercepted_at_step'].max() if len(intercepted) > 0 else np.nan,
        'mean_path_cost': seeker_df['path_cost'].mean(),
        'std_path_cost': seeker_df['path_cost'].std(),
        'mean_nodes_expanded': seeker_df['nodes_expanded'].mean(),
        'std_nodes_expanded': seeker_df['nodes_expanded'].std(),
    }

# ═══════════════════════════════════════════════════════════════════
# VISUALIZATION — Defense-Focused Dashboard
# ═══════════════════════════════════════════════════════════════════

def _add_description(ax, text, fontsize=7.5):
    """Add a small description below the title explaining what the graph shows."""
    ax.text(0.5, 1.02, text, transform=ax.transAxes, fontsize=fontsize,
            ha='center', va='bottom', color='#555555', style='italic')


def _smooth(series, window=5):
    """Rolling average for noisy line plots."""
    if len(series) < window:
        return series
    return series.rolling(window, center=True, min_periods=1).mean()


def generate_dashboard(summary_df, seeker_df, detector_df, chokepoint_df, output_dir):
    fig = plt.figure(figsize=(22, 28), facecolor='white')
    fig.suptitle('UUV Simulation — Defense Effectiveness Analysis',
                 fontsize=20, fontweight='bold', y=0.995)

    gs = GridSpec(4, 3, figure=fig, hspace=0.50, wspace=0.30)
    has_noise = summary_df['noise_level'].nunique() > 1
    intercepted_sk = seeker_df[seeker_df['intercepted']]
    noise_sorted = summary_df.sort_values('noise_level')
    n_iters = len(summary_df)

    # ──────────────────────────────────────────────────────────────
    # ROW 1: Core Defense Outcome Metrics
    # ──────────────────────────────────────────────────────────────

    # Panel 1 — Seeker Interception Rate per Iteration
    ax = fig.add_subplot(gs[0, 0])
    bars = ax.bar(summary_df['iteration'], summary_df['intercept_rate'],
                  color='#2ecc71', edgecolor='none', width=1.0)
    # Color failed iterations red
    for i, rate in enumerate(summary_df['intercept_rate']):
        if rate < 1.0:
            bars[i].set_color('#e74c3c')
    mean_ir = summary_df['intercept_rate'].mean()
    ax.axhline(mean_ir, color='black', ls='--', lw=1.2,
               label=f'Mean: {mean_ir:.1%}')
    ax.set(xlabel='Iteration', ylabel='Intercept Rate', ylim=(0, 1.05))
    ax.set_title('Seeker Interception Rate per Iteration', fontsize=11, fontweight='bold')
    _add_description(ax, 'What fraction of seekers were intercepted each run.\n'
                         'Green = all stopped. Red = some got through.')
    ax.legend(fontsize=8, loc='lower left')

    # Panel 2 — Target Destruction Rate per Iteration
    ax = fig.add_subplot(gs[0, 1])
    bars = ax.bar(summary_df['iteration'], summary_df['target_destruction_rate'],
                  color='#e74c3c', edgecolor='none', width=1.0)
    for i, rate in enumerate(summary_df['target_destruction_rate']):
        if rate == 0:
            bars[i].set_color('#2ecc71')
    mean_td = summary_df['target_destruction_rate'].mean()
    ax.axhline(mean_td, color='black', ls='--', lw=1.2,
               label=f'Mean: {mean_td:.1%}')
    ax.set(xlabel='Iteration', ylabel='Destruction Rate', ylim=(0, max(1.05, summary_df['target_destruction_rate'].max() + 0.05)))
    ax.set_title('Target Destruction Rate per Iteration', fontsize=11, fontweight='bold')
    _add_description(ax, 'What fraction of targets were destroyed each run.\n'
                         'Green = target survived. Red = target destroyed.')
    ax.legend(fontsize=8, loc='upper left')

    # Panel 3 — Overall Outcome Pie Chart
    ax = fig.add_subplot(gs[0, 2])
    n_intercepted = seeker_df['intercepted'].sum()
    n_reached = seeker_df['reached_target'].sum()
    n_other = len(seeker_df) - n_intercepted - n_reached
    sizes = [n_intercepted, n_reached]
    labels_pie = [f'Intercepted\n({n_intercepted}/{len(seeker_df)})',
                  f'Reached Target\n({n_reached}/{len(seeker_df)})']
    colors_pie = ['#2ecc71', '#e74c3c']
    if n_other > 0:
        sizes.append(n_other)
        labels_pie.append(f'In Transit\n({n_other}/{len(seeker_df)})')
        colors_pie.append('#f39c12')
    wedges, texts, autotexts = ax.pie(sizes, labels=labels_pie, colors=colors_pie,
                                       autopct='%1.1f%%', startangle=90,
                                       textprops={'fontsize': 9})
    for at in autotexts:
        at.set_fontweight('bold')
        at.set_fontsize(11)
    ax.set_title('Overall Seeker Outcomes', fontsize=11, fontweight='bold')
    _add_description(ax, 'Across ALL iterations: how many seekers were\n'
                         'intercepted vs reached their target.')

    # ──────────────────────────────────────────────────────────────
    # ROW 2: Noise Impact on Defense
    # ──────────────────────────────────────────────────────────────

    if has_noise:
        # Panel 4 — Intercept Rate vs Noise Level (line plot)
        ax = fig.add_subplot(gs[1, 0])
        ax.scatter(noise_sorted['noise_level'], noise_sorted['intercept_rate'],
                   alpha=0.3, s=15, color='#2ecc71', zorder=1)
        smoothed = _smooth(noise_sorted.set_index('noise_level')['intercept_rate'])
        ax.plot(smoothed.index, smoothed.values, color='#27ae60', lw=2.5, zorder=2,
                label='Trend (rolling avg)')
        ax.set(xlabel='Noise Level', ylabel='Intercept Rate', ylim=(-0.02, 1.05))
        ax.set_title('Interception Rate vs Noise Level', fontsize=11, fontweight='bold')
        _add_description(ax, 'Does increasing environmental noise degrade the\n'
                             'defense\'s ability to intercept seekers?')
        ax.legend(fontsize=8)
        ax.grid(True, alpha=0.3)

        # Panel 5 — Target Destruction Rate vs Noise Level (line plot)
        ax = fig.add_subplot(gs[1, 1])
        ax.scatter(noise_sorted['noise_level'], noise_sorted['target_destruction_rate'],
                   alpha=0.3, s=15, color='#e74c3c', zorder=1)
        smoothed = _smooth(noise_sorted.set_index('noise_level')['target_destruction_rate'])
        ax.plot(smoothed.index, smoothed.values, color='#c0392b', lw=2.5, zorder=2,
                label='Trend (rolling avg)')
        ax.set(xlabel='Noise Level', ylabel='Destruction Rate',
               ylim=(-0.02, max(1.05, noise_sorted['target_destruction_rate'].max() + 0.05)))
        ax.set_title('Target Destruction vs Noise Level', fontsize=11, fontweight='bold')
        _add_description(ax, 'Does noise help attackers destroy targets?\n'
                             'Higher = worse for the defender.')
        ax.legend(fontsize=8)
        ax.grid(True, alpha=0.3)

        # Panel 6 — Steps to Intercept vs Noise Level
        ax = fig.add_subplot(gs[1, 2])
        valid = noise_sorted.dropna(subset=['avg_intercept_step'])
        ax.scatter(valid['noise_level'], valid['avg_intercept_step'],
                   alpha=0.3, s=15, color='#3498db', zorder=1)
        if len(valid) > 0:
            smoothed = _smooth(valid.set_index('noise_level')['avg_intercept_step'])
            ax.plot(smoothed.index, smoothed.values, color='#2980b9', lw=2.5, zorder=2,
                    label='Trend (rolling avg)')
        ax.set(xlabel='Noise Level', ylabel='Avg Steps to Intercept')
        ax.set_title('Time to Intercept vs Noise Level', fontsize=11, fontweight='bold')
        _add_description(ax, 'How many simulation steps before seekers are caught.\n'
                             'Higher = seekers penetrate deeper before interception.')
        ax.legend(fontsize=8)
        ax.grid(True, alpha=0.3)

    else:
        # If single noise level, show histograms instead
        ax = fig.add_subplot(gs[1, 0])
        ax.hist(summary_df['intercept_rate'], bins=10, color='#2ecc71', edgecolor='black', lw=0.5)
        ax.set(xlabel='Intercept Rate', ylabel='Count')
        ax.set_title('Interception Rate Distribution', fontsize=11, fontweight='bold')

        ax = fig.add_subplot(gs[1, 1])
        ax.hist(summary_df['target_destruction_rate'], bins=10, color='#e74c3c', edgecolor='black', lw=0.5)
        ax.set(xlabel='Destruction Rate', ylabel='Count')
        ax.set_title('Target Destruction Distribution', fontsize=11, fontweight='bold')

        ax = fig.add_subplot(gs[1, 2])
        valid = intercepted_sk['intercepted_at_step']
        ax.hist(valid, bins=15, color='#3498db', edgecolor='black', lw=0.5)
        ax.set(xlabel='Step at Interception', ylabel='Count')
        ax.set_title('Steps-to-Intercept Distribution', fontsize=11, fontweight='bold')

    # ──────────────────────────────────────────────────────────────
    # ROW 3: Detector Analysis
    # ──────────────────────────────────────────────────────────────

    # Panel 7 — Detector Utilization Bar Chart
    ax = fig.add_subplot(gs[2, 0])
    det_sorted = detector_df.sort_values('total_sightings', ascending=True)
    colors = ['#e74c3c' if v > 0 else '#d5d8dc' for v in det_sorted['total_sightings']]
    ax.barh(det_sorted['detector_id'].astype(str), det_sorted['total_sightings'],
            color=colors, edgecolor='black', lw=0.3)
    ax.set(xlabel='Total Intercepts (all iterations)', ylabel='Detector ID')
    ax.set_title('Detector Intercept Count', fontsize=11, fontweight='bold')
    _add_description(ax, 'How many seekers each detector intercepted across all runs.\n'
                         'Gray = zero intercepts (candidate for repositioning).')

    # Panel 8 — Detector Spatial Map with Chokepoints
    ax = fig.add_subplot(gs[2, 1:3])
    # Plot chokepoint cells
    top_cp = chokepoint_df.head(40)
    sc = ax.scatter(top_cp['col'], top_cp['row'], c=top_cp['visit_fraction'],
                    s=top_cp['visit_count'] * (300 / max(top_cp['visit_count'].max(), 1)),
                    cmap='YlOrRd', edgecolors='black', lw=0.3, alpha=0.7, zorder=1)
    # Plot detectors
    active_det = detector_df[detector_df['total_sightings'] > 0]
    idle_det = detector_df[detector_df['total_sightings'] == 0]
    if len(idle_det) > 0:
        ax.scatter(idle_det['col'], idle_det['row'], marker='^', s=100,
                   c='#d5d8dc', edgecolors='black', lw=0.8, zorder=3, label='Idle detectors')
    if len(active_det) > 0:
        ax.scatter(active_det['col'], active_det['row'], marker='^', s=150,
                   c='#2ecc71', edgecolors='black', lw=0.8, zorder=4, label='Active detectors')
    # Plot target(s) — get from first iteration
    ax.set(xlabel='Column (grid)', ylabel='Row (grid)')
    ax.set_title('Detector Positions & Seeker Chokepoints', fontsize=11, fontweight='bold')
    _add_description(ax, 'Circles = most-visited cells by seekers (darker/larger = more traffic).\n'
                         'Triangles = detector positions (green = active, gray = idle).')
    ax.invert_yaxis()
    ax.legend(fontsize=8, loc='upper left')
    plt.colorbar(sc, ax=ax, label='Seeker Visit Fraction', shrink=0.8)

    # ──────────────────────────────────────────────────────────────
    # ROW 4: Deeper Dive
    # ──────────────────────────────────────────────────────────────

    # Panel 9 — Steps-to-intercept histogram (all iterations)
    ax = fig.add_subplot(gs[3, 0])
    if len(intercepted_sk) > 0:
        ax.hist(intercepted_sk['intercepted_at_step'],
                bins=min(30, max(5, len(intercepted_sk)//5)),
                color='#3498db', edgecolor='black', lw=0.5, alpha=0.8)
        mean_step = intercepted_sk['intercepted_at_step'].mean()
        ax.axvline(mean_step, color='red', ls='--', lw=1.5,
                   label=f'Mean: {mean_step:.1f} steps')
        ax.legend(fontsize=8)
    ax.set(xlabel='Simulation Step at Interception', ylabel='Number of Seekers')
    ax.set_title('When Are Seekers Intercepted?', fontsize=11, fontweight='bold')
    _add_description(ax, 'Distribution of how deep seekers penetrate before being caught.\n'
                         'Earlier interception = better defense.')

    # Panel 10 — Intercept rate by detector (who's doing the work)
    ax = fig.add_subplot(gs[3, 1])
    active_only = detector_df[detector_df['total_sightings'] > 0].copy()
    if len(active_only) > 0:
        active_only = active_only.sort_values('total_sightings', ascending=False)
        total_all = detector_df['total_sightings'].sum()
        active_only['share'] = active_only['total_sightings'] / max(total_all, 1)
        wedges, texts, autotexts = ax.pie(
            active_only['total_sightings'],
            labels=[f'Det {int(d)}' for d in active_only['detector_id']],
            autopct='%1.1f%%', startangle=90,
            colors=plt.cm.Set2(np.linspace(0, 1, len(active_only))),
            textprops={'fontsize': 9})
        for at in autotexts:
            at.set_fontweight('bold')
    ax.set_title('Intercept Share by Detector', fontsize=11, fontweight='bold')
    _add_description(ax, 'Among active detectors, what share of intercepts\n'
                         'does each one handle?')

    # Panel 11 — Defense success rate summary text box
    ax = fig.add_subplot(gs[3, 2])
    ax.axis('off')
    n_total = len(summary_df)
    n_perfect = (summary_df['intercept_rate'] == 1.0).sum()
    n_failed = (summary_df['target_destruction_rate'] > 0).sum()
    n_active_det = (detector_df['total_sightings'] > 0).sum()
    n_total_det = len(detector_df)
    avg_ir = summary_df['intercept_rate'].mean()
    avg_td = summary_df['target_destruction_rate'].mean()

    summary_text = (
        f"━━━ DEFENSE SUMMARY ━━━\n\n"
        f"Iterations Run:          {n_total}\n"
        f"Total Seekers Launched:  {len(seeker_df)}\n"
        f"Seekers Intercepted:     {int(seeker_df['intercepted'].sum())}\n"
        f"Seekers Reached Target:  {int(seeker_df['reached_target'].sum())}\n\n"
        f"Avg Interception Rate:   {avg_ir:.1%}\n"
        f"Avg Target Destruction:  {avg_td:.1%}\n\n"
        f"Perfect Defense Runs:    {n_perfect}/{n_total} ({n_perfect/n_total:.0%})\n"
        f"Runs With Target Hit:    {n_failed}/{n_total} ({n_failed/n_total:.0%})\n\n"
        f"Active Detectors:        {n_active_det}/{n_total_det}\n"
        f"Idle Detectors:          {n_total_det - n_active_det}/{n_total_det}\n"
    )
    ax.text(0.05, 0.95, summary_text, transform=ax.transAxes,
            fontsize=12, fontfamily='monospace', verticalalignment='top',
            bbox=dict(boxstyle='round,pad=0.8', facecolor='#f8f9fa', edgecolor='#333'))
    ax.set_title('Quick Reference', fontsize=11, fontweight='bold')

    out = os.path.join(output_dir, 'analysis_dashboard.png')
    plt.savefig(out, dpi=150, bbox_inches='tight')
    plt.close()
    print(f"  Dashboard → {out}")

# ═══════════════════════════════════════════════════════════════════
# MARKDOWN REPORT
# ═══════════════════════════════════════════════════════════════════

def write_report(stats, summary_df, detector_df, noise_df, chokepoint_df, seeker_df, output_dir):
    L = []
    noise_levels = [int(n) for n in sorted(summary_df['noise_level'].unique())]

    L.append("# UUV Simulation — Defense Effectiveness Report\n")
    L.append(f"**Iterations analyzed:** {stats['n_iterations']}  ")
    L.append(f"**Total seekers launched:** {stats['n_seekers_total']}  ")
    L.append(f"**Noise levels tested:** {min(noise_levels)} – {max(noise_levels)} "
             f"({len(noise_levels)} levels)\n")

    # ── Section 1: Key Results ──
    L.append("\n## 1. Key Defense Results\n")
    L.append("| Metric | Value |")
    L.append("|---|---|")
    L.append(f"| **Seeker Interception Rate** | **{stats['overall_intercept_rate']:.1%}** |")
    L.append(f"| Seekers Intercepted / Total | {stats['n_seekers_intercepted']} / {stats['n_seekers_total']} |")
    L.append(f"| **Target Destruction Rate** | **{stats['overall_target_destruction_rate']:.1%}** |")
    L.append(f"| **Target Survival Rate** | **{stats['overall_target_survival_rate']:.1%}** |")
    L.append(f"| Seekers That Reached Target | {stats['n_seekers_reached']} / {stats['n_seekers_total']} |")
    n_perfect = int((summary_df['intercept_rate'] == 1.0).sum())
    L.append(f"| Perfect Defense Iterations | {n_perfect} / {stats['n_iterations']} "
             f"({n_perfect/stats['n_iterations']:.0%}) |")
    n_failed = int((summary_df['target_destruction_rate'] > 0).sum())
    L.append(f"| Iterations With Target Destroyed | {n_failed} / {stats['n_iterations']} "
             f"({n_failed/stats['n_iterations']:.0%}) |")
    if not np.isnan(stats['mean_intercept_step']):
        L.append(f"| Avg Steps to Intercept | {stats['mean_intercept_step']:.1f} ± {stats['std_intercept_step']:.1f} |")
        L.append(f"| Intercept Step Range | [{stats['min_intercept_step']:.0f}, {stats['max_intercept_step']:.0f}] |")

    # ── Section 2: Per-Iteration ──
    L.append("\n\n## 2. Per-Iteration Breakdown\n")
    cols = ['iteration', 'noise_level', 'total_seekers', 'seekers_intercepted',
            'seekers_reached_target', 'intercept_rate', 'target_destruction_rate',
            'avg_intercept_step']
    L.append(summary_df[cols].round(3).to_markdown(index=False))

    # ── Section 3: Detector Utilization ──
    L.append("\n\n## 3. Detector Utilization\n")
    L.append(detector_df.to_markdown(index=False))
    active = detector_df[detector_df['total_sightings'] > 0]
    idle = detector_df[detector_df['total_sightings'] == 0]
    L.append(f"\n- **Active detectors:** {len(active)} / {len(detector_df)} "
             f"({len(active)/max(len(detector_df),1):.0%})")
    L.append(f"- **Idle detectors (reposition candidates):** "
             f"{[int(x) for x in idle['detector_id'].values]}")
    if len(active) > 0:
        top_det = active.sort_values('total_sightings', ascending=False).iloc[0]
        L.append(f"- **Most effective detector:** #{int(top_det['detector_id'])} "
                 f"at row={int(top_det['row'])}, col={int(top_det['col'])} "
                 f"with {int(top_det['total_sightings'])} total intercepts")

    # ── Section 4: Noise Impact ──
    if noise_df is not None:
        L.append("\n\n## 4. Noise Impact on Defense\n")
        L.append(noise_df.round(4).to_markdown(index=False))

        n0 = summary_df[summary_df['noise_level'] == min(noise_levels)]
        n_max = summary_df[summary_df['noise_level'] == max(noise_levels)]
        if len(n0) > 0 and len(n_max) > 0:
            L.append(f"\n**Comparing noise {min(noise_levels)} → {max(noise_levels)}:**\n")
            ir0 = n0['intercept_rate'].mean()
            irn = n_max['intercept_rate'].mean()
            L.append(f"- Intercept rate: {ir0:.1%} → {irn:.1%}")
            td0 = n0['target_destruction_rate'].mean()
            tdn = n_max['target_destruction_rate'].mean()
            L.append(f"- Target destruction rate: {td0:.1%} → {tdn:.1%}")
            if not np.isnan(n0['avg_intercept_step'].mean()) and not np.isnan(n_max['avg_intercept_step'].mean()):
                L.append(f"- Avg intercept step: {n0['avg_intercept_step'].mean():.1f} → {n_max['avg_intercept_step'].mean():.1f}")

    # ── Section 5: Chokepoints ──
    L.append("\n\n## 5. Seeker Path Chokepoints\n")
    L.append("Top 15 most-visited grid cells across all seeker paths:\n")
    L.append(chokepoint_df.head(15).to_markdown(index=False))
    top = chokepoint_df.head(20)
    if len(top) > 0:
        L.append(f"\n- **Dominant corridor row:** {int(top['row'].mode().values[0])}")
        L.append(f"- **Column range:** {int(top['col'].min())} – {int(top['col'].max())}")

    L.append("\n\n---\n*Generated by analyze_iterations.py*\n")

    out = os.path.join(output_dir, 'analysis_report.md')
    with open(out, 'w') as f:
        f.write('\n'.join(L))
    print(f"  Report → {out}")

# ═══════════════════════════════════════════════════════════════════
# MAIN
# ═══════════════════════════════════════════════════════════════════

def main():
    input_dir = sys.argv[1] if len(sys.argv) > 1 else '.'
    output_dir = sys.argv[2] if len(sys.argv) > 2 else '.'
    os.makedirs(output_dir, exist_ok=True)

    iterations = load_iterations(input_dir)

    print("Building tables...")
    summary_df = build_summary_table(iterations)
    seeker_df = build_seeker_table(iterations)
    detector_df = build_detector_table(iterations)
    chokepoint_df = chokepoint_analysis(iterations)
    noise_df = noise_impact_table(summary_df)
    stats = compute_aggregate_stats(summary_df, seeker_df)

    print("Generating outputs...")
    generate_dashboard(summary_df, seeker_df, detector_df, chokepoint_df, output_dir)
    write_report(stats, summary_df, detector_df, noise_df, chokepoint_df, seeker_df, output_dir)

    summary_df.to_csv(os.path.join(output_dir, 'iteration_summaries.csv'), index=False)
    seeker_df.to_csv(os.path.join(output_dir, 'seeker_details.csv'), index=False)
    detector_df.to_csv(os.path.join(output_dir, 'detector_utilization.csv'), index=False)
    chokepoint_df.to_csv(os.path.join(output_dir, 'chokepoints.csv'), index=False)
    if noise_df is not None:
        noise_df.to_csv(os.path.join(output_dir, 'noise_impact.csv'), index=False)
    print("Done — all files saved.")

if __name__ == '__main__':
    main()