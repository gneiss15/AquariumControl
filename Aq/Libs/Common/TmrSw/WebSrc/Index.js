// Index.js, Version: 2.12
const ViewportFitMargin=8;
const DashboardSettingTimeoutMilliseconds=500;
let ViewportFitTimer=null;
let ViewportMutationObserver=null;
let DashboardSettingTimer=null;
let PendingLanguageNr=null;
let SaveRequestsPending={};

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
function PostSaveRequest(func,uri,data,buttonId){
 if(SaveRequestsPending[buttonId])return;
 let SaveButton=document.getElementById(buttonId);
 SaveRequestsPending[buttonId]=true;
 if(SaveButton)SaveButton.disabled=true;
 let xhr=PostRequest(func,uri,data);
 xhr.addEventListener("loadend",function(){
  SaveRequestsPending[buttonId]=false;
  if(SaveButton)SaveButton.disabled=false;});}
function PostSaveJRequest(func,uri,jdata,buttonId){
 return PostSaveRequest(func,uri,JSON.stringify(jdata),buttonId);}
//Set Document-Title
function ChangeTitle(newTitle){
 document.title = newTitle;}
//*****************************
//change value of Cb
function ChChg(ele){ele.value=ele.checked?1:0;}
//Make DOM Elements (with Value)
function MkCb(val,cls="")
 {
  var s2=cls?`class="${cls}" `:"";
  return `<input type="checkbox" value=${val} ${val?"checked ":""}`+s2+`onclick="ChChg(this)">`;
 }
function MkSel(val){let r="";for(let i=1;i<=9;i++)r+=`<option${(val==i)?" selected":""}>${i}</option>`;return `<select>${r}</select>`;}	
function MkTm(val){return `<input type="time" value="${val}">`;}
//Make DOM Elements
function MkTxt(){return `<input type=text>`;}
function MkPw(){return `<input type=password>`;}
function MkNr(min_,max_,val){return `<input type=number${min_==undefined?"":` min=${min_}`}${max_==undefined?"":` max=${max_}`}${val==undefined?"":` value=${val}`}>`;}
function MkPin(){return MkNr(0,15);}
function MkPercent(){return MkNr(0,100);}
function MkUint(){return MkNr(0,10000);}
function MkMode(){
 const nr2Name=["Station","Accesspoint","Station + Accesspoint"];
 let r="";for(let i=1;i<=3;i++)r+=`<option value=${i}>${nr2Name[i-1]}</option>`;
 return `<select data-number>${r}</select>`;}	
function MkCell(inner,extra=""){return `<td>${inner}${extra!=""?" ":""}${extra}</td>`;}
function MkRow(inner,id){return `<tr${id==undefined?"":" id=${id}"}>${inner}</tr>`;}
function Icon(name){return `<svg class="Icon" aria-hidden="true"><use href="#Icon${name}"></use></svg>`;}
function RowNumberCell(){return `<td class="RowNumber"><button type="button" class="DragHandle" draggable="true" data-tr-title="MoveRow">${Icon("Drag")}</button><span class="Nr"></span></td>`;}
function DeleteButton(){return `<button type="button" class="Delete" data-tr-title="DeleteRow">${Icon("Trash")}</button>`;}
function Renumber(){
 let numbers=document.querySelectorAll("#SwitchData .Nr");
 for(let i=0;i<numbers.length;++i)numbers[i].textContent=i+1;}
function EnableDrag(row){
 let handle=row.querySelector(".DragHandle");
 handle.addEventListener("dragstart",function(e){
  row.classList.add("Dragging");
  e.dataTransfer.effectAllowed="move";
  e.dataTransfer.setData("text/plain","");});
 handle.addEventListener("dragend",function(){row.classList.remove("Dragging");});
 row.addEventListener("dragover",function(e){
  e.preventDefault();
  let drag=document.querySelector("#SwitchData .Dragging");
  if(drag&&drag!==row){
   let before=e.clientY<row.getBoundingClientRect().top+row.offsetHeight/2;
   row.parentNode.insertBefore(drag,before?row:row.nextSibling);
   Renumber();}});}
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
function ElesDisNone(es)
 {
  for(e of es)
    e.style.display="none";
 }
