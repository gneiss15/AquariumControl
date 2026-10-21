// Index.js, Version: 1.23
const ViewportFitMargin=8;
const DashboardSettingTimeoutMilliseconds=500;
let ViewportFitTimer=null;
let ViewportMutationObserver=null;
let DashboardSettingTimer=null;
let PendingLanguageNr=null;
let ServoStepsAvailable=true;
let HomeInputTestAvailable=false;
let HomeInputTestTimer=null;
let ConfigEntryTypes={};
let SaveRequestsPending={};
const ManualFeedDoneReturnMilliseconds=15000;

function NotifyDashboard(Type,Values={}){
  if(window.parent===window)return;
  window.parent.postMessage(Object.assign({Source:"GnConfigPage",Type:Type},Values),"*");}
function ApplySetting(Name,Value){
  if(Name!=="LNr")return;
  const Nr=Number(Value);
  if(!Number.isInteger(Nr))return;
  if(DashboardSettingTimer!==null){clearTimeout(DashboardSettingTimer);DashboardSettingTimer=null;}
  PendingLanguageNr=Nr;
  if(LangCnt>0)LSelect(Nr,false);}
function ReadLocalSetting(Name){
  return Name==="LNr"?GetNrCookie(Name):null;}
function StoreLocalSetting(Name,Value){
  if(Name==="LNr")SetCookie(Name,Value);}
function GetSetting(Name){
  if(window.parent===window){ApplySetting(Name,ReadLocalSetting(Name));return;}
  if(DashboardSettingTimer!==null)clearTimeout(DashboardSettingTimer);
  DashboardSettingTimer=setTimeout(function(){DashboardSettingTimer=null;ApplySetting(Name,ReadLocalSetting(Name));},DashboardSettingTimeoutMilliseconds);
  NotifyDashboard("GetSetting",{Name:Name});}
function SetSetting(Name,Value){
  if(window.parent===window){StoreLocalSetting(Name,Value);return;}
  if(DashboardSettingTimer!==null)clearTimeout(DashboardSettingTimer);
  DashboardSettingTimer=setTimeout(function(){DashboardSettingTimer=null;StoreLocalSetting(Name,Value);},DashboardSettingTimeoutMilliseconds);
  NotifyDashboard("SetSetting",{Name:Name,Value:Value});}
function FitViewport(){
  ViewportFitTimer=null;
  document.body.style.zoom="";
  const AvailableWidth=document.documentElement.clientWidth;
  if(AvailableWidth<=0)return;
  const TargetWidth=Math.max(1,AvailableWidth-ViewportFitMargin);
  const Menu=document.querySelector(".TopBar");
  const VisiblePages=[...document.querySelectorAll(".MenuSelectDiv")].filter(function(Element){return getComputedStyle(Element).display!=="none";});
  let Scale=1;
  for(let Iteration=0;Iteration<3;++Iteration){
    document.body.style.zoom=Scale<0.999?String(Scale):"";
    const PageWidths=VisiblePages.map(function(Element){const Table=Element.querySelector("table");return Table?Table.scrollWidth:Element.scrollWidth;});
    const ContentWidth=Math.max(Menu?Menu.scrollWidth:0,...PageWidths);
    if(Scale===1&&ContentWidth<=AvailableWidth)break;
    const VisualWidth=ContentWidth*Scale;
    if(VisualWidth<=TargetWidth+0.5)break;
    Scale=Math.max(0.1,Scale*TargetWidth/VisualWidth);}
  document.body.style.zoom=Scale<0.999?String(Scale):"";
  NotifyDashboard("ViewportScale",{Scale:Scale});}
function ScheduleViewportFit(){
  if(ViewportFitTimer!==null)clearTimeout(ViewportFitTimer);
  ViewportFitTimer=setTimeout(FitViewport,0);}
