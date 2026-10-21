// EspUpdateRescue.js, Version: 1.07
let EspUpdateDeviceType=null;
let EspUpdateDeviceInfo=null;

function EspUpdateFileChanged(Input){
 const Name=Input.closest("tr")?.nextElementSibling?.querySelector(".EspUpdateFileName");
 if(Name)Name.textContent=Input.files[0]?.name||"";}

function EspUpdateSetControls(Disabled){
 for(const Control of document.querySelectorAll('.EspUpdateControl'))Control.disabled=Disabled;}

function EspUpdateSetStatus(Text,IsError=false){
 const Status=document.getElementById("EspUpdateStatus");
 Status.textContent=Text;
 Status.classList.toggle("Error",IsError);}

function EspUpdateFormatBytes(Bytes){
 if(Bytes<1024)return `${Bytes} B`;
 if(Bytes<1024*1024)return `${(Bytes/1024).toFixed(1)} KiB`;
 return `${(Bytes/(1024*1024)).toFixed(2)} MiB`;}

function EspReadUInt16(Data,Offset){return Data[Offset]|(Data[Offset+1]<<8);}
function EspReadUInt32(Data,Offset){return (Data[Offset]|(Data[Offset+1]<<8)|(Data[Offset+2]<<16)|(Data[Offset+3]<<24))>>>0;}
function EspAscii(Data,Offset,Length){
 let Text="";
 for(let Index=0;Index<Length&&Data[Offset+Index];++Index)Text+=String.fromCharCode(Data[Offset+Index]);
 return Text;}
function EspMagic(Data,Offset,Text){
 if(Data.length<Offset+Text.length)return false;
 for(let Index=0;Index<Text.length;++Index)if(Data[Offset+Index]!==Text.charCodeAt(Index))return false;
 return true;}

async function EspGetDeviceType(){
 if(EspUpdateDeviceType)return EspUpdateDeviceType;
 const Data=await EspGetDeviceInfo();
 EspUpdateDeviceType=Data.DeviceType;
 return EspUpdateDeviceType;}

async function EspGetDeviceInfo(){
 if(EspUpdateDeviceInfo)return EspUpdateDeviceInfo;
 const Response=await fetch(`/api/v1/device?ArtifactCheck=${Date.now()}`,{cache:"no-store"});
 if(!Response.ok)throw Error("Could not read device type from Main.");
 const Data=await Response.json();
 if(!Data.DeviceType)throw Error("Main did not report a DeviceType.");
 EspUpdateDeviceInfo=Data;
 return EspUpdateDeviceInfo;}

async function EspLoadRescueTargetVersion(){
 const Display=document.getElementById("EspRescueTargetVersion");
 if(!Display)return;
 try{
  const Data=await EspGetDeviceInfo();
  Display.textContent=Data.RescueTargetVersion||"unknown";}
 catch(Error){Display.textContent="unavailable";}}

document.addEventListener("DOMContentLoaded",EspLoadRescueTargetVersion);

async function EspValidateSignedArtifact(File,ExpectedArtifact){
 const HeaderSize=64,SignatureHeaderSize=48;
 if(File.size<HeaderSize+SignatureHeaderSize)throw Error(`Signed artifact is too short: ${File.name}`);
 const Data=new Uint8Array(await File.slice(0,HeaderSize+SignatureHeaderSize).arrayBuffer());
 if(!EspMagic(Data,0,"ESPDT01\0")||EspReadUInt16(Data,8)!==64||Data[10]!==1)
  throw Error(`Invalid DeviceType header: ${File.name}`);
 if(Data[11]!==ExpectedArtifact)throw Error(`Wrong artifact type: ${File.name}`);
 const DeviceType=EspAscii(Data,16,32);
 const ExpectedDeviceType=await EspGetDeviceType();
 if(DeviceType!==ExpectedDeviceType)throw Error(`Wrong DeviceType: ${DeviceType||"?"}; expected ${ExpectedDeviceType}`);
 for(let Index=48;Index<64;++Index)if(Data[Index]!==0)throw Error(`Unsupported DeviceType header extension: ${File.name}`);
 if(!EspMagic(Data,64,"ESPSIG01")||EspReadUInt16(Data,72)!==48||Data[74]!==1||Data[75]!==1)
  throw Error(`Invalid signed-artifact header: ${File.name}`);
 const PayloadSize=EspReadUInt32(Data,12);
 const SignatureSize=EspReadUInt32(Data,76);
 if(!PayloadSize||!SignatureSize||112+SignatureSize+PayloadSize!==File.size)
  throw Error(`Signed artifact size mismatch: ${File.name}`);
 return File;}