function ElesEnableOrDisable(es,enable)
 {
  for(e of es)
    e.disabled=!enable;
 }
//*****************************
//Tabbed View
let PendingMenuNr=null;
function MSelect(nr,Persist=true){
 const nr2Name=["Times","Config","Update","Power"];
 const Option=document.querySelector(`#PageSelector option[value="${nr}"]`);
 if(!Number.isInteger(nr)||nr<0||nr>=nr2Name.length||!Option)nr=0;
 else if(Option.hidden){
  PendingMenuNr=nr;
  nr=0;
  Persist=false;}
 document.getElementById("ResDiv").style.display="none";
 document.getElementById("MainDiv").style.display="block";
 ElesDisNone(document.getElementsByClassName("MenuSelectDiv"));
 document.getElementById("PageSelector").value=nr;
 document.getElementById(nr2Name[nr]).style.display="block";
 if(nr2Name[nr]=="Update")LoadVersions();
 if(Persist)SetCookie("MNr",nr);
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
function TrAll()
 {
  for(e of document.querySelectorAll("span[name]"))
    e.innerHTML=Tr(e.attributes["name"].value,e.innerHTML);
  for(e of document.querySelectorAll("[data-tr]"))
    e.textContent=Tr(e.dataset.tr,e.textContent);
  for(e of document.querySelectorAll("[data-tr-title]"))
   {
    const text=Tr(e.dataset.trTitle,e.title);
    e.title=text;
    e.setAttribute("aria-label",text);
   }
 }
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
 const type2Func={"t":MkTxt,"T":MkPw,"n":MkNr,"p":MkPin,"P":MkPercent,"u":MkUint,"w":MkMode,"b":MkCb,"c":MkCb};
 if(type[0]=="d")return MkRow('<th style="padding-top:15px;"></th>');
 return MkRow(MkCell(name)+MkCell(type2Func[type[0]](),type[1]));}
//*****************************
//Times(global)
const bit2Col=[3,4,5,6,7,8,9,1];
function TimesGetBits(row)
 {
  let bits=0;
  for(let b=0;b<8;++b)
    bits|=GetCellVal(row.cells[bit2Col[b]])<<b;
  return bits;
 }
function FindActiveTimeConflict(data)
 {
  for(let i=0;i<data.length;++i)
   {
    if(!(data[i][1]&128))continue;
    const days=data[i][1]&127;
    for(let j=i+1;j<data.length;++j)
      if((data[j][1]&128)&&data[i][0]===data[j][0]&&(days&(data[j][1]&127)))
        return [i,j];
   }
  return null;
 }
//*****************************
//Times(data)
function NewUuid()
 {
  if( crypto.randomUUID )return crypto.randomUUID();
  const data=crypto.getRandomValues(new Uint8Array(16));
  data[6]=(data[6]&15)|64;
  data[8]=(data[8]&63)|128;
  const hex=[...data].map(value=>value.toString(16).padStart(2,'0'));
  return `${hex.slice(0,4).join('')}-${hex.slice(4,6).join('')}-${hex.slice(6,8).join('')}-${hex.slice(8,10).join('')}-${hex.slice(10).join('')}`;
 }
function AddTimeRow(value)
 {
  if(value===undefined)value=[new Date().toLocaleTimeString().slice(0,5),255,0];
  let body=document.getElementById("SwitchData");
  let row=body.insertRow();
  row.dataset.id=value[3]||NewUuid();
  let cells=RowNumberCell()+MkCell(MkCb((value[1]&128)?1:0))+MkCell(MkTm(value[0]));
  for(let i=0;i<7;++i)cells+=MkCell(MkCb((value[1]&(1<<i))?1:0));
  row.innerHTML=cells+MkCell(MkCb(value[2]))+MkCell(DeleteButton());
  row.querySelector(".Delete").onclick=function(){row.remove();Renumber();};
  EnableDrag(row);
  Renumber();
  TrAll();
 }
function SetTimes(data)
 {
  document.getElementById("SwitchData").innerHTML="";
  for(let row of data)AddTimeRow(row);
 }