function HandleDashboardMessage(Event){
  if(Event.source!==window.parent||!Event.data||Event.data.Source!=="GnDashboard"||Event.data.Type!=="SettingValue")return;
  if(Event.data.Found===false){
    const Value=ReadLocalSetting(Event.data.Name);
    ApplySetting(Event.data.Name,Value);
    SetSetting(Event.data.Name,Value);
    return;}
  ApplySetting(Event.data.Name,Event.data.Value);}
function SetupDashboardIntegration(){
  window.addEventListener("message",HandleDashboardMessage);
  window.addEventListener("resize",ScheduleViewportFit);
  ViewportMutationObserver=new MutationObserver(ScheduleViewportFit);
  ViewportMutationObserver.observe(document.getElementById("MainDiv"),{childList:true,subtree:true,characterData:true});
  ScheduleViewportFit();}

//*****************************
//Get Json Data
function SetUpdatedTime(){
 document.getElementById("UpdatedValue").textContent=new Date().toLocaleTimeString("de-DE",{hour:"2-digit",minute:"2-digit"});}
function GetJData(uri,onSuccess){
 let xhr=new XMLHttpRequest();
 xhr.onload=function(){
  if(xhr.status===200){SetUpdatedTime();if(onSuccess)onSuccess(JSON.parse(xhr.responseText));}
    else{console.error(xhr);}};
 xhr.open("GET",uri);
 xhr.send();}
function GetTextData(uri,onSuccess){
 let xhr=new XMLHttpRequest();
 xhr.onload=function(){
  if(xhr.status===200){SetUpdatedTime();if(onSuccess)onSuccess(xhr.responseText);}
    else{console.error(xhr);}};
 xhr.open("GET",uri);
 xhr.send();}
//PostRequest PostJRequest
function PostRequest(func,uri,data="{}"){
 console.log( "Post:", data );
 let xhr=new XMLHttpRequest();
 xhr.onload=function(){
  if(xhr.status!==200){console.error(xhr);return;}
  const result=xhr.responseText.trim();
  if(result==="1"){SetUpdatedTime();SetStatus(func,1);}
  else if(result==="0"||result==="")SetStatus(func,0);
  else alert(Tr("Error"+result,result));};
 xhr.open("POST",uri);
 xhr.setRequestHeader('Content-Type','application/json; charset=UTF-8');
 xhr.send(data);
 return xhr;}
function PostJRequest(func,uri,jdata){
 return PostRequest(func,uri,JSON.stringify(jdata));}
function PostSaveJRequest(func,uri,jdata,buttonId){
 if(SaveRequestsPending[buttonId])return;
 let SaveButton=document.getElementById(buttonId);
 SaveRequestsPending[buttonId]=true;
 if(SaveButton)SaveButton.disabled=true;
 let xhr=PostJRequest(func,uri,jdata);
 xhr.addEventListener("loadend",function(){
  SaveRequestsPending[buttonId]=false;
  if(SaveButton)SaveButton.disabled=false;});}
//Set Document-Title
function ChangeTitle(newTitle){
 document.title = newTitle;}
//*****************************
//change value of Cb
function ChChg(ele){ele.value=ele.checked?1:0;}
//Make DOM Elements (with Value)
function MkCb(val=0){return `<input type="checkbox" value=${val} ${val?"checked ":""}onclick="ChChg(this)">`;}
function MkBool(){return MkCb(0);}
function MkSel(val){let r="";for(let i=1;i<=9;i++)r+=`<option${(val==i)?" selected":""}>${i}</option>`;return `<select>${r}</select>`;}	
function MkTm(val){return `<input type="time" value="${val}">`;}
//Make DOM Elements
function MkTxt(){return `<input type=text>`;}
function MkPw(){return `<input type=password data-bwignore="true">`;}
function MkNr(min_,max_,val){return `<input type=number${min_==undefined?"":` min=${min_}`}${max_==undefined?"":` max=${max_}`}${val==undefined?"":` value=${val}`}>`;}
function MkPin(){return MkNr(0,15);}
function MkPercent(){return MkNr(0,100);}
function MkUint(){return MkNr(0,10000);}
function MkMode(){
 const nr2Name=["Station","Accesspoint","Station + Accesspoint"];
 let r="";for(let i=1;i<=3;i++)r+=`<option value=${i}>${nr2Name[i-1]}</option>`;
 return `<select data-number>${r}</select>`;}
