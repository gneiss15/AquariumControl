// HeatHistory.js, Version: 1.04
// Use browser-local time; clock ticks never request device or server data.
function UpdateBrowserClock() {
  const Clock = document.getElementById('BrowserClock');
  if (!Clock) return;
  const German = document.documentElement.lang === 'de';
  const Now = new Date();
  Clock.textContent = Now.toLocaleString(German ? 'de-DE' : 'en-GB', {
    year: 'numeric', month: '2-digit', day: '2-digit',
    hour: '2-digit', minute: '2-digit', second: '2-digit', hourCycle: 'h23'
  });
  Clock.dateTime = Now.toISOString();
  Clock.title = German ? 'Datum und Uhrzeit (Browserzeit)' : 'Date and time (browser clock)';
}
function StartBrowserClock() {
  UpdateBrowserClock();
  setInterval(UpdateBrowserClock, 1000);
}
if (document.readyState === 'loading')
  document.addEventListener('DOMContentLoaded', StartBrowserClock, {once: true});
else StartBrowserClock();
// End browser clock.

let chartSets = [];
let energySets = [];
let piSets = [];

const plotConfig = {responsive: true, scrollZoom: true, displaylogo: false};
const baseLayout = {
  autosize: true,
  height: 640,
  margin: {l: 78, r: 24, t: 78, b: 56},
  paper_bgcolor: '#fff',
  plot_bgcolor: '#fff',
  hovermode: 'x unified',
  dragmode: 'zoom',
  font: {family: 'system-ui, sans-serif', color: '#39494f'},
  xaxis: {
    type: 'date',
    domain: [0, 1],
    anchor: 'y',
    matches: 'x3',
    showticklabels: false,
    showgrid: true,
    gridcolor: '#e3eaec'
  },
  xaxis2: {
    type: 'date',
    domain: [0, 1],
    anchor: 'y2',
    matches: 'x3',
    showticklabels: false,
    showgrid: true,
    gridcolor: '#e3eaec'
  },
  xaxis3: {
    type: 'date',
    domain: [0, 1],
    anchor: 'y3',
    title: {text:'Time (UTC)'},
    tickformat: '%d.%m.%Y<br>%H:%M',
    showgrid: true,
    gridcolor: '#e3eaec',

  },
  yaxis: {
    domain: [0.39, 1],
    anchor: 'x',
    title: {text:'Temperature (°C)'},
    automargin: true,
    gridcolor: '#e3eaec'
  },
  yaxis2: {
    domain: [0.18, 0.30],
    anchor: 'x2',
    title: {text:'Floor'},
    range: [-0.1, 1.1],
    tickmode: 'array',
    tickvals: [0, 1],
    ticktext: ['OFF', 'ON'],
    fixedrange: true,
    showgrid: false,
    zeroline: false,
    automargin: true
  },
  yaxis3: {
    domain: [0, 0.12],
    anchor: 'x3',
    title: {text:'Main'},
    range: [-0.1, 1.1],
    tickmode: 'array',
    tickvals: [0, 1],
    ticktext: ['OFF', 'ON'],
    fixedrange: true,
    showgrid: false,
    zeroline: false,
    automargin: true
  }
};

