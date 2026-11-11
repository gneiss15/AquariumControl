#!/usr/bin/env python3
# CreateCharts.py, Version: 1.17

from __future__ import annotations

import argparse
import csv
import datetime as dt
import html
import json
import math
import sys
from dataclasses import dataclass
from EnergyHistory import HEADER as ENERGY_HEADER
from PiHistory import HEADER as PI_HEADER
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


def energy_chart_data(path: Path, carry: dict | None = None) -> dict:
  offsets=dict(carry or {'mainwh':0.,'floorwh':0.})
  totals=dict(offsets)
  result={key:[] for key in ('time','temp','floor','main','room','power','reported','floorpower','mainwh','floorwh')}
  with path.open(encoding='utf-8',newline='') as stream:
    reader=csv.DictReader(stream,delimiter=';')
    if tuple(reader.fieldnames or ())!=ENERGY_HEADER: raise ChartDataError(f"{path}: invalid energy header")
    for row in reader:
      if any(value is None for value in row.values()): continue
      try:
        if result['time']:
          gap=(dt.datetime.fromisoformat(row['Time'].replace('Z','+00:00'))-dt.datetime.fromisoformat(result['time'][-1].replace('Z','+00:00'))).total_seconds()
          if gap>15:
            result['time'].append(row['Time'])
            for key in result:
              if key!='time':result[key].append(None)
        def number(key):
          value=float(row[key]) if row[key] else None
          return value if value is None or math.isfinite(value) else None
        result['time'].append(row['Time'])
        result['temp'].append(number('TempC') if row['TempValid']=='1' else None)
        result['floor'].append(int(row['FloorState']) if row['FloorState'] in ('0','1') else None)
        result['main'].append(int(row['MainState']) if row['MainState'] in ('0','1') else None)
        result['room'].append(number('RoomTempC') if row['RoomTempValid']=='1' else None)
        result['power'].append(number('MainCorrectedW') if row['MainPowerValid']=='1' else None)
        result['reported'].append(number('MainReportedW') if row['MainPowerValid']=='1' else None)
        result['floorpower'].append(number('FloorNominalW')*int(row['FloorState']) if row['FloorState'] in ('0','1') and row['FloorNominalW'] else None)
        main_known=row['MainIntervalQuality'] in ('','valid','held','interpolated') and (row['MainPowerValid']=='1' or row['MainIntervalQuality'] in ('held','interpolated'))
        floor_known=row['FloorIntervalQuality'] in ('','valid','held','interpolated') and (row['FloorState'] in ('0','1') or row['FloorIntervalQuality'] in ('held','interpolated'))
        # Totals remain usable even when the latest interval cannot be plotted.
        for key,column,known in [('mainwh','MainIntegratedWh',main_known),('floorwh','FloorEstimatedWh',floor_known)]:
          value=number(column)
          if value is not None: totals[key]=offsets[key]+value
          result[key].append(offsets[key]+value if known and value is not None else None)
      except ValueError: raise ChartDataError(f"{path}: invalid energy value")
  if carry is not None: carry.update(totals)
  return result


def pi_chart_data(directory: Path) -> dict | None:
  result={key:[] for key in ('time','demand','actual','floor','main','p','i','ff','setpoint')}
  columns={'demand':'DemandW','actual':'ActualWatts','floor':'FloorDemandW','main':'MainDemandW',
           'p':'ProportionalW','i':'IntegralW','ff':'FeedForwardW'}
  for path in sorted(directory.glob('*.pi.csv')):
    # Each session records its applicable profile; never project today's target backwards.
    try:
      config=json.loads(path.with_suffix('.json').read_text()).get('Cfg') or {}
      target=config.get('Setpoint')
      if isinstance(target,bool) or not isinstance(target,(int,float)) or not math.isfinite(target): target=None
    except (OSError,ValueError,TypeError,AttributeError): target=None
    with path.open(newline='') as stream:
      reader=csv.DictReader(stream,delimiter=';')
      if tuple(reader.fieldnames or ())!=PI_HEADER:raise ChartDataError(f"{path}: invalid PI header")
      boundary=True
      for row in reader:
        if result['time']:
          gap=(dt.datetime.fromisoformat(row['Time'])-dt.datetime.fromisoformat(result['time'][-1])).total_seconds()
          if boundary or gap>15:
            result['time'].append(row['Time'])
            for key in result:
              if key!='time':result[key].append(result[key][-1] if key=='setpoint' and gap<=15 else None)
        boundary=False
        result['time'].append(row['Time'])
        result['setpoint'].append(target)
        for key,column in columns.items():
          try:value=float(row[column]) if row['Valid']=='1' and row[column] else None
          except (ValueError,TypeError) as error:raise ChartDataError(f"{path}: invalid PI value") from error
          result[key].append(value if value is None or math.isfinite(value) else None)
  return result if result['time'] else None