function MkTemperatureUnit(){return `<select><option value="C">°C</option><option value="F">°F</option></select>`;}
function MkCell(inner,extra=""){return `<td>${inner}${extra!=""?" ":""}${extra}</td>`;}
function MkRow(inner,id){return `<tr${id==undefined?"":" id=${id}"}>${inner}</tr>`;}
function Icon(name){return `<svg class="Icon" aria-hidden="true"><use href="#Icon${name}"></use></svg>`;}
function RowNumberCell(){return `<td class="RowNumber"><button type="button" class="DragHandle" draggable="true" data-tr-title="MoveRow">${Icon("Drag")}</button><span class="Nr"></span></td>`;}
function DeleteButton(){return `<button type="button" class="Delete" data-tr-title="DeleteRow">${Icon("Trash")}</button>`;}
function RenumberFeedings(){
 let numbers=document.querySelectorAll("#FeedData .Nr");
 for(let i=0;i<numbers.length;++i)numbers[i].textContent=i+1;}
function EnableFeedDrag(row){
 let handle=row.querySelector(".DragHandle");
 handle.addEventListener("dragstart",function(e){
  row.classList.add("Dragging");
  e.dataTransfer.effectAllowed="move";
  e.dataTransfer.setData("text/plain","");});
 handle.addEventListener("dragend",function(){row.classList.remove("Dragging");});
 row.addEventListener("dragover",function(e){
  e.preventDefault();
  let drag=document.querySelector("#FeedData .Dragging");
  if(drag&&drag!==row){
   let before=e.clientY<row.getBoundingClientRect().top+row.offsetHeight/2;
   row.parentNode.insertBefore(drag,before?row:row.nextSibling);
   RenumberFeedings();}});}
//Get Value of DOM Elements
function GetCellVal(cell){return cell.firstChild.value;}
function GetCellIntVal(cell){return parseInt(GetCellVal(cell));}
function CellGetStr(cell){return cell.innerHTML;}
function CellEleGetVal(cell){if(cell.firstChild.matches('[type="checkbox"]'))return cell.firstChild.checked;if(cell.firstChild.matches('[type="number"],[data-number]'))return parseInt(cell.firstChild.value);return cell.firstChild.value;}
//Set Value of DOM Elements
function SetEle(ele,value){if(ele.matches('[type="checkbox"]')){let bit=value===true||parseInt(value)===1;ele.value=bit?1:0;ele.checked=bit;}else ele.value=value;}
//Cookies
function GetCookie(cname){
 try{
  let name=cname+"=";
  let ca=(decodeURIComponent(document.cookie)).split(';');
  for(let i=0;i<ca.length;i++){
   let c=ca[i];
   while(c.charAt(0)==' '){c=c.substring(1);}
   if(c.indexOf(name)==0)return c.substring(name.length,c.length);}}
 catch(Error){}
 return "";}
function GetNrCookie(cname){
 let r=GetCookie(cname);
 if(r=="")r="0";
 return parseInt(r);}
function SetCookie(cname,value){
 try{document.cookie=cname+"="+value+"; SameSite=Lax";}
 catch(Error){}}
//*****************************
//Change Elements attribute
function ElesDisNone(es){
 for(let i=0;i<es.length;++i)es[i].style.display="none";}