async function EspRescueValidateFile(Input){
 const File=Input.files[0];
 if(!File)return null;
 const Suffix=Input.dataset.suffix||"";
 if(Suffix&&!File.name.endsWith(Suffix))throw Error(`Invalid file: ${File.name}\nRequired suffix: ${Suffix}`);
 return EspValidateSignedArtifact(File,Number(Input.dataset.artifact));}

function EspRescuePost(Path,Body,ProgressText){
 return new Promise((Resolve,Reject)=>{
  const Xhr=new XMLHttpRequest();
  Xhr.open("POST",Path,true);
  if(Body&&ProgressText)Xhr.upload.onprogress=Event=>{
   if(!Event.lengthComputable)return;
   const Percent=Math.min(100,Event.loaded*100/Event.total);
   document.getElementById("EspUpdateUploadBar").value=Percent;
   document.getElementById("EspUpdateUploadText").textContent=
    `${ProgressText}: ${Math.round(Percent)} % (${EspUpdateFormatBytes(Event.loaded)} / ${EspUpdateFormatBytes(Event.total)})`;};
  Xhr.onload=()=>{
   if(Xhr.status>=200&&Xhr.status<300)Resolve(Xhr.responseText);
   else Reject(Error(Xhr.responseText||`HTTP ${Xhr.status}`));};
  Xhr.onerror=()=>Reject(Error("Connection failed"));
  Xhr.send(Body||null);});}

function EspRescueWaitStatus(Target,Attempt,Started,State){
 const Seconds=Math.floor((Date.now()-Started)/1000);
 const Alive=Attempt&1?"●":"○";
 EspUpdateSetStatus(`${Target} · Versuch ${Attempt} · ${Seconds} s · ${Alive}${State?` · ${State}`:""}`);}

async function EspRescueFetch(Url,ReadText=false,TimeoutMs=1500){
 const Controller=new AbortController();
 const Timer=setTimeout(()=>Controller.abort(),TimeoutMs);
 try{
  const Response=await fetch(Url,{cache:"no-store",signal:Controller.signal});
  const Text=ReadText&&Response.ok?await Response.text():"";
  return {Response,Text};}
 finally{clearTimeout(Timer);}}

async function EspRescueWaitForRescue(){
 const Started=Date.now(),Timeout=Started+120000;
 let Attempt=0,MainState="Main reachable";
 while(Date.now()<Timeout){
  ++Attempt;
  EspRescueWaitStatus("Waiting for Rescue",Attempt,Started,MainState);
  try{
   const Probe=await EspRescueFetch(`/`,true);
   if(Probe.Response.ok){
    if(Probe.Text.includes("<title>Rescue</title>")){
     EspRescueWaitStatus("Rescue ready",Attempt,Started,"Main offline");
     return;}}}
  catch(Error){}
  try{
   const Probe=await EspRescueFetch(`/api/v1/device?MainState=${Date.now()}`);
   MainState=Probe.Response.ok?"Main reachable":"Main offline";}
  catch(Error){MainState="Main offline";}
  await new Promise(Resolve=>setTimeout(Resolve,750));}
 throw Error("Rescue did not become reachable within 120 seconds");}

