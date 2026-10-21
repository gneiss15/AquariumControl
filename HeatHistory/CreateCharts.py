#!/usr/bin/env python3
# CreateCharts.py, Version: 1.05

from __future__ import annotations

import argparse
import csv
import datetime as dt
import html
import json
import math
import sys
from dataclasses import dataclass
from pathlib import Path
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlsplit


CSV_HEADER = ("Time", "Temp", "Floor", "Main")


class ChartDataError(Exception):
  pass


@dataclass(frozen=True)
class Sample:
  time: dt.datetime
  temp: float
  floor: int
  main: int


def parse_timestamp(value: str, path: Path, line_number: int) -> dt.datetime:
  try:
    parsed = dt.datetime.fromisoformat(value.replace("Z", "+00:00"))
  except ValueError as error:
    raise ChartDataError(f"{path}:{line_number}: invalid ISO timestamp {value!r}") from error
  if parsed.tzinfo is None:
    raise ChartDataError(f"{path}:{line_number}: timestamp must include a timezone")
  return parsed.astimezone(dt.timezone.utc)


def read_csv(path: Path) -> list[Sample]:
  try:
    source = path.open("r", encoding="utf-8-sig", newline="")
  except OSError as error:
    raise ChartDataError(f"cannot read {path}: {error}") from error

  with source:
    reader = csv.DictReader(source, delimiter=";")
    if tuple(reader.fieldnames or ()) != CSV_HEADER:
      raise ChartDataError(f"{path}: expected header {';'.join(CSV_HEADER)}")
    samples: list[Sample] = []
    previous_time: dt.datetime | None = None
    for line_number, row in enumerate(reader, 2):
      try:
        timestamp = parse_timestamp(row["Time"] or "", path, line_number)
        temp = float(row["Temp"] or "")
        floor = int(row["Floor"] or "")
        main = int(row["Main"] or "")
      except (TypeError, ValueError) as error:
        raise ChartDataError(f"{path}:{line_number}: invalid numeric value") from error
      if not math.isfinite(temp):
        raise ChartDataError(f"{path}:{line_number}: temperature must be finite")
      if floor not in (0, 1) or main not in (0, 1):
        raise ChartDataError(f"{path}:{line_number}: Floor and Main must be 0 or 1")
      if previous_time is not None and timestamp < previous_time:
        raise ChartDataError(f"{path}:{line_number}: timestamps are not chronological")
      samples.append(Sample(timestamp, temp, floor, main))
      previous_time = timestamp
  if not samples:
    raise ChartDataError(f"{path}: no samples found")
  return samples


def chart_data(samples: list[Sample], profile_id: str) -> str:
  payload = {
    "profile": profile_id,
    "time": [sample.time.strftime("%Y-%m-%dT%H:%M:%S.%fZ") for sample in samples],
    "temp": [sample.temp for sample in samples],
    "floor": [sample.floor for sample in samples],
    "main": [sample.main for sample in samples],
  }
  serialized = json.dumps(payload, ensure_ascii=False, separators=(",", ":"))
  return (serialized.replace("&", "\\u0026").replace("<", "\\u003c")
          .replace(">", "\\u003e").replace("\u2028", "\\u2028")
          .replace("\u2029", "\\u2029"))