//*****************************
//Tabbed View
function MSelect(nr){
 const nr2Name=["Feedings","Config","ServoSteps","Update","HomeInputTest"];
 if(!Number.isInteger(nr)||nr<0||nr>=nr2Name.length||(nr===2&&!ServoStepsAvailable)||(nr===4&&!HomeInputTestAvailable))nr=0;
 document.getElementById("ResDiv").style.display="none";
 document.getElementById("MainDiv").style.display="block";
 ElesDisNone(document.getElementsByClassName("MenuSelectDiv"));
 document.getElementById("PageSelector").value=nr;
 document.getElementById(nr2Name[nr]).style.display="block";
 if(nr2Name[nr]=="Update")LoadVersions();
 if(nr2Name[nr]=="HomeInputTest")LoadHomeInputTestStatus();
 SetCookie("MNr",nr);
 ScheduleViewportFit();}
function MRestore(){
 MSelect(GetNrCookie("MNr"));}
//*****************************
var Translations={};
var LangCnt=0;
var LangNr=0;
function SetTrData(data){
 Translations=data;
 LangCnt=Translations["Lang"].length;
 let langs=Translations["Lang"];
 let t="";
 for(let i=0;i<LangCnt;++i)t+=`<a class="MenuDdA" id="M${langs[i]}" onclick="LSelect(${i})">${langs[i]}</a>`;
 document.getElementById("LanguageMenu").innerHTML=t;
 LRestore();}
function LoadTranslations(){GetJData('Translations.json',SetTrData);}
function Tr(name,defStr){
 if(!Translations.hasOwnProperty(name)){
   if(defStr==undefined)
     return name;
   return defStr;}
 let res=Translations[name][LangNr];
 if(res!==undefined)
   return res;
 res=Translations[name][0];
 if(res!==undefined)
   return res;
 return name;}
function TrAll(){
 let es=document.querySelectorAll("span[name]");
 for(let i=0;i<es.length;++i){
  let e=es[i];
  e.innerHTML=Tr(e.attributes["name"].value,e.innerHTML);}
 for(let e of document.querySelectorAll("[data-tr]"))e.textContent=Tr(e.dataset.tr,e.textContent);
 for(let e of document.querySelectorAll("[data-tr-title]")){
  const text=Tr(e.dataset.trTitle,e.title);
  e.title=text;
  e.setAttribute("aria-label",text);}}
function LSelect(nr,NotifyParent=true){
 if(!Number.isInteger(nr)||nr<0||nr>=LangCnt)nr=0;
 let lang=Translations["Lang"][nr];
 for(let e of document.getElementsByClassName("MenuDdA"))e.classList.remove("active");
 let entry=document.getElementById("M"+lang);
 if(entry)entry.classList.add("active");
 document.getElementById("MLang").textContent=lang;
 LangNr=nr;
 TrAll();
 ScheduleViewportFit();
 if(NotifyParent)SetSetting("LNr",nr);}
function LRestore(){
 if(PendingLanguageNr!==null)LSelect(PendingLanguageNr,false);
 else GetSetting("LNr");}
//*****************************
//Create dynamic Entries
function CD(type,name){
 const type2Func={"t":MkTxt,"T":MkPw,"n":MkNr,"p":MkPin,"P":MkPercent,"u":MkUint,"w":MkMode,"b":MkBool,"f":MkTemperatureUnit};
 if(type[0]=="d")return MkRow('<th style="padding-top:15px;"></th>');
 return MkRow(MkCell(name)+MkCell(type2Func[type[0]](),type[1]));}
//*****************************
//Feedings(data)
const FeedBitToCell=[3,4,5,6,7,8,9,1];
function FeedingsGetBits(row){
 let bits=0;
 for(let b=0;b<8;++b)bits|=GetCellVal(row.cells[FeedBitToCell[b]])<<b;
 return bits;}
function FindActiveFeedConflict(data){
 for(let i=0;i<data.length;++i){
  if(!(data[i][1]&128))continue;
  const days=data[i][1]&127;
  for(let j=i+1;j<data.length;++j)
   if((data[j][1]&128)&&data[i][0]===data[j][0]&&(days&(data[j][1]&127)))return [i,j];}
 return null;}