function LoadTimes(){GetJData('SwitchTimes.json',SetTimes);}
//*****************************
//Times(submit)
function SubmitTimes()
 {
  let data=[];
  for(row of document.getElementById("SwitchData").rows)
    data.push([GetCellVal(row.cells[2]),TimesGetBits(row),GetCellIntVal(row.cells[10]),row.dataset.id||NewUuid()]);
  const conflict=FindActiveTimeConflict(data);
  if(conflict)
   {
    alert(`${Tr("DuplicateActiveTimeRows")} ${conflict[0]+1} / ${conflict[1]+1}`);
    return;
   }
  PostSaveJRequest(0,"/SetSwitchTimes/",data,"TimeSaveBtn");
 }
//*****************************
//Config(Entries)
function SetConfigEntries(data)
 {
  let res='';
  for(key in data)
    res+=CD(data[key],key); 
  document.getElementById("ConfigData").innerHTML=res;
  TrAll();
  GetJData('Config.json',SetConfigData);
 }
function LoadConfig()
 {
  GetJData('ConfigEntries.json',SetConfigEntries);
  LoadChannels();
 }
function FormatRecordArray(Rows,Indent)
 {
  if(!Rows.length)return "[]";
  if(!Rows.every(Row=>Row!==null&&typeof Row==="object"&&!Array.isArray(Row)))
    return JSON.stringify(Rows,null,2);
  const Keys=[];
  for(const Row of Rows)
    for(const Key of Object.keys(Row))
      if(!Keys.includes(Key))Keys.push(Key);
  const Cells=[];
  const Widths={};
  for(const Key of Keys)Widths[Key]=0;
  for(const Row of Rows)
   {
    const RowCells={};
    for(const Key of Keys)
     {
      if(!(Key in Row))continue;
      const Text=`${JSON.stringify(Key)}: ${JSON.stringify(Row[Key])}`;
      RowCells[Key]=Text;
      Widths[Key]=Math.max(Widths[Key],Text.length);
     }
    Cells.push(RowCells);
   }
  const Lines=["["];
  const Prefix=" ".repeat(Indent+2);
  for(let RowIndex=0;RowIndex<Cells.length;++RowIndex)
   {
    const RowCells=Cells[RowIndex];
    const Parts=[];
    for(let KeyIndex=0;KeyIndex<Keys.length;++KeyIndex)
     {
      const Key=Keys[KeyIndex];
      const Later=Keys.slice(KeyIndex+1).some(NextKey=>NextKey in RowCells);
      if(Key in RowCells)
       {
        const Text=RowCells[Key];
        Parts.push(Later?Text+","+" ".repeat(Widths[Key]-Text.length+1):Text);
       }
      else if(Later)Parts.push(" ".repeat(Widths[Key]+2));
     }
    const Comma=RowIndex+1<Cells.length?",":"";
    Lines.push(`${Prefix}{ ${Parts.join("").trimEnd()} }${Comma}`);
   }
  Lines.push(" ".repeat(Indent)+"]");
  return Lines.join("\n");
 }
// Retain every key; record arrays use one aligned row per object.
function FormatEditorJson(Data,Indent=0)
 {
  const Prefix=" ".repeat(Indent);
  if(Array.isArray(Data))return FormatRecordArray(Data,Indent);
  if(Data!==null&&typeof Data==="object")
   {
    const Keys=Object.keys(Data);
    if(!Keys.length)return "{}";
    return "{\n"+Keys.map(Key=>Prefix+"  "+JSON.stringify(Key)+": "+
      FormatEditorJson(Data[Key],Indent+2)).join(",\n")+"\n"+Prefix+"}";
   }
  return JSON.stringify(Data);
 }
function FormatChannelsJson(Data)
 {
  return FormatEditorJson(Data)+"\n";
 }
function LoadChannels()
 {
  GetTextData('Channels.json',data=>
   {
    try{data=FormatChannelsJson(JSON.parse(data));}
    catch(error){}
    document.getElementById("ChannelsText").value=data;
   });
 }
function SubmitChannels()
 {
  const text=document.getElementById("ChannelsText").value;
  let data;
  try{data=FormatChannelsJson(JSON.parse(text));}
  catch(error){alert(`${Tr("InvalidJson")}: ${error.message}`);return;}
  document.getElementById("ChannelsText").value=data;
  PostSaveRequest(1,"/SetChannels/",data,"ChannelsSaveBtn");
 }