def render_report(data_dir: Path) -> tuple[str, int]:
  csv_files = sorted(data_dir.rglob("*.csv"))
  if not csv_files:
    raise ChartDataError(f"no CSV logs found under {data_dir}")

  cards: list[str] = []
  chart_sets: list[str] = []
  for csv_path in csv_files:
    samples = read_csv(csv_path)
    profile_id = csv_path.parent.name
    relative_path = html.escape(csv_path.relative_to(data_dir).as_posix())
    first = samples[0].time.strftime("%Y-%m-%d %H:%M:%S UTC")
    last = samples[-1].time.strftime("%Y-%m-%d %H:%M:%S UTC")
    chart_index = len(chart_sets)
    cards.append(
      f'<article><h2>{html.escape(profile_id)}</h2>'
      f'<p class="meta"><code>{relative_path}</code> · {len(samples)} Messpunkte · {first} – {last}</p>'
      f'<h3>Temperatur und Heiz-Ausgänge</h3><div class="chart" id="series-{chart_index}" aria-label="Temperatur- und Schaltzustandsdiagramm {html.escape(profile_id)}"></div></article>'
    )
    chart_sets.append(chart_data(samples, profile_id))

  document = f'''<!doctype html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>HeatHistory-Diagramme</title>
<script src="https://cdn.plot.ly/plotly-4.1.1.min.js" charset="utf-8"></script>
<style>
body {{ margin: 0; padding: 1rem; color: #17252a; background: #eef3f5; font: 16px system-ui, sans-serif; }}
main {{ max-width: 1100px; margin: 0 auto; }}
article {{ margin: 1rem 0; padding: 1rem; background: white; border: 1px solid #d5e0e3; border-radius: .6rem; }}
h1, h2, h3 {{ margin: .3rem 0 .7rem; }} h3 {{ margin-top: 1rem; font-size: 1rem; }}
.meta {{ color: #617177; font-size: .85rem; }} .chart {{ min-height: 620px; width: 100%; }}
.chart-error {{ padding: .8rem; color: #8b1e1e; background: #fff0f0; border-radius: .4rem; }}
@media print {{ body {{ background: white; }} article {{ break-inside: avoid; box-shadow: none; }} }}
</style>
</head>
<body><main><h1>HeatHistory-Diagramme</h1>
<p>CSV-Zeitstempel werden als UTC-Datum interpretiert. Temperatur, Floor und Main stehen gemeinsam auf einer Zeitachse; die beiden binären Ausgänge haben jeweils eine eigene kompakte Aus-/Ein-Achse. Zoome per Mausrad oder Auswahlrahmen; die Schnellwahl steuert alle drei Bereiche. Doppelklick setzt den Zeitbereich zurück.</p>
<p class="chart-error" id="chart-error" hidden>Die interaktive Diagrammbibliothek konnte nicht geladen werden. Zum Anzeigen ist eine Internetverbindung erforderlich.</p>
{''.join(cards)}
</main>
<script>
const chartSets = [{','.join(chart_sets)}];
const rangeButtons = [
  {{step: 'hour', stepmode: 'backward', count: 1, label: '1 Std.'}},
  {{step: 'hour', stepmode: 'backward', count: 6, label: '6 Std.'}},
  {{step: 'day', stepmode: 'backward', count: 1, label: '1 Tag'}},
  {{step: 'day', stepmode: 'backward', count: 7, label: '7 Tage'}},
  {{step: 'all', label: 'Gesamt'}}
];
const plotConfig = {{responsive: true, scrollZoom: true, displaylogo: false}};
const baseLayout = {{
  autosize: true,
  height: 640,
  margin: {{l: 78, r: 24, t: 78, b: 56}},
  paper_bgcolor: '#fff',
  plot_bgcolor: '#fff',
  hovermode: 'x unified',
  dragmode: 'zoom',
  font: {{family: 'system-ui, sans-serif', color: '#39494f'}},
  xaxis: {{
    type: 'date',
    domain: [0.12, 1],
    anchor: 'y',
    matches: 'x3',
    showticklabels: false,
    showgrid: true,
    gridcolor: '#e3eaec'
  }},
  xaxis2: {{
    type: 'date',
    domain: [0.12, 1],
    anchor: 'y2',
    matches: 'x3',
    showticklabels: false,
    showgrid: true,
    gridcolor: '#e3eaec'
  }},
  xaxis3: {{
    type: 'date',
    domain: [0.12, 1],
    anchor: 'y3',
    title: 'Zeit (UTC)',
    tickformat: '%d.%m.%Y<br>%H:%M',
    showgrid: true,
    gridcolor: '#e3eaec',
    rangeselector: {{buttons: rangeButtons, x: 0, y: 1.16, xanchor: 'left'}}
  }},
  yaxis: {{
    domain: [0.39, 1],
    anchor: 'x',
    title: 'Temperatur (°C)',
    automargin: true,
    gridcolor: '#e3eaec'
  }},
  yaxis2: {{
    domain: [0.18, 0.30],
    anchor: 'x2',
    title: 'Floor',
    range: [-0.1, 1.1],
    tickmode: 'array',
    tickvals: [0, 1],
    ticktext: ['Aus', 'Ein'],
    fixedrange: true,
    showgrid: false,
    zeroline: false,
    automargin: true
  }},
  yaxis3: {{
    domain: [0, 0.12],
    anchor: 'x3',
    title: 'Main',
    range: [-0.1, 1.1],
    tickmode: 'array',
    tickvals: [0, 1],
    ticktext: ['Aus', 'Ein'],
    fixedrange: true,
    showgrid: false,
    zeroline: false,
    automargin: true
  }}
}};

async function drawCharts() {{
  if (!window.Plotly) {{
    document.getElementById('chart-error').hidden = false;
    return;
  }}
  for (let index = 0; index < chartSets.length; index += 1) {{
    const source = chartSets[index];
    const series = document.getElementById(`series-${{index}}`);
    const commonTrace = {{x: source.time, connectgaps: false}};
    await Plotly.newPlot(series, [
      {{
        ...commonTrace,
        y: source.temp,
        yaxis: 'y',
        xaxis: 'x',
        type: 'scatter',
        mode: source.time.length < 500 ? 'lines+markers' : 'lines',
        name: 'Temperatur',
        line: {{color: '#087f8c', width: 2.5}},
        marker: {{size: 5}},
        hovertemplate: '%{{x|%d.%m.%Y %H:%M:%S}} UTC<br>%{{y:.2f}} °C<extra></extra>'
      }},
      {{
        ...commonTrace,
        y: source.floor,
        yaxis: 'y2',
        xaxis: 'x2',
        text: source.floor.map(value => value ? 'Ein' : 'Aus'),
        type: 'scatter',
        mode: 'lines',
        name: 'Floor',
        line: {{color: '#d17a00', width: 3, shape: 'hv'}},
        hovertemplate: '%{{x|%d.%m.%Y %H:%M:%S}} UTC<br>Floor: %{{text}}<extra></extra>'
      }},
      {{
        ...commonTrace,
        y: source.main,
        yaxis: 'y3',
        xaxis: 'x3',
        text: source.main.map(value => value ? 'Ein' : 'Aus'),
        type: 'scatter',
        mode: 'lines',
        name: 'Main',
        line: {{color: '#7057b8', width: 3, shape: 'hv'}},
        hovertemplate: '%{{x|%d.%m.%Y %H:%M:%S}} UTC<br>Main: %{{text}}<extra></extra>'
      }}
    ], baseLayout, plotConfig);
  }}
}}

drawCharts();
</script>
</body></html>
'''
  return document, len(csv_files)


