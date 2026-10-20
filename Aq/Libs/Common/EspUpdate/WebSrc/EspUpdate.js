// EspUpdate.js, Version: 1.10
function EspUpdateFileChanged(Input){
 const Name=Input.closest("tr")?.nextElementSibling?.querySelector(".EspUpdateFileName");
 if(Name)Name.textContent=Input.files[0]?.name||"";}

function ValidateEspUpdateForm(Form){
 const Inputs=Form.querySelectorAll('input[type="file"]');
 let SelectedCount=0;
 for(const Input of Inputs){
  const File=Input.files[0];
  const Suffixes=(Input.dataset.suffixes||Input.dataset.suffix||"").split(",").filter(Boolean);
  if(!File){
   if(Input.required){alert("Select the required update file.");return false;}
   continue;}
  ++SelectedCount;
  if(Suffixes.length&&!Suffixes.some(Suffix=>File.name.endsWith(Suffix))){
   alert(`Invalid file: ${File.name}\nRequired suffix: ${Suffixes.join(" or ")}`);
   return false;}}
 if(!SelectedCount){alert("Select at least one update file.");return false;}
 return true;}

function EspUpdateFormatBytes(Bytes){
 if(Bytes<1024)return `${Bytes} B`;
 if(Bytes<1024*1024)return `${(Bytes/1024).toFixed(1)} KiB`;
 return `${(Bytes/(1024*1024)).toFixed(2)} MiB`;}

function EspUpdateSetControls(Disabled){
 for(const Control of document.querySelectorAll('.EspUpdateControl'))
  Control.disabled=Disabled;}

function EspUpdateSetStatus(Text,IsError=false){
 const Status=document.getElementById("EspUpdateStatus");
 Status.textContent=Text;
 Status.classList.toggle("Error",IsError);}

function EspUpdateWaitForRestart(){
 const Check=()=>{
  fetch(`/Index.html?UpdateReload=${Date.now()}`,{cache:"no-store"})
   .then(Response=>{
    if(Response.ok)location.replace("/Index.html");
    else setTimeout(Check,1000);})
   .catch(()=>setTimeout(Check,1000));};
 setTimeout(Check,1500);}

function SubmitEspUpdateForm(Form){
 if(!ValidateEspUpdateForm(Form))return false;

 const Progress=document.getElementById("EspUpdateProgress");
 const UploadText=document.getElementById("EspUpdateUploadText");
 const UploadBar=document.getElementById("EspUpdateUploadBar");
 const FileProgress=document.getElementById("EspUpdateFileProgress");
 const FileText=document.getElementById("EspUpdateFileText");
 const FileBar=document.getElementById("EspUpdateFileBar");
 let FileSummary=document.getElementById("EspUpdateFileSummary");
 if(!FileSummary){
  FileSummary=document.createElement("div");
  FileSummary.id="EspUpdateFileSummary";
  FileProgress.insertBefore(FileSummary,FileText);}

 Progress.hidden=false;
 UploadText.textContent="Upload: 0 %";
 UploadBar.value=0;
 FileProgress.hidden=true;
 FileSummary.textContent="";
 FileBar.max=1;
 FileBar.value=0;
 EspUpdateSetStatus("");

 const Data=new FormData(Form);
 for(const Input of Form.querySelectorAll('input[type="file"]'))
  if(!Input.files[0])Data.delete(Input.name);
 Data.append("_EspUpdateProgress","1");
 EspUpdateSetControls(true);
 const Xhr=new XMLHttpRequest();
 let ResponseOffset=0;
 let ResponseBuffer="";
 let RestartReceived=false;
 let ErrorReceived=false;

 const HandleLine=Line=>{
  if(!Line)return;
  if(Line==="PROCESS"){
   EspUpdateSetStatus("Upload abgeschlossen – Update wird verarbeitet ...");
   return;}
  if(Line.startsWith("Files changed: ")){
   FileSummary.textContent=Line;
   FileProgress.hidden=false;
   return;}
  if(Line.startsWith("FILES ")){
   const Parts=Line.split(" ");
   if(Parts.length===3){
    const Done=Number(Parts[1]);
    const Total=Number(Parts[2]);
    if(Number.isFinite(Done)&&Number.isFinite(Total)){
     FileProgress.hidden=false;
     FileText.textContent=`Dateien: ${Done} / ${Total}`;
     FileBar.max=Math.max(Total,1);
     FileBar.value=Total?Done:1;}}
   return;}
  if(Line==="FIRMWARE"){
   EspUpdateSetStatus("Firmware wird aktiviert ...");
   return;}
  if(Line==="RESTART"){
   RestartReceived=true;
   EspUpdateSetStatus("Neustart – warte auf Gerät ...");
   EspUpdateWaitForRestart();
   return;}
  if(Line.startsWith("ERROR ")){
   ErrorReceived=true;
   EspUpdateSetStatus(`Update fehlgeschlagen: ${Line.substring(6)}`,true);
   EspUpdateSetControls(false);}}

 const ProcessResponse=Final=>{
  ResponseBuffer+=Xhr.responseText.substring(ResponseOffset);
  ResponseOffset=Xhr.responseText.length;
  let End;
  while((End=ResponseBuffer.indexOf("\n"))>=0){
   HandleLine(ResponseBuffer.substring(0,End).trim());
   ResponseBuffer=ResponseBuffer.substring(End+1);}
  if(Final&&ResponseBuffer.trim()){
   HandleLine(ResponseBuffer.trim());
   ResponseBuffer="";}};

 let Url=Form.action;
 const FirmwareInput=Form.querySelector('input[name="firmware"]');
 if(FirmwareInput&&FirmwareInput.files[0]){
  const Separator=Url.includes("?")?"&":"?";
  Url+=`${Separator}_EspFirmwareSize=${FirmwareInput.files[0].size}`;}
 Xhr.open("POST",Url,true);
 Xhr.upload.onprogress=Event=>{
  if(!Event.lengthComputable)return;
  const Percent=Math.min(100,Event.loaded*100/Event.total);
  UploadBar.value=Percent;
  UploadText.textContent=`Upload: ${Math.round(Percent)} % (${EspUpdateFormatBytes(Event.loaded)} / ${EspUpdateFormatBytes(Event.total)})`;};
 Xhr.upload.onload=()=>{
  UploadBar.value=100;
  EspUpdateSetStatus("Upload abgeschlossen – Update wird verarbeitet ...");};
 Xhr.onprogress=()=>ProcessResponse(false);
 Xhr.onload=()=>{
  ProcessResponse(true);
  if(!RestartReceived&&!ErrorReceived){
   EspUpdateSetStatus("Ungültige oder unvollständige Update-Antwort.",true);
   EspUpdateSetControls(false);}};
 Xhr.onerror=()=>{
  if(RestartReceived)return;
  EspUpdateSetStatus("Verbindung während des Updates abgebrochen.",true);
  EspUpdateSetControls(false);};
 Xhr.onabort=()=>{
  EspUpdateSetStatus("Upload abgebrochen.",true);
  EspUpdateSetControls(false);};
 Xhr.send(Data);
 return false;}