const narrowCharts = window.matchMedia('(max-width: 900px)');
function legendLayout(baseHeight) {
  return narrowCharts.matches
    ? {legend: {groupclick:'toggleitem',orientation:'h',x:0,y:-0.14,xanchor:'left',yanchor:'top'},
        'margin.b':220,height:baseHeight+164}
    : {legend: {groupclick:'toggleitem',orientation:'v',x:1.02,y:1,xanchor:'left',yanchor:'top'},
        'margin.b':56,height:baseHeight};
}
async function drawCharts() {
  if (!window.Plotly) {
    document.getElementById('chart-error').hidden = false;
    return;
  }
  for (let index = 0; index < chartSets.length; index += 1) {
    const energy = energySets[index];
    const pi = piSets[index];
    const source = energy || chartSets[index];
    const layout = JSON.parse(JSON.stringify(baseLayout));
    if (energy) {
      layout.height = pi ? 1120 : 820;
      const count=pi?7:5;
      const domains = pi ? [[.83,1],[.75,.79],[.67,.71],[.50,.62],[.33,.45],[.16,.28],[0,.12]]
        : [[.76,1],[.66,.71],[.56,.61],[.28,.50],[0,.22]];
      for (let row=1; row<=count; ++row) {
        const suffix=row===1?'':String(row);
        const x='x'+suffix, y='y'+suffix;
        layout['xaxis'+suffix]={type:'date',domain:[0,1],anchor:y,
          ...(row<count?{matches:pi?'x7':'x5'}:{}),showticklabels:row===count,
          showgrid:true,gridcolor:'#e3eaec',
          ...(row===count?{title:{text:'Time (UTC)'},tickformat:'%d.%m.%Y<br>%H:%M',
            }:{})};
        layout['yaxis'+suffix]={domain:domains[row-1],anchor:x,automargin:true,
          title:{text:['Temperature (°C)','Floor','Main','Power (W)','Energy (Wh)','PI demand (W)','PI components (W)'][row-1]},
          gridcolor:'#e3eaec',...([4,6,7].includes(row)?{rangemode:'tozero',zeroline:true,zerolinecolor:'#52636b',zerolinewidth:1.5}:{}),...(row===2||row===3?{range:[-0.1,1.1],tickvals:[0,1],
            ticktext:['OFF','ON'],fixedrange:true,showgrid:false,zeroline:false}:{})};
      }
    }
    const series = document.getElementById(`series-${index}`);
    const baseHeight=layout.height;
    const responsiveLegend=legendLayout(baseHeight);
    layout.legend=responsiveLegend.legend;
    layout.margin.b=responsiveLegend['margin.b'];
    layout.height=responsiveLegend.height;
    const commonTrace = {x: source.time, connectgaps: false};
    await Plotly.newPlot(series, [
      {
        ...commonTrace,
        y: source.temp,
        connectgaps: true,
        yaxis: 'y',
        xaxis: 'x',
        type: 'scatter',
        mode: source.time.length < 500 ? 'lines+markers' : 'lines',
        name: 'Water',
        legendgroup: 'Temperature',
        legendgrouptitle: {text: 'Temperature'},
        line: {color: '#087f8c', width: 2.5},
        marker: {size: 5},
        hovertemplate: '%{x|%d.%m.%Y %H:%M:%S} UTC<br>Water: %{y:.2f} °C<extra></extra>'
      },
      {
        ...commonTrace,
        y: source.floor,
        yaxis: 'y2',
        xaxis: 'x2',
        text: source.floor.map(value => value === null ? 'Unknown' : value ? 'ON' : 'OFF'),
        type: 'scatter',
        mode: 'lines',
        name: 'Floor',
        legendgroup: 'Switches',
        legendgrouptitle: {text: 'Switches'},
        line: {color: '#d17a00', width: 3, shape: 'hv'},
        hovertemplate: '%{x|%d.%m.%Y %H:%M:%S} UTC<br>Floor: %{text}<extra></extra>'
      },
      {
        ...commonTrace,
        y: source.main,
        yaxis: 'y3',
        xaxis: 'x3',
        text: source.main.map(value => value === null ? 'Unknown' : value ? 'ON' : 'OFF'),
        type: 'scatter',
        mode: 'lines',
        name: 'Main',
        legendgroup: 'Switches',
        line: {color: '#7057b8', width: 3, shape: 'hv'},
        hovertemplate: '%{x|%d.%m.%Y %H:%M:%S} UTC<br>Main: %{text}<extra></extra>'
      }
    ], layout, plotConfig);
    const target = energy ? {time:energy.time,setpoint:energy.setpoint} : pi;
    if (target && target.setpoint.some(value=>value!==null)) {
      await Plotly.addTraces(series,[{name:'Setpoint',x:target.time,y:target.setpoint,
        xaxis:'x',yaxis:'y',type:'scatter',mode:'lines',connectgaps:false,
        legendgroup:'Temperature',legendgrouptitle:{text:'Temperature'},
        line:{color:'#c43c39',width:2,dash:'dash',shape:'hv'},
        hovertemplate:'%{x|%d.%m.%Y %H:%M:%S} UTC<br>Setpoint: %{y:.2f} °C<extra></extra>'}]);
    }
    legendListeners.push(()=>Plotly.relayout(series,legendLayout(baseHeight)));
    if (energy) {
      const trace=(name,values,row,color,group,unit)=>({name,x:energy.time,y:values,type:'scatter',mode:'lines',
        xaxis:row===1?'x':'x'+row,yaxis:row===1?'y':'y'+row,connectgaps:name==='Room',line:{color},
        legendgroup:group,legendgrouptitle:{text:group},
        hovertemplate:`%{x|%d.%m.%Y %H:%M:%S} UTC<br>${name}: %{y:.2f} ${unit}<extra></extra>`});
      const traces=[
        trace('Room',energy.room,1,'#318c47','Temperature','°C'),
        trace('Main',energy.power,4,'#7057b8','Power','W'),
        trace('Floor',energy.floorpower,4,'#d17a00','Power','W'),
        trace('Main',energy.mainwh,5,'#7057b8','Energy','Wh'),
        trace('Floor',energy.floorwh,5,'#d17a00','Energy','Wh')
      ];
      await Plotly.addTraces(series,traces);
      if (pi) {
        const pt=(name,values,row,color,group)=>({name,x:pi.time,y:values,type:'scatter',mode:'lines',
          xaxis:'x'+row,yaxis:'y'+row,connectgaps:name==='Room',line:{color},
          legendgroup:group,legendgrouptitle:{text:group},
          hovertemplate:`%{x|%d.%m.%Y %H:%M:%S} UTC<br>${name}: %{y:.2f} W<extra></extra>`});
        await Plotly.addTraces(series,[
          pt('Total',pi.demand,6,'#333333','PI demand'),pt('Act',pi.actual,6,'#087f8c','PI demand'),
          pt('Proportional',pi.p,7,'#7057b8','PI components'),pt('Integral',pi.i,7,'#bb5577','PI components'),
          pt('Heat-loss estimate',pi.ff,7,'#318c47','PI components')
        ]);
      }
    }
  }
}