async function EspRescueWaitForMain(){
 const Started=Date.now(),Timeout=Started+120000;
 let Attempt=0;
 while(Date.now()<Timeout){
  ++Attempt;
  EspRescueWaitStatus("Waiting for Main",Attempt,Started,"");
  try{
   const Probe=await EspRescueFetch(`/api/v1/device?MainProbe=${Date.now()}`);
   if(Probe.Response.ok){
    EspRescueWaitStatus("Main ready",Attempt,Started,"");
    location.replace(`/Index.html?Updated=${Date.now()}`);
    return;}}
  catch(Error){}
  await new Promise(Resolve=>setTimeout(Resolve,750));}
 throw Error("Main did not become reachable within 120 seconds");}

async function EspRescueRunUpdate(Firmware,Filesystem){
 const Bar=document.getElementById("EspUpdateUploadBar");
 const Text=document.getElementById("EspUpdateUploadText");
 EspUpdateSetStatus("Starting Rescue ...");
 Bar.value=0;
 try{await EspRescuePost("/StartRescue/",null,null);}catch(Error){}
 await EspRescueWaitForRescue();
 EspUpdateSetStatus("Rescue ready");
 if(Firmware){Bar.value=0;await EspRescuePost("/Firmware",Firmware,"Firmware");}
 if(Filesystem){Bar.value=0;await EspRescuePost("/Filesystem",Filesystem,"Filesystem");}
 Text.textContent="Upload complete";
 EspUpdateSetStatus("Activating update ...");
 await EspRescuePost("/Commit",null,null);
 EspUpdateSetStatus("Restarting Main ...");
 await EspRescueWaitForMain();}

function SubmitEspRescueUpdateForm(Form){
 (async()=>{
  const Firmware=await EspRescueValidateFile(Form.querySelector('input[name="firmware"]'));
  const Filesystem=await EspRescueValidateFile(Form.querySelector('input[name="filesystem"]'));
  if(!Firmware&&!Filesystem)throw Error("Select firmware and/or filesystem.");
  document.getElementById("EspUpdateProgress").hidden=false;
  EspUpdateSetControls(true);
  await EspRescueRunUpdate(Firmware,Filesystem);
 })().catch(Error=>{
  document.getElementById("EspUpdateProgress").hidden=false;
  EspUpdateSetStatus(`Update failed: ${Error.message}`,true);
  EspUpdateSetControls(false);});
 return false;}

async function EspStartRescue(){
 const Progress=document.getElementById("EspUpdateProgress");
 Progress.hidden=false;
 EspUpdateSetControls(true);
 document.getElementById("EspUpdateUploadBar").value=0;
 document.getElementById("EspUpdateUploadText").textContent="Starting Rescue";
 EspUpdateSetStatus("Starting Rescue ...");
 try{await EspRescuePost("/StartRescue/",null,null);}catch(Error){}
 try{
  await EspRescueWaitForRescue();
  location.replace(`/`);
 }
 catch(Error){
  EspUpdateSetStatus(`Start Rescue failed: ${Error.message}`,true);
  EspUpdateSetControls(false);}}

function EspToggleRescueUpdate(){
 const Div=document.getElementById("EspRescueSelfUpdate");
 Div.hidden=!Div.hidden;}

function SubmitEspRescueSelfUpdateForm(Form){
 (async()=>{
  const Rescue=await EspRescueValidateFile(Form.querySelector('input[name="rescue"]'));
  if(!Rescue)throw Error("Select Rescue artifact.");
  document.getElementById("EspUpdateProgress").hidden=false;
  EspUpdateSetControls(true);
  EspUpdateSetStatus("Updating Rescue ...");
  const Data=new FormData();
  Data.append("rescue",Rescue,Rescue.name);
  await EspRescuePost("/UpdateRescue/",Data,"Rescue");
  document.getElementById("EspUpdateUploadText").textContent="Upload complete";
  EspUpdateSetStatus("Rescue updated and verified");
  EspUpdateSetControls(false);
 })().catch(Error=>{
  document.getElementById("EspUpdateProgress").hidden=false;
  EspUpdateSetStatus(`Rescue update failed: ${Error.message}`,true);
  EspUpdateSetControls(false);});
 return false;}