//*****************************
//Config(data)
function SetConfigData(data)
 {
  let tab=document.getElementById("ConfigData");
  //for(let r=0,row;(row=tab.rows[r]);++r){
  for(row of tab.rows)
   {
    let key=CellGetStr(row.cells[0]);
    if(key.length)
     {
      SetEle(row.cells[1].firstChild,data[key]);
      if(key=="Hostname")
        ChangeTitle(data[key]);
     }
   }
 }
//*****************************
//Config(submit)
function SubmitConfig()
 {
  let tab=document.getElementById("ConfigData");
  let cfg={};
  for(row of tab.rows)
   {
    let key=CellGetStr(row.cells[0]);
    if(key.length)
      cfg[key]=CellEleGetVal(row.cells[1]);
   }
  //Chk Config
  if(cfg["ApPw"].length!=0)
   {
    //ApPw (no matter whether the AP mode is switched on or off)
    if(cfg["ApSsid"].length==0)
     {
      //ApPw but no ApSsid
      alert(Tr("ErrorApSsid"));
      return;
     }
   }
  if(cfg["WifiMode"]&1)
   {
    //STA enabled
    if(cfg["WlanSsid"].length==0)
     {
      //WlanSsid not set
      alert(Tr("ErrorWlanSsid"));
      return;
     }
    if(cfg["NtpServerAdr"].length==0)
     {
      //NtpServerAdr not set
      alert(Tr("ErrorNtpServer"));
      return;
     }
    if(cfg["TZ"].length==0)
     {
      //TZ not set
      alert(Tr("ErrorTZ"));
      return;
     }
   }
  PostSaveJRequest(1,"/SetConfig/",cfg,"ConfigSaveBtn");
 }
//*****************************
//Update
let FirmwareRevision="?";
let LittleFsRevision="?";
function UpdateVersionSummary(){
 document.getElementById("VersionSummary").textContent=`FW ${FirmwareRevision} · FS ${LittleFsRevision}`;}
function SetPowerMeterAvailable(Available){
 const Option=document.getElementById("PowerPageOption");
 if(!Option)return;
 Option.hidden=!Available;
 if(Available&&PendingMenuNr===3){
  PendingMenuNr=null;
  MSelect(3);
  return;}
 if(!Available){
  if(PendingMenuNr===3)PendingMenuNr=null;
  if(document.getElementById("PageSelector").value==="3")MSelect(0);}}
function SetFirmwareRevision(data){
 document.getElementById("DeviceName").textContent=data.Name||data.DeviceId||"?";
 FirmwareRevision=data.FirmwareVersion||"?";
 SetPowerMeterAvailable(Array.isArray(data.Capabilities)&&data.Capabilities.includes("PowerMeter"));
 UpdateVersionSummary();}
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
 GetJData(`/api/v1/device${cacheBust}`,SetFirmwareRevision);
 GetTextData(`/Versions.txt${cacheBust}`,SetLittleFsRevision);}
function ResetDevice()
 {
  PostRequest(2,"/ResetDevice/");
 }
//*****************************
//ResDiv
function ReloadCountdown(){
 var t=10;var x=setInterval(function(){--t;document.getElementById("ReloadTimeOut").innerHTML=t;if(t<=0){clearInterval(x);location.replace("/Index.html");}},1000);
 return `${Tr("ReloadTxt2")}<span id="ReloadTimeOut">10</span>${Tr("ReloadTxt3")}<br><button onclick=location="/Index.html">${Tr("Reload Now")}</button>`;}
function ResHeader(ok,trTxt,fontSize=2){
 const ok2Color=["red","green"];
 return `<font color="${ok2Color[ok]}" style="font-size:${fontSize}em;"><strong>${Tr(trTxt)}</strong></font><br>`;}