const legendListeners=[];
narrowCharts.addEventListener('change',()=>legendListeners.forEach(fn=>fn()));
let loadSequence=0;
function splitSeries(rows, keys) {
  const out=Object.fromEntries(keys.map(key=>[key,[]]));
  let previous=null;
  for(const row of rows) {
    const time=Date.parse(row[0]);
    if(previous!==null && (time-previous>15000 || row[row.length-1]===true)) {
      out.time.push(row[0]);
      for(const key of keys.slice(1))out[key].push(null);
    }
    keys.forEach((key,index)=>out[key].push(row[index]));
    previous=time;
  }
  return out;
}
function energySeries(rows) {
  if(!rows.length)return null;
  const out={time:[],temp:[],room:[],floor:[],main:[],power:[],floorpower:[],mainwh:[],floorwh:[],setpoint:[]};
  let mainWh=0,floorWh=0,previous=null;
  for(const row of rows) {
    const [stamp,temp,room,floor,main,power,floorpower,mainInterval,floorInterval,mainKnown,floorKnown,boundary,setpoint]=row;
    const time=Date.parse(stamp);
    if(previous!==null && (time-previous>15000 || boundary)) {
      out.time.push(stamp);for(const key of Object.keys(out).filter(k=>k!=='time'))out[key].push(key==='setpoint' && time-previous<=15000?out[key].at(-1):null);
    }
    mainWh+=mainInterval===null?0:mainInterval;floorWh+=floorInterval===null?0:floorInterval;
    out.time.push(stamp);out.temp.push(temp);out.room.push(room);out.floor.push(floor);out.main.push(main);
    out.power.push(power);out.floorpower.push(floorpower);out.setpoint.push(setpoint);
    out.mainwh.push(mainKnown?mainWh:null);out.floorwh.push(floorKnown?floorWh:null);
    previous=time;
  }
  return out;
}
function parseCsvRows(text) {
  const rows=[];let row=[],field='',quoted=false;
  for(let i=0;i<text.length;i++) {
    const ch=text[i];
    if(quoted) {
      if(ch==='"' && text[i+1]==='"'){field+='"';i++;}
      else if(ch==='"')quoted=false;
      else field+=ch;
    } else if(ch==='"' && field==='')quoted=true;
    else if(ch===';'){row.push(field);field='';}
    else if(ch==='\n'){row.push(field.endsWith('\r')?field.slice(0,-1):field);rows.push(row);row=[];field='';}
    else field+=ch;
  }
  if(!quoted && (field!=='' || row.length)){row.push(field);rows.push(row);}
  if(rows.length)rows[0][0]=rows[0][0].replace(/^\uFEFF/,'');
  return rows;
}
function decodeHistoryCsv(text,contentType) {
  const match=/boundary=([^;]+)/i.exec(contentType || '');
  if(!match)throw new Error('Missing CSV bundle boundary');
  const blocks=text.split('--'+match[1].trim());let manifest=null;
  const payloads=new Map();
  for(const block of blocks.slice(1)) {
    const split=block.indexOf('\r\n\r\n');if(split<0)continue;
    const headers=block.slice(0,split),body=block.slice(split+4).replace(/\r\n$/,'');
    if(/Content-Type: application\/json/i.test(headers))manifest=GnJson.Read('THttpHistoryManifest',body);
    else {const id=/Content-ID: ([0-9]+)/i.exec(headers);if(id)payloads.set(id[1],body);}
  }
  if(!manifest)throw new Error('Invalid CSV manifest');
  const profiles=new Map(),start=Date.parse(manifest.From),end=Date.parse(manifest.To);
  for(const file of manifest.Files) {
    if(!payloads.has(file.Id))throw new Error('Missing CSV segment');
    if(!/^HeatProfileAq[0-9]+$/.test(file.Prof) || !['Energy','Pi','Temperature'].includes(file.Kind))throw new Error('Invalid CSV segment');
    const parsed=parseCsvRows(payloads.get(file.Id)),header=parsed.shift() || [];
    const columns=Object.fromEntries(header.map((key,index)=>[key,index]));
    if(columns.Time===undefined)throw new Error('Missing CSV time column');
    let profile=profiles.get(file.Prof);
    if(!profile){profile={Id:file.Prof,Temp:[],Energy:[],Heat:[]};profiles.set(file.Prof,profile);}
    let boundary=true;
    for(const cells of parsed) {
      if(cells.length!==header.length)continue;
      const get=key=>cells[columns[key]],time=Date.parse(get('Time'));
      if(!Number.isFinite(time) || time<start || time>end)continue;
      const stamp=new Date(time).toISOString();
      const number=(key,valid=true)=>{
        const cell=get(key);if(!valid || cell===undefined || cell==='')return null;
        const value=Number(cell);return Number.isFinite(value)?value:null;
      };
      const state=key=>get(key)==='1'?1:get(key)==='0'?0:null;
      if(file.Kind==='Energy') {
        const floor=state('FloorState'),main=state('MainState'),nominal=number('FloorNominalW');
        let mi=number('MainIntervalWh',['valid','held','interpolated'].includes(get('MainIntervalQuality')));
        let fi=number('FloorIntervalWh',['valid','held','interpolated'].includes(get('FloorIntervalQuality')));
        for(const role of ['Main','Floor']) {
          const seconds=number(role+'IntervalSeconds');
          if(seconds!==null && time-seconds*1000<start){if(role==='Main')mi=null;else fi=null;}
        }
        profile.Energy.push([stamp,number('TempC',get('TempValid')==='1'),number('RoomTempC',get('RoomTempValid')==='1'),floor,main,
          number('MainCorrectedW',get('MainPowerValid')==='1'),nominal!==null && floor!==null?nominal*floor:null,mi,fi,
          (get('MainPowerValid')==='1' || ['held','interpolated'].includes(get('MainIntervalQuality'))) && ['', 'valid','held','interpolated'].includes(get('MainIntervalQuality')),
          (floor!==null || ['held','interpolated'].includes(get('FloorIntervalQuality'))) && ['', 'valid','held','interpolated'].includes(get('FloorIntervalQuality')),boundary,file.Setpoint]);
      } else if(file.Kind==='Pi') {
        if(get('Mode')!=='Active')continue;
        profile.Heat.push([stamp,...['DemandW','ActualWatts','FloorDemandW','MainDemandW','ProportionalW','IntegralW','FeedForwardW'].map(key=>number(key,get('Valid')==='1')),file.Setpoint,boundary]);
      } else profile.Temp.push([stamp,number('Temp'),state('Floor'),state('Main')]);
      boundary=false;
    }
  }
  const data={...manifest,Profs:[...profiles.values()].filter(p=>p.Energy.length || p.Heat.length || p.Temp.length)};
  for(const profile of data.Profs)for(const kind of ['Energy','Heat','Temp'])profile[kind].sort((a,b)=>a[0].localeCompare(b[0]));
  return data;
}
async function loadRange() {
  const started=Date.now();
  const request=++loadSequence;
  const button=document.getElementById('Reload');button.disabled=true;
  const range=document.getElementById('Range');range.disabled=true;
  const status=document.getElementById('LoadStatus');status.textContent='Loading…';
  try {
    const response=await fetch('/api/history.csv?range='+encodeURIComponent(document.getElementById('Range').value),{cache:'no-store'});
    if(!response.ok)throw new Error('History request failed ('+response.status+')');
    const text=await response.text(),received=Date.now();
    const data=decodeHistoryCsv(text,response.headers.get('Content-Type'));const parsed=Date.now();if(request!==loadSequence)return;
    legendListeners.length=0;
    document.querySelectorAll('.chart').forEach(node=>{if(window.Plotly)Plotly.purge(node);});
    const charts=document.getElementById('Charts');charts.replaceChildren();
    chartSets=[];energySets=[];piSets=[];
    for(const profile of data.Profs) {
      const index=chartSets.length,article=document.createElement('article'),title=document.createElement('h2');
      title.textContent=profile.Id;article.append(title);
      const chart=document.createElement('div');chart.className='chart';chart.id='series-'+index;article.append(chart);charts.append(article);
      chartSets.push(splitSeries(profile.Temp,['time','temp','floor','main']));
      energySets.push(energySeries(profile.Energy));
      const pi=splitSeries(profile.Heat,['time','demand','actual','floor','main','p','i','ff','setpoint']);
      piSets.push(pi.time.length?pi:null);
    }
    await drawCharts();
    if(request!==loadSequence)return;
    status.textContent=data.Profs.length?('UTC: '+data.From+' – '+data.To):'No recorded data in this range.';
    status.dataset.loadMilliseconds=String(Date.now()-started);
    status.dataset.transferMilliseconds=String(received-started);
    status.dataset.parseMilliseconds=String(parsed-received);
    status.dataset.drawMilliseconds=String(Date.now()-parsed);
  } catch(error) {if(request===loadSequence)status.textContent=error.message;}
  finally {if(request===loadSequence){button.disabled=false;range.disabled=false;}}
}
document.getElementById('Range').addEventListener('change',loadRange);
document.getElementById('Reload').addEventListener('click',loadRange);
loadRange();
