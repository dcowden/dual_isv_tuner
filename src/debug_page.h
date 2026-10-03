#pragma once
#include <Arduino.h>

const char DEBUG_PAGE[] PROGMEM = R"HTML(<!doctype html>
<html lang="en"><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Dual servo diagnostics</title>
<style>
body{margin:24px auto;padding:0 16px;max-width:1100px;background:#101923;color:#e1eaf2;font:16px system-ui}
h1{font-size:26px}p{line-height:1.5;color:#b9cbd9}button{padding:10px 16px;margin:0 8px 8px 0;border:0;border-radius:6px;background:#9ce0ca;color:#102820;font:inherit;cursor:pointer}
pre{padding:18px;border:1px solid #365064;border-radius:8px;overflow:auto;background:#080f17;font:13px/1.5 Consolas,monospace;min-height:200px}
#status{display:block;margin:8px 0;color:#9ce0ca}a{color:#9ce0ca}
</style>
<h1>Dual servo diagnostics</h1>
<p>Keep the tuning software connected over USB. Start a drive search, then inspect the byte counts and traffic below.
This page observes traffic only. The switch selects replies; commands still go to both drives.</p>
<button id="pause">Pause display</button><button id="save">Save capture</button>
<span id="status" role="status">Connecting to ESP32...</span>
<pre id="capture">Waiting for capture...</pre>
<p>TX counts mean bytes accepted by the UART driver, not confirmed at the drive connector.
Hex rows group nearby bytes; they are not decoded protocol frames. Only the newest 256 rows are retained.
The overwrite count describes lost diagnostic history, not necessarily lost serial data.</p>
<script>
let paused=false, latest='', lastSuccess=0;
const status=document.getElementById('status'), capture=document.getElementById('capture');
document.getElementById('pause').onclick=()=>{
  paused=!paused;
  document.getElementById('pause').textContent=paused?'Resume display':'Pause display';
  status.textContent=paused?'Display paused; the ESP32 continues capturing.':'Resuming...';
};
document.getElementById('save').onclick=()=>{
  if(!latest){status.textContent='No capture received yet.';return;}
  const url=URL.createObjectURL(new Blob([latest],{type:'text/plain'}));
  const a=document.createElement('a');a.href=url;a.download='servo-capture-'+new Date().toISOString().replace(/[:.]/g,'-')+'.txt';
  a.click();setTimeout(()=>URL.revokeObjectURL(url),1000);
};
async function poll(){
  if(!paused){
    const controller=new AbortController(), timer=setTimeout(()=>controller.abort(),4000);
    try{
      const response=await fetch('/capture.txt',{cache:'no-store',signal:controller.signal});
      if(!response.ok)throw new Error('HTTP '+response.status);
      const text=await response.text();
      if(!paused){latest=text;capture.textContent=text;lastSuccess=Date.now();status.textContent='Live - updated '+new Date().toLocaleTimeString();}
    }catch(error){
      if(!paused)status.textContent='Disconnected or ESP32 restarting. Retrying...'+(lastSuccess?' Last update '+Math.round((Date.now()-lastSuccess)/1000)+'s ago.':'');
    }finally{clearTimeout(timer);}
  }
  setTimeout(poll,750);
}
poll();
</script></html>)HTML";