function NewUuid()
 {
  if( crypto.randomUUID )return crypto.randomUUID();
  const data=crypto.getRandomValues(new Uint8Array(16));
  data[6]=(data[6]&15)|64;
  data[8]=(data[8]&63)|128;
  const hex=[...data].map(value=>value.toString(16).padStart(2,'0'));
  return `${hex.slice(0,4).join('')}-${hex.slice(4,6).join('')}-${hex.slice(6,8).join('')}-${hex.slice(8,10).join('')}-${hex.slice(10).join('')}`;
 }
function AddFeedingRow(value){
 if(value===undefined)value=[new Date().toLocaleTimeString().slice(0,5),255,1];
 let body=document.getElementById("FeedData");
 let row=body.insertRow();
 row.dataset.id=value[3]||NewUuid();
 let cells=RowNumberCell()+MkCell(MkCb((value[1]&128)?1:0))+MkCell(MkTm(value[0]));
 for(let i=0;i<7;++i)cells+=MkCell(MkCb((value[1]&(1<<i))?1:0));
 row.innerHTML=cells+MkCell(MkSel(value[2]))+MkCell(DeleteButton());
 row.querySelector(".Delete").onclick=function(){row.remove();RenumberFeedings();};
 EnableFeedDrag(row);
 RenumberFeedings();
 TrAll();}
function SetFeedings(data){
 document.getElementById("FeedData").innerHTML="";
 for(let row of data)AddFeedingRow(row);}
function LoadFeedings(){GetJData('FeedTimes.json',SetFeedings);}
//*****************************
//Feedings(submit)
function SubmitFeedings(){
 let data=[];
 for(let row of document.getElementById("FeedData").rows)
  data.push([GetCellVal(row.cells[2]),FeedingsGetBits(row),GetCellIntVal(row.cells[10]),row.dataset.id||NewUuid()]);
 const conflict=FindActiveFeedConflict(data);
 if(conflict){
  alert(`${Tr("DuplicateActiveTimeRows")} ${conflict[0]+1} / ${conflict[1]+1}`);
  return;}
 PostSaveJRequest(0,"/SetFeedTimes/",data,"FeedTimesSaveButton");}
//*****************************
//Feedings(Buttons)
function ManualFeed(){
  PostRequest(1,"/ManualFeed/");}
function StopFeed(){
  PostRequest(5,"/StopFeed/");}
//*****************************
//Config(Entries)
function SetConfigEntries(data){
 ConfigEntryTypes=data;
 let res='';for(var key in data)res+=CD(data[key],key); 
 document.getElementById("ConfigData").innerHTML=res;
 TrAll();
 GetJData('Config.json',SetConfigData);}
function LoadConfig(){GetJData('ConfigEntries.json',SetConfigEntries);}  
//*****************************
//Config(data)
function SetConfigData(data){
 let tab=document.getElementById("ConfigData");
 for(let r=0,row;(row=tab.rows[r]);++r){
  let key=CellGetStr(row.cells[0]);
  if(key.length){
   SetEle(row.cells[1].firstChild,data[key]);
   if(key=="Hostname")
    ChangeTitle(data[key]);
   }}}