function SwitchTimesNok(e){e.innerHTML=`${ResHeader(0,"SwitchTimesNok")}${ReloadCountdown()}`;}
function SwitchTimesOk(e){e.innerHTML=`${ResHeader(1,"SwitchTimesOk")}${ReloadCountdown()}`;}
function ConfigNok(e){e.innerHTML=`${ResHeader(0,"ConfigNok")}${ReloadCountdown()}`;}
function ConfigOk(e){e.innerHTML=`${ResHeader(1,"ConfigOk")}${Tr("ReloadTxt1")}<br><button onclick=location="/Index.html">${Tr("Reload")}</button>`;}
function ResetNok(e){e.innerHTML=`${ResHeader(0,"ResetNok")}${ReloadCountdown()}`;}
function ResetOk(e){e.innerHTML=`${ResHeader(1,"ResetOk")}${Tr("ReloadTxt1")}<br><button onclick=location="/Index.html">${Tr("Reload")}</button>`;}
function SetStatus(func,ok){
 const funcTab=[[SwitchTimesNok,SwitchTimesOk],[ConfigNok,ConfigOk],[ResetNok,ResetOk]];
 let ResDiv=document.getElementById("ResDiv");
 document.getElementById("MainDiv").style.display="none";
 ResDiv.style.display="block";
 let f=funcTab[func][ok];
 if(f) f(ResDiv);}

function FormatPowerValue(Value,Digits,Unit){
 const NumberValue=Number(Value);
 return Number.isFinite(NumberValue)?`${NumberValue.toFixed(Digits)} ${Unit}`:`— ${Unit}`;}
function SetPowerMeterStatus(Data){
 const Summary=document.getElementById("PowerMeterSummary");
 const Meter=Data?.PowerMeter;
 if(!Meter){
  Summary.hidden=true;
  SetPowerMeterAvailable(false);
  return;}
 SetPowerMeterAvailable(true);
 const Voltage=FormatPowerValue(Meter.Voltage,1,"V");
 const Current=FormatPowerValue(Meter.Current,3,"A");
 const Power=FormatPowerValue(Meter.ActivePower,1,"W");
 Summary.hidden=false;
 Summary.textContent=`${Voltage} · ${Current} · ${Power}`;
 document.getElementById("PowerVoltage").textContent=Voltage;
 document.getElementById("PowerCurrent").textContent=Current;
 document.getElementById("PowerActivePower").textContent=Power;
 const State=document.getElementById("PowerMeterState");
 if(Meter.Valid===false){
  const D=Meter.Diagnostics||{};
  const Cf=Number.isFinite(Number(D.CfEdges))?Number(D.CfEdges):"?";
  const Cf1=Number.isFinite(Number(D.Cf1Edges))?Number(D.Cf1Edges):"?";
  const U=Number.isFinite(Number(D.VoltageIntervals))?Number(D.VoltageIntervals):"?";
  const I=Number.isFinite(Number(D.CurrentIntervals))?Number(D.CurrentIntervals):"?";
  const Sel=Number.isFinite(Number(D.SelectorLevel))?Number(D.SelectorLevel):"?";
  const Load=D.LoadEnabled===true?1:D.LoadEnabled===false?0:"?";
  State.textContent=`${Tr("PowerMeterNotValid")} · CF ${Cf} · CF1 ${Cf1} · U ${U} · I ${I} · SEL ${Sel} · LOAD ${Load}`;
 }else
  State.textContent=Tr("PowerMeterValid");
 State.classList.toggle("Invalid",Meter.Valid===false);
}
function LoadPowerMeterStatus(){
 fetch(`/api/v1/status?Meter=${Date.now()}`,{cache:"no-store"})
  .then(Response=>Response.ok?Response.json():Promise.reject())
  .then(SetPowerMeterStatus)
  .catch(()=>{});
}
let PowerCalibrationValues=null;
let PowerCalibrationMeter=null;

function PowerCalibrationNumber(Id){
 const Value=Number(document.getElementById(Id).value);
 return Number.isFinite(Value)&&Value>0?Value:null;}

function SetPowerCalibrationValues(){
 if(!PowerCalibrationValues)return;
 document.getElementById("CalVoltageValue").textContent=PowerCalibrationValues.VoltageCal??"?";
 document.getElementById("CalCurrentValue").textContent=PowerCalibrationValues.CurrentCal??"?";
 document.getElementById("CalPowerValue").textContent=PowerCalibrationValues.PowerCal??"?";
 document.getElementById("CalDirectVoltage").value=PowerCalibrationValues.VoltageCal??"";
 document.getElementById("CalDirectCurrent").value=PowerCalibrationValues.CurrentCal??"";
 document.getElementById("CalDirectPower").value=PowerCalibrationValues.PowerCal??"";}