def render_report(data_dir: Path) -> tuple[str, int]:
  csv_files = sorted(path for path in data_dir.rglob("*.csv") if not path.name.endswith((".energy.csv", ".pi.csv")))
  if not csv_files:
    raise ChartDataError(f"no CSV logs found under {data_dir}")

  cards: list[str] = []
  chart_sets: list[str] = []
  energy_sets=[]
  pi_sets=[]
  profiles=sorted({path.parent.name for path in csv_files})
  for profile_id in profiles:
    logs=[path for path in csv_files if path.parent.name==profile_id]
    samples=[]
    for path in logs: samples.extend(read_csv(path))
    samples.sort(key=lambda sample:sample.time)
    values=None
    carry={'mainwh':0.,'floorwh':0.}
    for path in sorted((data_dir/profile_id/'Energy').glob('*.energy.csv')):
      session=energy_chart_data(path,carry)
      if not session['time']:continue
      if values is None: values=session
      else:
        # Keep session boundaries visible while carrying recorded energy forward.
        for key in values:
          values[key].append(session['time'][0] if key=='time' else None)
          values[key].extend(session[key])
    index=len(chart_sets)
    first=samples[0].time.strftime("%Y-%m-%d %H:%M:%S UTC")
    last=samples[-1].time.strftime("%Y-%m-%d %H:%M:%S UTC")
    paths=', '.join(html.escape(path.relative_to(data_dir).as_posix()) for path in logs)
    cards.append(f'<article><h2>{html.escape(profile_id)}</h2>'
                 f'<p class="meta"><code>{paths}</code> · {len(samples)} samples · {first} – {last}</p>'
                 '<p class="meta">All rows share one time axis. Main is the measured switch power. '
                 'Floor power uses the measured transformer '
                 'output (Aq1: 41.052 W; Aq2: 21.216 W) and the reported ON/OFF state; it is not continuously metered. '
                 'Energy is the time integral of these powers. Totals continue across sessions; gaps remain visible.</p>'
                 f'<div class="chart" id="series-{index}" aria-label="Aligned chart {html.escape(profile_id)}"></div></article>')
    pi_sets.append(json.dumps(pi_chart_data(data_dir/profile_id/"Pi"),separators=(",",":")))
    chart_sets.append(chart_data(samples,profile_id))
    energy_sets.append(json.dumps(values,separators=(',',':')).replace('<','\\u003c').replace('>','\\u003e'))

  document = f'''<!doctype html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>HeatHistory charts</title>
<script src="https://cdn.plot.ly/plotly-4.1.1.min.js" charset="utf-8"></script>
<style>
body {{ margin: 0; padding: 1rem; color: #17252a; background: #eef3f5; font: 16px system-ui, sans-serif; }}
main {{ width: 100%; min-width: 0; }}
.BrowserClock {{ display:block; text-align:left; font-size:.85rem; font-variant-numeric:tabular-nums; }}
article {{ margin: 1rem 0; padding: 1rem; background: white; border: 1px solid #d5e0e3; border-radius: .6rem; }}
h1, h2, h3 {{ margin: .3rem 0 .7rem; }} h3 {{ margin-top: 1rem; font-size: 1rem; }}
.meta {{ color: #617177; font-size: .85rem; }} .chart {{ min-height: 620px; width: 100%; }}
.chart-error {{ padding: .8rem; color: #8b1e1e; background: #fff0f0; border-radius: .4rem; }}
@media print {{ body {{ background: white; }} article {{ break-inside: avoid; box-shadow: none; }} }}
</style>
</head>
<body><main><time id="BrowserClock" class="BrowserClock" aria-live="off"></time><h1>HeatHistory charts</h1>
<p>Timestamps use UTC. Water and room temperature share one plot. Floor and Main show switch states. Power, energy, PI demand and PI components use aligned rows. Zoom by scrolling or dragging; double-click resets the time range.</p>
<details><summary>PI components explained</summary><p><strong>Proportional:</strong> Kp × (setpoint − water temperature); positive below the setpoint and negative above it. <strong>Integral:</strong> accumulates persistent temperature error to remove a lasting offset; it is limited and tracks actual delivered power. <strong>Heat-loss estimate:</strong> heat-loss coefficient (W/K) × (setpoint − room temperature), providing the estimated baseline heating power. Their sum is limited to the available heater power and becomes Total demand. Floor receives priority; Main supplies the remaining demand. Act is the current delivered power, not a demand or an energy total.</p></details>
<p class="chart-error" id="chart-error" hidden>The interactive chart library could not be loaded. An internet connection is required.</p>
{''.join(cards)}
</main>
<script>

// Use browser-local time; clock ticks never request device or server data.
function UpdateBrowserClock() {{
  const Clock = document.getElementById('BrowserClock');
  if (!Clock) return;
  const German = document.documentElement.lang === 'de';
  const Now = new Date();
  Clock.textContent = Now.toLocaleString(German ? 'de-DE' : 'en-GB', {{
    year: 'numeric', month: '2-digit', day: '2-digit',
    hour: '2-digit', minute: '2-digit', second: '2-digit', hourCycle: 'h23'
  }});
  Clock.dateTime = Now.toISOString();
  Clock.title = German ? 'Datum und Uhrzeit (Browserzeit)' : 'Date and time (browser clock)';
}}
function StartBrowserClock() {{
  UpdateBrowserClock();
  setInterval(UpdateBrowserClock, 1000);
}}
if (document.readyState === 'loading')
  document.addEventListener('DOMContentLoaded', StartBrowserClock, {{once: true}});
else StartBrowserClock();
// End browser clock.

const chartSets = [{','.join(chart_sets)}];
const energySets = [{','.join(energy_sets)}];
const piSets = [{','.join(pi_sets)}];
const rangeButtons = [
  {{step: 'hour', stepmode: 'backward', count: 1, label: '1 h'}},
  {{step: 'hour', stepmode: 'backward', count: 6, label: '6 h'}},
  {{step: 'day', stepmode: 'backward', count: 1, label: '1 day'}},
  {{step: 'day', stepmode: 'backward', count: 7, label: '7 days'}},
  {{step: 'all', label: 'All'}}
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
    domain: [0, 1],
    anchor: 'y',
    matches: 'x3',
    showticklabels: false,
    showgrid: true,
    gridcolor: '#e3eaec'
  }},
  xaxis2: {{
    type: 'date',
    domain: [0, 1],
    anchor: 'y2',
    matches: 'x3',
    showticklabels: false,
    showgrid: true,
    gridcolor: '#e3eaec'
  }},
  xaxis3: {{
    type: 'date',
    domain: [0, 1],
    anchor: 'y3',
    title: {{text:'Time (UTC)'}},
    tickformat: '%d.%m.%Y<br>%H:%M',
    showgrid: true,
    gridcolor: '#e3eaec',
    rangeselector: {{buttons: rangeButtons, x: 0, y: 1.16, xanchor: 'left'}}
  }},
  yaxis: {{
    domain: [0.39, 1],
    anchor: 'x',
    title: {{text:'Temperature (°C)'}},
    automargin: true,
    gridcolor: '#e3eaec'
  }},
  yaxis2: {{
    domain: [0.18, 0.30],
    anchor: 'x2',
    title: {{text:'Floor'}},
    range: [-0.1, 1.1],
    tickmode: 'array',
    tickvals: [0, 1],
    ticktext: ['OFF', 'ON'],
    fixedrange: true,
    showgrid: false,
    zeroline: false,
    automargin: true
  }},
  yaxis3: {{
    domain: [0, 0.12],
    anchor: 'x3',
    title: {{text:'Main'}},
    range: [-0.1, 1.1],
    tickmode: 'array',
    tickvals: [0, 1],
    ticktext: ['OFF', 'ON'],
    fixedrange: true,
    showgrid: false,
    zeroline: false,
    automargin: true
  }}
}};

const narrowCharts = window.matchMedia('(max-width: 900px)');
function legendLayout(baseHeight) {{
  return narrowCharts.matches
    ? {{legend: {{groupclick:'toggleitem',orientation:'h',x:0,y:-0.14,xanchor:'left',yanchor:'top'}},
        'margin.b':220,height:baseHeight+164}}
    : {{legend: {{groupclick:'toggleitem',orientation:'v',x:1.02,y:1,xanchor:'left',yanchor:'top'}},
        'margin.b':56,height:baseHeight}};
}}
async function drawCharts() {{
  if (!window.Plotly) {{
    document.getElementById('chart-error').hidden = false;
    return;
  }}
  for (let index = 0; index < chartSets.length; index += 1) {{
    const energy = energySets[index];
    const pi = piSets[index];
    const source = energy || chartSets[index];
    const layout = JSON.parse(JSON.stringify(baseLayout));
    if (energy) {{
      layout.height = pi ? 1120 : 820;
      const count=pi?7:5;
      const domains = pi ? [[.83,1],[.75,.79],[.67,.71],[.50,.62],[.33,.45],[.16,.28],[0,.12]]
        : [[.76,1],[.66,.71],[.56,.61],[.28,.50],[0,.22]];
      for (let row=1; row<=count; ++row) {{
        const suffix=row===1?'':String(row);
        const x='x'+suffix, y='y'+suffix;
        layout['xaxis'+suffix]={{type:'date',domain:[0,1],anchor:y,
          ...(row<count?{{matches:pi?'x7':'x5'}}:{{}}),showticklabels:row===count,
          showgrid:true,gridcolor:'#e3eaec',
          ...(row===count?{{title:{{text:'Time (UTC)'}},tickformat:'%d.%m.%Y<br>%H:%M',
            rangeselector:{{buttons:rangeButtons,x:0,y:1.10,xanchor:'left'}}}}:{{}})}};
        layout['yaxis'+suffix]={{domain:domains[row-1],anchor:x,automargin:true,
          title:{{text:['Temperature (°C)','Floor','Main','Power (W)','Energy (Wh)','PI demand (W)','PI components (W)'][row-1]}},
          gridcolor:'#e3eaec',...([4,6,7].includes(row)?{{rangemode:'tozero',zeroline:true,zerolinecolor:'#52636b',zerolinewidth:1.5}}:{{}}),...(row===2||row===3?{{range:[-0.1,1.1],tickvals:[0,1],
            ticktext:['OFF','ON'],fixedrange:true,showgrid:false,zeroline:false}}:{{}})}};
      }}
    }}
    const series = document.getElementById(`series-${{index}}`);
    const baseHeight=layout.height;
    const responsiveLegend=legendLayout(baseHeight);
    layout.legend=responsiveLegend.legend;
    layout.margin.b=responsiveLegend['margin.b'];
    layout.height=responsiveLegend.height;
    const commonTrace = {{x: source.time, connectgaps: false}};
    await Plotly.newPlot(series, [
      {{
        ...commonTrace,
        y: source.temp,
        connectgaps: true,
        yaxis: 'y',
        xaxis: 'x',
        type: 'scatter',
        mode: source.time.length < 500 ? 'lines+markers' : 'lines',
        name: 'Water',
        legendgroup: 'Temperature',
        legendgrouptitle: {{text: 'Temperature'}},
        line: {{color: '#087f8c', width: 2.5}},
        marker: {{size: 5}},
        hovertemplate: '%{{x|%d.%m.%Y %H:%M:%S}} UTC<br>Water: %{{y:.2f}} °C<extra></extra>'
      }},
      {{
        ...commonTrace,
        y: source.floor,
        yaxis: 'y2',
        xaxis: 'x2',
        text: source.floor.map(value => value === null ? 'Unknown' : value ? 'ON' : 'OFF'),
        type: 'scatter',
        mode: 'lines',
        name: 'Floor',
        legendgroup: 'Switches',
        legendgrouptitle: {{text: 'Switches'}},
        line: {{color: '#d17a00', width: 3, shape: 'hv'}},
        hovertemplate: '%{{x|%d.%m.%Y %H:%M:%S}} UTC<br>Floor: %{{text}}<extra></extra>'
      }},
      {{
        ...commonTrace,
        y: source.main,
        yaxis: 'y3',
        xaxis: 'x3',
        text: source.main.map(value => value === null ? 'Unknown' : value ? 'ON' : 'OFF'),
        type: 'scatter',
        mode: 'lines',
        name: 'Main',
        legendgroup: 'Switches',
        line: {{color: '#7057b8', width: 3, shape: 'hv'}},
        hovertemplate: '%{{x|%d.%m.%Y %H:%M:%S}} UTC<br>Main: %{{text}}<extra></extra>'
      }}
    ], layout, plotConfig);
    if (pi && pi.setpoint.some(value=>value!==null)) {{
      await Plotly.addTraces(series,[{{name:'Setpoint',x:pi.time,y:pi.setpoint,
        xaxis:'x',yaxis:'y',type:'scatter',mode:'lines',connectgaps:false,
        legendgroup:'Temperature',legendgrouptitle:{{text:'Temperature'}},
        line:{{color:'#c43c39',width:2,dash:'dash',shape:'hv'}},
        hovertemplate:'%{{x|%d.%m.%Y %H:%M:%S}} UTC<br>Setpoint: %{{y:.2f}} °C<extra></extra>'}}]);
    }}
    narrowCharts.addEventListener('change',()=>Plotly.relayout(series,legendLayout(baseHeight)));
    if (energy) {{
      const trace=(name,values,row,color,group,unit)=>({{name,x:energy.time,y:values,type:'scatter',mode:'lines',
        xaxis:row===1?'x':'x'+row,yaxis:row===1?'y':'y'+row,connectgaps:name==='Room',line:{{color}},
        legendgroup:group,legendgrouptitle:{{text:group}},
        hovertemplate:`%{{x|%d.%m.%Y %H:%M:%S}} UTC<br>${{name}}: %{{y:.2f}} ${{unit}}<extra></extra>`}});
      const traces=[
        trace('Room',energy.room,1,'#318c47','Temperature','°C'),
        trace('Main',energy.power,4,'#7057b8','Power','W'),
        trace('Floor',energy.floorpower,4,'#d17a00','Power','W'),
        trace('Main',energy.mainwh,5,'#7057b8','Energy','Wh'),
        trace('Floor',energy.floorwh,5,'#d17a00','Energy','Wh')
      ];
      await Plotly.addTraces(series,traces);
      if (pi) {{
        const pt=(name,values,row,color,group)=>({{name,x:pi.time,y:values,type:'scatter',mode:'lines',
          xaxis:'x'+row,yaxis:'y'+row,connectgaps:name==='Room',line:{{color}},
          legendgroup:group,legendgrouptitle:{{text:group}},
          hovertemplate:`%{{x|%d.%m.%Y %H:%M:%S}} UTC<br>${{name}}: %{{y:.2f}} W<extra></extra>`}});
        await Plotly.addTraces(series,[
          pt('Total',pi.demand,6,'#333333','PI demand'),pt('Act',pi.actual,6,'#087f8c','PI demand'),
          pt('Proportional',pi.p,7,'#7057b8','PI components'),pt('Integral',pi.i,7,'#bb5577','PI components'),
          pt('Heat-loss estimate',pi.ff,7,'#318c47','PI components')
        ]);
      }}
    }}
  }}
}}

drawCharts();
</script>
</body></html>
'''
  return document, len(profiles)


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
        document = ('<!doctype html><html lang="en"><meta charset="utf-8">'
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