//*****************************
//Config(submit)
function SubmitConfig(){
 let tab=document.getElementById("ConfigData");
 let cfg={};for(let r=0,row;(row=tab.rows[r]);++r){
  let key=CellGetStr(row.cells[0]);
  if(key.length)cfg[key]=CellEleGetVal(row.cells[1]);}
 //Chk Config
 if(cfg["ApPw"].length!=0){//ApPw (no matter whether the AP mode is switched on or off)
   if(cfg["ApPw"].length<8){//ApPw (no matter whether the AP mode is switched on or off)
    alert(Tr("ErrorApPw"));return;}
   if(cfg["ApSsid"].length==0){//ApPw but no ApSsid
    alert(Tr("ErrorApSsid"));return;}}
 if(cfg["WifiMode"]&1){//STA enabled
  if(cfg["WlanSsid"].length==0){//WlanSsid not set
   alert(Tr("ErrorWlanSsid"));return;}
  if(cfg["NtpServerAdr"].length==0){//NtpServerAdr not set
   alert(Tr("ErrorNtpServer"));return;}
  if(cfg["TZ"].length==0){//TZ not set
   alert(Tr("ErrorTZ"));return;}}
 if(cfg["DallasPresent"]){
  const usedPins=Object.keys(ConfigEntryTypes)
   .filter(function(Name){return Name!=="DallasPin"&&ConfigEntryTypes[Name][0]==="p";})
   .map(function(Name){return cfg[Name];});
  if(usedPins.includes(cfg["DallasPin"])){
   alert(Tr("ErrorDallasPinConflict"));return;}
  if(cfg["TemperatureUnit"]!=="C"&&cfg["TemperatureUnit"]!=="F"){
   alert(Tr("ErrorTemperatureUnit"));return;}}
 PostSaveJRequest(2,"/SetConfig/",cfg,"ConfigSaveButton");}
//*****************************
//ServoSteps(data)
function RenumberServoSteps(){
 let numbers=document.querySelectorAll("#ServoStepData .Nr");
 for(let i=0;i<numbers.length;++i)numbers[i].textContent=i+1;}
function EnableServoStepDrag(row){
 let handle=row.querySelector(".DragHandle");
 handle.addEventListener("dragstart",function(e){
  row.classList.add("Dragging");
  e.dataTransfer.effectAllowed="move";
  e.dataTransfer.setData("text/plain","");});
 handle.addEventListener("dragend",function(){row.classList.remove("Dragging");});
 row.addEventListener("dragover",function(e){
  e.preventDefault();
  let drag=document.querySelector("#ServoStepData .Dragging");
  if(drag&&drag!==row){
   let before=e.clientY<row.getBoundingClientRect().top+row.offsetHeight/2;
   row.parentNode.insertBefore(drag,before?row:row.nextSibling);
   RenumberServoSteps();}});}
function AddServoStep(value){
 if(value===undefined)value=[0,20,0];
 let body=document.getElementById("ServoStepData");
 let row=body.insertRow();
 row.innerHTML=RowNumberCell()+MkCell(MkNr(0,180,value[0]))+MkCell(MkNr(20,3000,value[1]))+MkCell(MkNr(0,10000,value[2]))+MkCell(DeleteButton());
 row.querySelector(".Delete").onclick=function(){row.remove();RenumberServoSteps();};
 EnableServoStepDrag(row);
 RenumberServoSteps();
 TrAll();}
function SetServoSteps(data){
 document.getElementById("ServoStepData").innerHTML="";
 for(let row of data)AddServoStep(row);}
function LoadServoSteps(){GetJData('ServoSteps.json',SetServoSteps);}
//*****************************
//ServoSteps(submit)
function SubmitServoSteps(){
 let tab=document.getElementById("ServoStepData");
 let jdata=[];
 for(let r=0,row;(row=tab.rows[r]);++r){
  jdata.push([GetCellVal(row.cells[1]),GetCellVal(row.cells[2]),GetCellVal(row.cells[3])]);}
 PostSaveJRequest(4,"/SetServoSteps/",jdata,"ServoStepsSaveButton");}
//*****************************
//HOME input test
function HomeInputTestStatisticsText(Name,Statistics){
 if(!Statistics||!Statistics.Valid)return `${Name}: --`;
 return `${Name}: ${Tr("Transitions")}=${Statistics.Transitions}, LOW=${Statistics.LowMs} ms, HIGH=${Statistics.HighMs} ms`;}
