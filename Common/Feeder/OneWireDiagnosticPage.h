// OneWireDiagnosticPage.h, Version: 1.00
#pragma once

static const char OneWireDiagnosticPage[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>OneWire Diagnose</title>
<style>
body{font-family:sans-serif;max-width:1000px;margin:1em auto;padding:0 1em}button,a{margin:.25em .4em .25em 0}pre{background:#eee;padding:.8em;overflow:auto;white-space:pre-wrap}table{border-collapse:collapse}td{padding:.15em .8em .15em 0;vertical-align:top}
</style>
</head>
<body>
<h1>OneWire Produktivzyklus</h1>
<p>
<button onclick="Refresh()">Aktualisieren</button>
<button onclick="ClearLog()">Log + Statistik löschen</button>
<a href="/OneWireDiagLog.csv" download>CSV herunterladen</a>
</p>
<table id="Stats"></table>
<h2>Zustandswechsel</h2>
<pre id="Log">Lade...</pre>
<script>
async function Fetch(URL,Options){const R=await fetch(URL,Options);if(!R.ok)throw new Error('HTTP '+R.status);return R;}
async function Refresh(){
 try{
  const S=await (await Fetch('/OneWireDiagStats/?t='+Date.now(),{cache:'no-store'})).json();
  const T=document.getElementById('Stats');T.textContent='';
  for(const [K,V] of Object.entries(S)){const R=T.insertRow();R.insertCell().textContent=K;R.insertCell().textContent=V;}
  document.getElementById('Log').textContent=await (await Fetch('/OneWireDiagLog.csv?t='+Date.now(),{cache:'no-store'})).text();
 }catch(E){document.getElementById('Log').textContent='Fehler: '+E.message;}
}
async function ClearLog(){
 if(!confirm('OneWire-Log und Statistik löschen?'))return;
 try{const R=await Fetch('/OneWireDiagClear/',{method:'POST',cache:'no-store'});if((await R.text()).trim()!=='1')throw new Error('Reset fehlgeschlagen');await Refresh();}
 catch(E){alert('Fehler: '+E.message);}
}
Refresh();
</script>
</body>
</html>)HTML";