def create_report(data_dir: Path, output: Path) -> int:
  document, count = render_report(data_dir)
  output.parent.mkdir(parents=True, exist_ok=True)
  output.write_text(document, encoding="utf-8", newline="\n")
  return count


def chart_server(data_dir: Path, host: str, port: int) -> ThreadingHTTPServer:
  class ChartHandler(BaseHTTPRequestHandler):
    def do_GET(self) -> None:
      self.respond(False)

    def do_HEAD(self) -> None:
      self.respond(True)

    def respond(self, head_only: bool) -> None:
      # Serve generated reports only; never expose configuration or raw files.
      if urlsplit(self.path).path not in ("/", "/HeatHistoryCharts.html"):
        self.send_error(404)
        return
      try:
        document, _ = render_report(data_dir)
        status = 200
      except (ChartDataError, OSError) as error:
        document = ('<!doctype html><html lang="de"><meta charset="utf-8">'
                    '<title>HeatHistory</title><h1>Diagramme derzeit nicht verfügbar</h1>'
                    f'<p>{html.escape(str(error))}</p><p>Seite später neu laden.</p></html>')
        status = 503
      body = document.encode("utf-8")
      self.send_response(status)
      self.send_header("Content-Type", "text/html; charset=utf-8")
      self.send_header("Content-Length", str(len(body)))
      self.send_header("Cache-Control", "no-store")
      self.end_headers()
      if not head_only:
        self.wfile.write(body)

  return ThreadingHTTPServer((host, port), ChartHandler)


def main() -> int:
  project_dir = Path(__file__).resolve().parent
  parser = argparse.ArgumentParser(description="Create interactive browser charts from HeatHistory CSV logs")
  parser.add_argument("--data-dir", type=Path, default=Path("data"), help="directory containing profile CSV logs")
  parser.add_argument("--output", type=Path, default=Path("data/HeatHistoryCharts.html"), help="HTML report destination")
  parser.add_argument("--serve", action="store_true", help="generate and serve current charts on each browser request")
  parser.add_argument("--host", default="127.0.0.1", help="server bind address (default: local PC only)")
  parser.add_argument("--port", type=int, default=8091, help="HTTP server port (default: 8091)")
  args = parser.parse_args()
  if not 1 <= args.port <= 65535:
    parser.error("--port must be between 1 and 65535")
  data_dir = args.data_dir if args.data_dir.is_absolute() else project_dir / args.data_dir
  output = args.output if args.output.is_absolute() else project_dir / args.output
  try:
    if args.serve:
      with chart_server(data_dir.resolve(), args.host, args.port) as server:
        print(f"HeatHistory charts: http://{args.host}:{server.server_port}/ (Ctrl+C to stop)", flush=True)
        try:
          server.serve_forever()
        except KeyboardInterrupt:
          pass
      return 0
    count = create_report(data_dir.resolve(), output.resolve())
  except (ChartDataError, OSError) as error:
    print(f"ERROR: {error}", file=sys.stderr)
    return 2
  print(f"Created {count} chart sections: {output.resolve()}")
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