function SetHomeInputTestStatus(data){
 let lines=[`${Tr("State")}: ${data.State}`];
 lines.push(HomeInputTestStatisticsText("INPUT",data.Input));
 lines.push(HomeInputTestStatisticsText("INPUT_PULLUP",data.InputPullup));
 if(data.Detected){
  lines.push("");
  lines.push(`FeederSimpleHomeActiveLow = ${data.ActiveLow?1:0}`);
  lines.push(`FeederSimpleHomeUsePullup = ${data.UsePullup?1:0}`);
  lines.push(`${Tr("VerificationRotations")}: ${data.VerificationRuns}/2`);}
 if(data.Error){lines.push("");lines.push(`${Tr("Error")}: ${data.Error}`);}
 if(data.Complete){lines.push("");lines.push(Tr("HomeInputTestComplete"));}
 document.getElementById("HomeInputTestStatus").textContent=lines.join("\n");
 document.getElementById("HomeInputTestStartButton").disabled=!!data.Running;
 document.getElementById("HomeInputTestStopButton").disabled=!data.Running;
 if(HomeInputTestTimer!==null){clearTimeout(HomeInputTestTimer);HomeInputTestTimer=null;}
 if(data.Running)HomeInputTestTimer=setTimeout(LoadHomeInputTestStatus,500);
 ScheduleViewportFit();}
function LoadHomeInputTestStatus(){
 if(!HomeInputTestAvailable)return;
 GetJData(`/HomeInputTestStatus/?t=${Date.now()}`,SetHomeInputTestStatus);}
function PostHomeInputTest(uri){
 let xhr=new XMLHttpRequest();
 xhr.onload=function(){
  if(xhr.status!==200||xhr.responseText.trim()!=="1"){console.error(xhr);alert(Tr("HomeInputTestRequestFailed"));return;}
  SetUpdatedTime();LoadHomeInputTestStatus();};
 xhr.open("POST",uri);
 xhr.setRequestHeader("Content-Type","application/json; charset=UTF-8");
 xhr.send("{}");}
function StartHomeInputTest(){PostHomeInputTest("/HomeInputTestStart/");}
function StopHomeInputTest(){PostHomeInputTest("/HomeInputTestStop/");}
//*****************************
//Update
let FirmwareRevision="?";
let LittleFsRevision="?";
function UpdateVersionSummary(){
 document.getElementById("VersionSummary").textContent=`FW ${FirmwareRevision} · FS ${LittleFsRevision}`;}
function SetFirmwareRevision(data){
 FirmwareRevision=data.FirmwareVersion||"?";
 UpdateVersionSummary();}
function SetDeviceInfo(data){
 document.getElementById("DeviceName").textContent=data.Name||data.DeviceId||"?";
 SetFirmwareRevision(data);
 const capabilities=Array.isArray(data.Capabilities)?data.Capabilities:[];
 const rotor=capabilities.includes("ExternalButton")||capabilities.includes("DirectMotor");
 ServoStepsAvailable=capabilities.includes("ServoSteps")||!rotor;
 const option=document.querySelector('#PageSelector option[value="2"]');
 if(option)option.hidden=!ServoStepsAvailable;
 if(ServoStepsAvailable)LoadServoSteps();
 else if(parseInt(document.getElementById("PageSelector").value)===2)MSelect(0);
 HomeInputTestAvailable=capabilities.includes("HomeInputTest");
 const homeTestOption=document.getElementById("HomeInputTestMenuOption");
 if(homeTestOption)homeTestOption.hidden=!HomeInputTestAvailable;
 if(!HomeInputTestAvailable&&parseInt(document.getElementById("PageSelector").value)===4)MSelect(0);
 ScheduleViewportFit();}
function ParseVersion(data,name){
 for(const line of data.split(/\r?\n/)){
  const separator=line.indexOf("=");
  if(separator<0)continue;
  if(line.slice(0,separator).trim()==name)return line.slice(separator+1).trim();}
 return "?";}
function SetLittleFsRevision(data){
 LittleFsRevision=ParseVersion(data,"LittleFS");
 UpdateVersionSummary();}
function LoadVersions(){
 const cacheBust=`?t=${Date.now()}`;
 GetJData(`/api/v1/device${cacheBust}`,SetDeviceInfo);
 GetTextData(`/Versions.txt${cacheBust}`,SetLittleFsRevision);}