async function LoadPowerCalibration(){
 const [CalibrationResponse,StatusResponse]=await Promise.all([
  fetch(`/api/v1/power/calibration?Calibration=${Date.now()}`,{cache:"no-store"}),
  fetch(`/api/v1/status?Calibration=${Date.now()}`,{cache:"no-store"})]);
 if(!CalibrationResponse.ok||!StatusResponse.ok)throw Error("Kalibrierdaten konnten nicht geladen werden.");
 PowerCalibrationValues=await CalibrationResponse.json();
 PowerCalibrationMeter=(await StatusResponse.json()).PowerMeter;
 SetPowerCalibrationValues();}

function TogglePowerCalibration(){
 const Div=document.getElementById("PowerCalibration");
 Div.hidden=!Div.hidden;
 if(!Div.hidden)LoadPowerCalibration().catch(Error=>{
  document.getElementById("PowerCalibrationStatus").textContent=Error.message;});}

async function SavePowerCalibration(Changes){
 if(!PowerCalibrationValues)await LoadPowerCalibration();
 const FactoryWarning=document.getElementById("ShellyFactoryCalibrationWarning");
 if(FactoryWarning){
  if(!window.confirm("Warnung: Die Shelly-Werks-Kalibrierung wird dadurch überschrieben. Fortfahren?"))return;
 }
 const Values=Object.assign({},PowerCalibrationValues,Changes);
 if(FactoryWarning)Values.ConfirmFactoryOverwrite=true;
 const Response=await fetch("/api/v1/power/calibration",{
  method:"POST",headers:{"Content-Type":"application/json; charset=UTF-8"},body:JSON.stringify(Values)});
 if(!Response.ok||(await Response.text()).trim()!=="1")throw Error("Kalibrierwerte konnten nicht gespeichert werden.");
 PowerCalibrationValues=Values;
 SetPowerCalibrationValues();
 document.getElementById("PowerCalibrationStatus").textContent=FactoryWarning?"Werks-Kalibrierung überschrieben.":"Gespeichert.";}

function CalibrationMeterValue(Name){
 const Value=Number(PowerCalibrationMeter?.[Name]);
 if(!Number.isFinite(Value)||Value<=0)throw Error(`${Name} ist noch nicht gültig.`);
 return Value;}

async function CalibrateVoltage(){
 try{
  await LoadPowerCalibration();
  const Reference=PowerCalibrationNumber("CalVoltageRef");
  if(!Reference)throw Error("Referenzspannung fehlt.");
  const Measured=CalibrationMeterValue("Voltage");
  const Old=Number(PowerCalibrationValues.VoltageCal);
  await SavePowerCalibration({VoltageCal:Math.round(Old*Reference/Measured)});}
 catch(Error){document.getElementById("PowerCalibrationStatus").textContent=Error.message;}}

async function CalibrateCurrent(){
 try{
  await LoadPowerCalibration();
  const Reference=PowerCalibrationNumber("CalCurrentRef");
  if(!Reference)throw Error("Referenzstrom fehlt.");
  const Measured=CalibrationMeterValue("Current");
  const Old=Number(PowerCalibrationValues.CurrentCal);
  await SavePowerCalibration({CurrentCal:Math.round(Old*Reference/Measured)});}
 catch(Error){document.getElementById("PowerCalibrationStatus").textContent=Error.message;}}

async function CalibrateLoad(){
 try{
  await LoadPowerCalibration();
  const NominalPower=PowerCalibrationNumber("CalLoadPower");
  const NominalVoltage=PowerCalibrationNumber("CalLoadVoltage");
  if(!NominalPower||!NominalVoltage)throw Error("Referenzleistung/-spannung fehlt.");
  const Voltage=CalibrationMeterValue("Voltage");
  const Current=CalibrationMeterValue("Current");
  const Power=CalibrationMeterValue("ActivePower");
  const ExpectedPower=NominalPower*Math.pow(Voltage/NominalVoltage,2);
  const ExpectedCurrent=ExpectedPower/Voltage;
  const NewPower=Math.round(Number(PowerCalibrationValues.PowerCal)*ExpectedPower/Power);
  const NewCurrent=Math.round(Number(PowerCalibrationValues.CurrentCal)*ExpectedCurrent/Current);
  await SavePowerCalibration({PowerCal:NewPower,CurrentCal:NewCurrent});}
 catch(Error){document.getElementById("PowerCalibrationStatus").textContent=Error.message;}}

async function SaveDirectPowerCalibration(){
 try{
  const Voltage=PowerCalibrationNumber("CalDirectVoltage");
  const Current=PowerCalibrationNumber("CalDirectCurrent");
  const Power=PowerCalibrationNumber("CalDirectPower");
   if(!Voltage||!Current||!Power||![Voltage,Current,Power].every(Value=>Number.isSafeInteger(Value)&&Value<=0xFFFFFFFF))
    throw Error("Bitte drei positive ganzzahlige Kalibrierwerte bis 4294967295 eingeben.");
  await SavePowerCalibration({VoltageCal:Voltage,CurrentCal:Current,PowerCal:Power});}
 catch(Error){document.getElementById("PowerCalibrationStatus").textContent=Error.message;}}

//*****************************
//Setup
function SetupOnLoaded(){
 SetupSources();
 LoadTranslations();
 LoadTimes();
 LoadConfig();
 LoadVersions();
 LoadPowerMeterStatus();
 setInterval(LoadPowerMeterStatus,5000);
 MRestore();
 SetupDashboardIntegration();
 }

// Source mappings are editable only while the consumer cannot reach the Dashboard.
let SourcesDirty = false;
let SourcesLoading = false;
async function LoadSources() {
  if (SourcesLoading) return;
  SourcesLoading = true;
  const Text = document.getElementById('SourcesText');
  const Message = document.getElementById('SourcesMessage');
  const Save = document.getElementById('SourcesSaveBtn');
  try {
    const Response = await fetch('/api/v1/sources', {cache: 'no-store'});
    if (!Response.ok) throw new Error(`HTTP ${Response.status}`);
    const Data = await Response.json();
    const Editable = Data.Editable === true;
    Text.readOnly = !Editable;
    Save.disabled = !Editable;
    if (!Editable || !SourcesDirty) {
      Text.value = FormatEditorJson(Data.Sources) + '\n';
      SourcesDirty = false;
    }
    Message.textContent = Tr(Editable ? 'SourcesLocal' : 'SourcesLocked');
  } catch (Error) {
    Text.readOnly = true; Save.disabled = true;
    Message.textContent = Tr('SourcesError') + Error.message;
  } finally { SourcesLoading = false; }
}
async function SaveSources() {
  const Text = document.getElementById('SourcesText');
  const Message = document.getElementById('SourcesMessage');
  try {
    const Body = JSON.stringify(JSON.parse(Text.value));
    const Response = await fetch('/api/v1/sources', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: Body});
    if (!Response.ok) throw new Error(Response.status === 409 ? Tr('SourcesLocked') : `HTTP ${Response.status}`);
    SourcesDirty = false;
    await LoadSources();
  } catch (Error) { Message.textContent = Tr('SourcesError') + Error.message; }
}
function SetupSources() {
  const Text = document.getElementById('SourcesText');
  if (!Text) return;
  const Panel = document.getElementById('SourcesConfigPanel');
  const Toggle = document.getElementById('SourcesToggleBtn');
  Toggle.addEventListener('click', () => {
    Panel.hidden = !Panel.hidden;
    Toggle.setAttribute('aria-expanded', String(!Panel.hidden));
    if (!Panel.hidden) LoadSources();
    ScheduleViewportFit();
  });
  Text.addEventListener('input', () => { SourcesDirty = true; });
  document.getElementById('SourcesSaveBtn').addEventListener('click', SaveSources);
  document.getElementById('SourcesReloadBtn').addEventListener('click', () => { SourcesDirty = false; LoadSources(); });
  LoadSources();
  setInterval(LoadSources, 15000);
}