function ResetDevice(){
 PostRequest(6,"/ResetDevice/");}
//*****************************
//ResDiv
function ReloadCountdown(){
 var t=10;var x=setInterval(function(){--t;document.getElementById("ReloadTimeOut").innerHTML=t;if(t<=0){clearInterval(x);location.replace("/Index.html");}},1000);
 return `${Tr("ReloadTxt2")}<span id="ReloadTimeOut">10</span>${Tr("ReloadTxt3")}<br><button onclick=location="/Index.html">${Tr("Reload Now")}</button>`;}
function ResHeader(ok,trTxt,fontSize=2){
 const ok2Color=["red","green"];
 return `<font color="${ok2Color[ok]}" style="font-size:${fontSize}em;"><strong>${Tr(trTxt)}</strong></font><br>`;}
function FeedTimesNok(e){e.innerHTML=`${ResHeader(0,"FeedTimesNok")}${ReloadCountdown()}`;}
function FeedTimesOk(e){e.innerHTML=`${ResHeader(1,"FeedTimesOk")}${ReloadCountdown()}`;}
function ConfigNok(e){e.innerHTML=`${ResHeader(0,"ConfigNok")}${ReloadCountdown()}`;}
function ConfigOk(e){e.innerHTML=`${ResHeader(1,"ConfigOk")}${Tr("ReloadTxt1")}<br><button onclick=location="/Index.html">${Tr("Reload")}</button>`;}
function ManualFeedNok(e){e.innerHTML=`${ResHeader(0,"ManualFeedNok")}${ReloadCountdown()}`;}
function ManualFeedOk(e){setTimeout(function(){PostRequest(3,"/MFeedChk/");},1000);e.innerHTML=`${ResHeader(1,"ManualFeedOk",6)}`;}
function StopFeedNok(e){e.innerHTML=`${ResHeader(0,"StopFeedNok")}${ReloadCountdown()}`;}
function StopFeedOk(e){e.innerHTML=`${ResHeader(1,"StopFeedOk",6)}`;}
function MFeedChkActive(e){setTimeout(function(){PostRequest(3,"/MFeedChk/");},1000);}
function MFeedChkDone(e){
 e.innerHTML+=`<br><button onclick=location="/Index.html">${Tr("ManualFeedDone")}</button>`;
 setTimeout(function(){location.replace("/Index.html");},ManualFeedDoneReturnMilliseconds);}
function ServoStepsNok(e){e.innerHTML=`${ResHeader(0,"ServoStepsNok")}${ReloadCountdown()}`;}
function ServoStepsOk(e){e.innerHTML=`${ResHeader(1,"ServoStepsOk")}${ReloadCountdown()}`;}
function ResetNok(e){e.innerHTML=`${ResHeader(0,"ResetNok")}${ReloadCountdown()}`;}
function ResetOk(e){e.innerHTML=`${ResHeader(1,"ResetOk")}${Tr("ReloadTxt1")}<br><button onclick=location="/Index.html">${Tr("Reload")}</button>`;}
function SetStatus(func,ok){
 const funcTab=[[FeedTimesNok,FeedTimesOk],[ManualFeedNok,ManualFeedOk],[ConfigNok,ConfigOk],[MFeedChkActive,MFeedChkDone],[ServoStepsNok,ServoStepsOk],[StopFeedNok,StopFeedOk],[ResetNok,ResetOk]];
 let ResDiv=document.getElementById("ResDiv");
 document.getElementById("MainDiv").style.display="none";
 ResDiv.style.display="block";
 let f=funcTab[func][ok];
 if(f) f(ResDiv);}
//*****************************
//Setup
function SetupOnLoaded(){
 LoadTranslations();
 LoadFeedings();
 LoadConfig();
 LoadVersions();
 MRestore();
 SetupDashboardIntegration();
 }
