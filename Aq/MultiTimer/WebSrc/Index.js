// Index.js, Version: 1.24
const ViewportFitMargin = 8;
const DashboardSettingTimeoutMilliseconds = 500;

let Channels = [];
let ChannelConfigs = [];
let AllocationSort = {Key: 'PhysPos', Direction: 1};
let Translations = {};
let ViewportFitTimer = null;
let ViewportMutationObserver = null;
let DashboardSettingTimer = null;
let CurrentPlatform = "";

function ReadLocalNumber(Name, DefaultValue) {
  try {
    const StoredValue = localStorage.getItem(Name);
    if (StoredValue === null) return DefaultValue;
    const Value = Number(StoredValue);
    return Number.isInteger(Value) ? Value : DefaultValue;
  } catch (Error) {
    return DefaultValue;
  }
}

function StoreLocalNumber(Name, Value) {
  try {
    localStorage.setItem(Name, String(Value));
  } catch (Error) {
    // Embedded third-party storage may be unavailable.
  }
}

function NotifyDashboard(Type, Values = {}) {
  if (window.parent === window) return;
  window.parent.postMessage({Source: 'GnConfigPage', Type, ...Values}, '*');
}

function ReadLocalLanguageIndex() {
  const CurrentValue = ReadLocalNumber('LNr', -1);
  if (CurrentValue >= 0) return CurrentValue;
  const LegacyValue = ReadLocalNumber('LanguageIndex', 0);
  StoreLocalNumber('LNr', LegacyValue);
  try {
    localStorage.removeItem('LanguageIndex');
  } catch (Error) {
    // Ignore unavailable local storage.
  }
  return LegacyValue;
}

function ApplySetting(Name, Value) {
  if (Name !== 'LNr') return;
  const Index = Number(Value);
  if (!Number.isInteger(Index)) return;
  if (DashboardSettingTimer !== null) {
    clearTimeout(DashboardSettingTimer);
    DashboardSettingTimer = null;
  }
  LanguageIndex = Index;
  if (Array.isArray(Translations.Lang)) SetLanguage(LanguageIndex, false);
}

function ReadLocalSetting(Name) {
  return Name === 'LNr' ? ReadLocalLanguageIndex() : null;
}

function StoreLocalSetting(Name, Value) {
  if (Name === 'LNr') StoreLocalNumber(Name, Value);
}

function GetSetting(Name) {
  if (window.parent === window) {
    ApplySetting(Name, ReadLocalSetting(Name));
    return;
  }
  if (DashboardSettingTimer !== null) clearTimeout(DashboardSettingTimer);
  DashboardSettingTimer = setTimeout(() => {
    DashboardSettingTimer = null;
    ApplySetting(Name, ReadLocalSetting(Name));
  }, DashboardSettingTimeoutMilliseconds);
  NotifyDashboard('GetSetting', {Name});
}

function SetSetting(Name, Value) {
  if (window.parent === window) {
    StoreLocalSetting(Name, Value);
    return;
  }
  if (DashboardSettingTimer !== null) clearTimeout(DashboardSettingTimer);
  DashboardSettingTimer = setTimeout(() => {
    DashboardSettingTimer = null;
    StoreLocalSetting(Name, Value);
  }, DashboardSettingTimeoutMilliseconds);
  NotifyDashboard('SetSetting', {Name, Value});
}

let LanguageIndex = 0;

function Tr(Key, DefaultText = Key) {
  const Values = Translations[Key];
  return Values && Values[LanguageIndex] !== undefined ? Values[LanguageIndex] : DefaultText;
}

function ApplyTranslations() {
  document.querySelectorAll('[data-tr]').forEach(Element => {
    Element.textContent = Tr(Element.dataset.tr, Element.textContent);
  });
  document.querySelectorAll('[data-tr-title]').forEach(Element => {
    const Text = Tr(Element.dataset.trTitle, Element.title);
    Element.title = Text;
    Element.setAttribute('aria-label', Text);
  });
}

function MSelect(Page) {
  const Names = ['SwitchTimes', 'Intervals', 'Configuration', 'Allocation'];
  const RequestedIndex = Number.isInteger(Page) ? Page : Names.indexOf(String(Page));
  const ValidIndex = RequestedIndex >= 0 && RequestedIndex < Names.length ? RequestedIndex : 0;
  const Name = Names[ValidIndex];
  document.querySelectorAll('.MenuSelectDiv').forEach(Element => Element.style.display = 'none');
  document.getElementById('PageSelector').value = Name;
  document.getElementById(Name).style.display = 'block';
  StoreLocalNumber('MenuIndex', ValidIndex);
  ScheduleViewportFit();
}

function SetLanguage(Index, NotifyParent = true) {
  const Languages = Translations.Lang || [];
  LanguageIndex = Index >= 0 && Index < Languages.length ? Index : 0;
  document.querySelectorAll('.MenuDdA').forEach(Element => Element.classList.remove('active'));
  const Language = Languages[LanguageIndex] || 'EN';
  const Entry = document.getElementById(`M${Language}`);
  if (Entry) Entry.classList.add('active');
  document.getElementById('MLang').textContent = Language;
  ApplyTranslations();
  ScheduleViewportFit();
  if (NotifyParent) SetSetting('LNr', LanguageIndex);
}

function SetupLanguages() {
  const Languages = Translations.Lang || ['EN'];
  document.getElementById('LanguageMenu').innerHTML = Languages.map((Name, Index) =>
    `<a class="MenuDdA" id="M${Name}" href="javascript:void(0)" onclick="SetLanguage(${Index})">${Name}</a>`
  ).join('');
  SetLanguage(LanguageIndex, false);
  GetSetting('LNr');
}

function SetUpdatedTime() {
  document.getElementById('UpdatedValue').textContent = new Date().toLocaleTimeString('de-DE', {
    hour: '2-digit',
    minute: '2-digit'
  });
}

function FitViewport() {
  ViewportFitTimer = null;
  document.body.style.zoom = '';

  const AvailableWidth = document.documentElement.clientWidth;
  if (AvailableWidth <= 0) return;

  const TargetWidth = Math.max(1, AvailableWidth - ViewportFitMargin);
  const Menu = document.querySelector('.TopBar');
  const VisiblePages = [...document.querySelectorAll('.MenuSelectDiv')].filter(
    Element => getComputedStyle(Element).display !== 'none'
  );
  let Scale = 1;

  for (let Iteration = 0; Iteration < 3; ++Iteration) {
    document.body.style.zoom = Scale < 0.999 ? String(Scale) : '';
    const ContentWidth = Math.max(
      Menu ? Menu.scrollWidth : 0,
      ...VisiblePages.map(Element => {
        const Table = Element.querySelector('table');
        return Table ? Table.scrollWidth : Element.scrollWidth;
      })
    );
    if (Scale === 1 && ContentWidth <= AvailableWidth) break;
    const VisualWidth = ContentWidth * Scale;
    if (VisualWidth <= TargetWidth + 0.5) break;
    Scale = Math.max(0.1, Scale * TargetWidth / VisualWidth);
  }

  document.body.style.zoom = Scale < 0.999 ? String(Scale) : '';
  NotifyDashboard('ViewportScale', {Scale});
}

function ScheduleViewportFit() {
  if (ViewportFitTimer !== null) clearTimeout(ViewportFitTimer);
  ViewportFitTimer = setTimeout(FitViewport, 0);
}

function HandleDashboardMessage(Event) {
  if (Event.source !== window.parent || Event.data?.Source !== 'GnDashboard') return;
  if (Event.data.Type === 'ConfigurationActivated') { CheckConfigurationRevision(); return; }
  if (Event.data.Type !== 'SettingValue') return;
  if (Event.data.Found === false) {
    const Value = ReadLocalSetting(Event.data.Name);
    ApplySetting(Event.data.Name, Value);
    SetSetting(Event.data.Name, Value);
    return;
  }
  ApplySetting(Event.data.Name, Event.data.Value);
}


function NewUuid() {
  if (crypto.randomUUID) return crypto.randomUUID();
  const Bytes = new Uint8Array(16);
  crypto.getRandomValues(Bytes);
  Bytes[6] = (Bytes[6] & 0x0f) | 0x40;
  Bytes[8] = (Bytes[8] & 0x3f) | 0x80;
  const Hex = [...Bytes].map(Value => Value.toString(16).padStart(2, '0'));
  return `${Hex.slice(0,4).join('')}-${Hex.slice(4,6).join('')}-${Hex.slice(6,8).join('')}-${Hex.slice(8,10).join('')}-${Hex.slice(10).join('')}`;
}

function RowUuid(Value, Index) {
  return typeof Value?.[Index] === 'string' && Value[Index] ? Value[Index] : NewUuid();
}

function NaturalCompare(First, Second) {
  return String(First ?? '').localeCompare(String(Second ?? ''), undefined, {numeric: true, sensitivity: 'base'});
}

function RenderAllocation() {
  const Body = document.getElementById('AllocationData');
  if (!Body) return;
  const Rows = ChannelConfigs.filter(Channel => typeof Channel?.PhysPos === 'string' && Channel.PhysPos.length > 0).map(Channel => ({
    Name: Channel.Id || '',
    PhysPos: Channel.PhysPos,
    GPIO: Number.isInteger(Channel.OutputPin) ? Channel.OutputPin : ''
  }));
  Rows.sort((First, Second) => {
    const Result = AllocationSort.Key === 'GPIO'
      ? (Number(First.GPIO) || 0) - (Number(Second.GPIO) || 0)
      : NaturalCompare(First[AllocationSort.Key], Second[AllocationSort.Key]);
    return Result * AllocationSort.Direction;
  });
  Body.replaceChildren(...Rows.map(Value => {
    const Row = document.createElement('tr');
    for (const Key of ['Name', 'PhysPos', 'GPIO']) {
      const Cell = document.createElement('td');
      Cell.textContent = Value[Key];
      Row.appendChild(Cell);
    }
    return Row;
  }));
}

function SortAllocation(Key) {
  if (AllocationSort.Key === Key) AllocationSort.Direction *= -1;
  else AllocationSort = {Key, Direction: 1};
  RenderAllocation();
}

async function GetJson(Path) {
  const Response = await fetch(Path);
  if (!Response.ok) throw new Error(`${Path}: HTTP ${Response.status}`);
  const Result = await Response.json();
  SetUpdatedTime();
  return Result;
}

const Checkbox = Value => `<input type="checkbox" ${Value ? 'checked' : ''}>`;

function ChannelSelect(Value) {
  return `<select class="ChannelSelect">${Channels.map(Name => `<option ${Name === Value ? 'selected' : ''}>${Name}</option>`).join('')}</select>`;
}

const Icon = Name => `<svg class="Icon" aria-hidden="true"><use href="#Icon${Name}"></use></svg>`;
const RowNumberCell = () => `<td class="RowNumber"><button type="button" class="DragHandle" draggable="true" data-tr-title="MoveRow">${Icon('Drag')}</button><span class="Nr"></span></td>`;
const DeleteButton = () => `<button type="button" class="Delete" data-tr-title="DeleteRow">${Icon('Trash')}</button>`;

function EnableDrag(Row, RenumberFunction) {
  const Handle = Row.querySelector('.DragHandle');
  Handle.addEventListener('dragstart', Event => {
    Row.classList.add('Dragging');
    Event.dataTransfer.effectAllowed = 'move';
    Event.dataTransfer.setData('text/plain', '');
  });
  Handle.addEventListener('dragend', () => Row.classList.remove('Dragging'));
  Row.addEventListener('dragover', Event => {
    Event.preventDefault();
    const Drag = document.querySelector('.Dragging');
    if (Drag && Drag !== Row && Drag.parentNode === Row.parentNode) {
      Row.parentNode.insertBefore(Drag, Event.clientY < Row.getBoundingClientRect().top + Row.offsetHeight / 2 ? Row : Row.nextSibling);
      RenumberFunction();
    }
  });
}

function Renumber(BodyId) {
  document.querySelectorAll(`#${BodyId} .Nr`).forEach((Cell, Index) => Cell.textContent = Index + 1);
}

function AddSwitchRow(Value = ['00:00', 255, Channels[0] || '', false]) {
  const Body = document.getElementById('SwitchData');
  const Row = Body.insertRow();
  Row.dataset.id = RowUuid(Value, 4);
  Row.innerHTML = `${RowNumberCell()}<td>${Checkbox(Value[1] & 128)}</td><td>${ChannelSelect(Value[2])}</td><td><input type="time" value="${Value[0]}"></td>${[0,1,2,3,4,5,6].map(Bit => `<td>${Checkbox(Value[1] & (1 << Bit))}</td>`).join('')}<td>${Checkbox(Value[3])}</td><td>${DeleteButton()}</td>`;
  Row.querySelector('.Delete').onclick = () => { Row.remove(); Renumber('SwitchData'); };
  EnableDrag(Row, () => Renumber('SwitchData'));
  ApplyTranslations();
  Renumber('SwitchData');
}

function AddIntervalRow(Value = ['00:00', '01:00', '00:15', 255, Channels[0] || '']) {
  const Body = document.getElementById('IntervalData');
  const Row = Body.insertRow();
  Row.dataset.id = RowUuid(Value, 5);
  Row.innerHTML = `${RowNumberCell()}<td>${Checkbox(Value[3] & 128)}</td><td>${ChannelSelect(Value[4])}</td><td><input type="time" value="${Value[0]}"></td><td><input type="time" value="${Value[1]}"></td><td><input type="time" value="${Value[2]}"></td>${[0,1,2,3,4,5,6].map(Bit => `<td>${Checkbox(Value[3] & (1 << Bit))}</td>`).join('')}<td>${DeleteButton()}</td>`;
  Row.querySelector('.Delete').onclick = () => { Row.remove(); Renumber('IntervalData'); };
  EnableDrag(Row, () => Renumber('IntervalData'));
  ApplyTranslations();
  Renumber('IntervalData');
}

function TimeToMinutes(Value) {
  if (!/^\d{2}:\d{2}$/.test(Value)) return -1;
  const [Hours, Minutes] = Value.split(':').map(Number);
  if (Minutes > 59) return -1;
  return Hours * 60 + Minutes;
}

function FindActiveTimeConflict(Data, TimeIndex, MaskIndex, ChannelIndex) {
  for (let First = 0; First < Data.length; ++First) {
    if (!(Data[First][MaskIndex] & 128)) continue;
    for (let Second = First + 1; Second < Data.length; ++Second) {
      if (!(Data[Second][MaskIndex] & 128)) continue;
      if (Data[First][TimeIndex] === Data[Second][TimeIndex] &&
          Data[First][ChannelIndex] === Data[Second][ChannelIndex] &&
          (Data[First][MaskIndex] & Data[Second][MaskIndex] & 0x7f)) {
        return [First, Second];
      }
    }
  }
  return null;
}

function ReportSaveResult(Result) {
  const Value = Result.trim();
  if (Value !== '1') {
    alert(`${Tr('SaveFailed', 'Save failed')}: ${Tr(`Error${Value}`, Value)}`);
    return;
  }
  SetUpdatedTime();
  alert(Tr('Saved', 'Saved'));
}

async function LoadChannels() {
  const Response = await fetch('/Channels.json', {cache: 'no-store'});
  if (!Response.ok) throw new Error(`/Channels.json: HTTP ${Response.status}`);
  const Text = await Response.text();
  const Config = JSON.parse(Text);
  if (!Array.isArray(Config.Channels)) throw new Error('Channels.json: Channels is not an array');
  ChannelConfigs = Config.Channels;
  Channels = ChannelConfigs.map(Channel => Channel?.Id).filter(Id => typeof Id === 'string' && Id.length > 0);
  RenderAllocation();
  document.getElementById('ChannelsText').value = FormatEditorJson(Config) + '\n';
  SetUpdatedTime();
}

async function RestartMultiTimer(ShowMessage = true) {
  const RestartButton = document.getElementById('RestartMultiTimer');
  if (RestartButton.disabled) return false;
  RestartButton.disabled = true;
  try {
    const Response = await fetch('/Restart/', {method: 'POST'});
    const Result = (await Response.text()).trim();
    if (!Response.ok || Result !== '1') {
      alert(`${Tr('RestartFailed', 'Restart failed')}: ${Result || `HTTP ${Response.status}`}`);
      RestartButton.disabled = false;
      return false;
    }
    if (ShowMessage) alert(Tr('RestartingMultiTimer', 'MultiTimer is restarting...'));
    return true;
  } catch (Error) {
    alert(`${Tr('RestartFailed', 'Restart failed')}: ${Error.message}`);
    RestartButton.disabled = false;
    return false;
  }
}

async function SaveChannels() {
  const SaveButton = document.getElementById('SaveChannels');
  if (SaveButton.disabled) return;
  let Text = document.getElementById('ChannelsText').value;
  try {
    Text = FormatEditorJson(JSON.parse(Text)) + '\n';
  } catch (Error) {
    alert(`${Tr('SaveFailed', 'Save failed')}: ${Tr('InvalidJson', 'invalid JSON')}\n${Error.message}`);
    return;
  }
  document.getElementById('ChannelsText').value = Text;
  SaveButton.disabled = true;
  try {
    const Response = await fetch('/SetChannels/', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: Text});
    const Result = (await Response.text()).trim();
    if (!Response.ok || Result !== '1') {
      alert(`${Tr('SaveFailed', 'Save failed')}: ${Result || `HTTP ${Response.status}`}`);
      return;
    }
    SetUpdatedTime();
    if (CurrentPlatform === 'RasPi') {
      if (await RestartMultiTimer(false)) {
        alert(`${Tr('Saved', 'Saved')}\n${Tr('RestartingMultiTimer', 'MultiTimer is restarting...')}`);
      }
    } else {
      alert(`${Tr('Saved', 'Saved')}\n${Tr('RestartRequired', 'Restart required to apply Channels.json')}`);
    }
  } finally {
    SaveButton.disabled = false;
  }
}

async function LoadSwitchTimes() {
  document.getElementById('SwitchData').innerHTML = '';
  (await GetJson('/SwitchTimes.json')).forEach(AddSwitchRow);
}

async function LoadIntervals() {
  document.getElementById('IntervalData').innerHTML = '';
  (await GetJson('/Intervals.json')).forEach(AddIntervalRow);
}

async function PostSaveRequest(ButtonId, Uri, Data) {
  const SaveButton = document.getElementById(ButtonId);
  if (SaveButton.disabled) return;
  SaveButton.disabled = true;
  try {
    const Response = await fetch(Uri, {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify(Data)});
    ReportSaveResult(await Response.text());
  } finally {
    SaveButton.disabled = false;
  }
}

async function SaveSwitchTimes() {
  const Data = [...document.getElementById('SwitchData').rows].map(Row => {
    let Mask = Row.cells[1].firstChild.checked ? 128 : 0;
    for (let Bit = 0; Bit < 7; ++Bit) if (Row.cells[4 + Bit].firstChild.checked) Mask |= 1 << Bit;
    return [Row.cells[3].firstChild.value, Mask, Row.cells[2].firstChild.value, Row.cells[11].firstChild.checked, Row.dataset.id || NewUuid()];
  });
  const Conflict = FindActiveTimeConflict(Data, 0, 1, 2);
  if (Conflict) {
    alert(`${Tr('DuplicateActiveTimeRows', 'Active rows with the same time, channel and overlapping weekdays:')} ${Conflict[0] + 1} / ${Conflict[1] + 1}`);
    return;
  }
  await PostSaveRequest('SaveSwitch', '/SetSwitchTimes/', Data);
}

async function SaveIntervals() {
  const Rows = [...document.getElementById('IntervalData').rows];
  const Data = [];
  for (let Index = 0; Index < Rows.length; ++Index) {
    const Row = Rows[Index];
    const Start = Row.cells[3].firstChild.value;
    const Period = Row.cells[4].firstChild.value;
    const OnTime = Row.cells[5].firstChild.value;
    const StartMinutes = TimeToMinutes(Start);
    const PeriodMinutes = TimeToMinutes(Period);
    const OnMinutes = TimeToMinutes(OnTime);
    if (StartMinutes < 0 || StartMinutes >= 1440) {
      alert(`${Tr('IntervalRow', 'Interval row')} ${Index + 1}: ${Tr('InvalidStartTime', 'invalid start time')}`);
      return;
    }
    if (PeriodMinutes <= 0) {
      alert(`${Tr('IntervalRow', 'Interval row')} ${Index + 1}: ${Tr('PeriodGreaterZero', 'period must be greater than 00:00')}`);
      return;
    }
    if (OnMinutes <= 0) {
      alert(`${Tr('IntervalRow', 'Interval row')} ${Index + 1}: ${Tr('OnTimeGreaterZero', 'on-time must be greater than 00:00')}`);
      return;
    }
    if (OnMinutes >= PeriodMinutes) {
      alert(`${Tr('IntervalRow', 'Interval row')} ${Index + 1}: ${Tr('OnTimeShorterPeriod', 'on-time must be shorter than period')}`);
      return;
    }
    let Mask = Row.cells[1].firstChild.checked ? 128 : 0;
    for (let Bit = 0; Bit < 7; ++Bit) if (Row.cells[6 + Bit].firstChild.checked) Mask |= 1 << Bit;
    Data.push([Start, Period, OnTime, Mask, Row.cells[2].firstChild.value, Row.dataset.id || NewUuid()]);
  }
  const Conflict = FindActiveTimeConflict(Data, 0, 3, 4);
  if (Conflict) {
    alert(`${Tr('DuplicateActiveTimeRows', 'Active rows with the same time, channel and overlapping weekdays:')} ${Conflict[0] + 1} / ${Conflict[1] + 1}`);
    return;
  }
  await PostSaveRequest('SaveInterval', '/SetIntervals/', Data);
}

async function LoadVersionInfo() {
  let FirmwareVersion = '?';
  let Platform = '';
  let WebVersion = '?';

  try {
    const Device = await GetJson('/api/v1/device');
    FirmwareVersion = Device.FirmwareVersion || '?';
    Platform = Device.Platform || '';
    CurrentPlatform = Platform;
    document.getElementById('RestartMultiTimer').hidden = Platform !== 'RasPi';
    document.getElementById('DeviceName').textContent = Device.Name || Device.DeviceId || '?';
    document.title = Device.Name || Device.DeviceId || document.title;
  } catch (Error) {
    console.warn('Could not load firmware version', Error);
  }

  try {
    const Text = await (await fetch('/Versions.txt')).text();
    const Match = Text.match(/^Web=(.+)$/m);
    if (Match) WebVersion = Match[1].trim();
  } catch (Error) {
    console.warn('Could not load web version', Error);
  }

  const IsEsp = Platform === 'ESP';
  document.getElementById('VersionSummary').textContent = `${IsEsp ? 'FW' : 'App'} ${FirmwareVersion} · ${IsEsp ? 'FS' : 'Web'} ${WebVersion}`;
  SetUpdatedTime();
}

async function Load() {
  Translations = await GetJson('/Translations.json');
  SetupLanguages();
  SetupSources();
  await LoadChannels();
  await Promise.all([LoadSwitchTimes(), LoadIntervals(), LoadVersionInfo()]);
  await RememberConfigurationRevision();
  MSelect(ReadLocalNumber('MenuIndex', 0));
}

window.addEventListener('message', HandleDashboardMessage);
window.addEventListener('resize', ScheduleViewportFit);
ViewportMutationObserver = new MutationObserver(ScheduleViewportFit);
ViewportMutationObserver.observe(document.getElementById('MainDiv'), {
  childList: true,
  subtree: true,
  characterData: true
});

document.getElementById('AddSwitch').onclick = () => AddSwitchRow();
document.getElementById('SaveSwitch').onclick = SaveSwitchTimes;
document.getElementById('ResetSwitch').onclick = LoadSwitchTimes;
document.getElementById('AddInterval').onclick = () => AddIntervalRow();
document.getElementById('SaveInterval').onclick = SaveIntervals;
document.getElementById('ResetInterval').onclick = LoadIntervals;
document.getElementById('SaveChannels').onclick = SaveChannels;
document.getElementById('ResetChannels').onclick = LoadChannels;
document.getElementById('RestartMultiTimer').onclick = () => RestartMultiTimer(true);
document.querySelectorAll('[data-allocation-sort]').forEach(Button => Button.onclick = () => SortAllocation(Button.dataset.allocationSort));
Load();

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

let ConfigurationRevision = null;
let ConfigurationSnapshot = null;
let ConfigurationCheck = null;
let ConfigurationLastCheck = 0;

function ConfigurationFormState() {
  return JSON.stringify(['SwitchData', 'IntervalData', 'ChannelsText'].map(Id => {
    const Element = document.getElementById(Id);
    return Element ? [Element.innerHTML, ...[...Element.querySelectorAll('input,select,textarea')].map(Input => [Input.value, Input.checked]), Element.value || ''] : [];
  }));
}

async function RememberConfigurationRevision() {
  try {
    const Response = await fetch('/api/v1/config-revision', {cache: 'no-store'});
    if (Response.ok) ConfigurationRevision = (await Response.json()).Revision;
    ConfigurationSnapshot = ConfigurationFormState();
  } catch (Error) { console.warn('Configuration revision unavailable', Error); }
}

async function CheckConfigurationRevision() {
  if (ConfigurationRevision === null || document.hidden || ConfigurationCheck || Date.now() - ConfigurationLastCheck < 250) return;
  ConfigurationLastCheck = Date.now();
  ConfigurationCheck = (async () => {
    try {
      const Response = await fetch('/api/v1/config-revision', {cache: 'no-store'});
      if (!Response.ok) return;
      const Revision = (await Response.json()).Revision;
      if (Revision === ConfigurationRevision) return;
      if (ConfigurationSnapshot !== ConfigurationFormState()) {
        let Notice = document.getElementById('ConfigurationChangeNotice');
        if (!Notice) { Notice = document.createElement('p'); Notice.id = 'ConfigurationChangeNotice'; document.getElementById('MainDiv').prepend(Notice); }
        Notice.textContent = Tr('ExternalConfigurationChange', 'Configuration changed in Dashboard. Unsaved edits are retained; reload after saving or discarding them.');
        return;
      }
      await LoadChannels();
      await Promise.all([LoadSwitchTimes(), LoadIntervals()]);
      ConfigurationRevision = Revision;
      ConfigurationSnapshot = ConfigurationFormState();
      document.getElementById('ConfigurationChangeNotice')?.remove();
    } catch (Error) { console.warn('Configuration refresh failed', Error); }
  })();
  try { await ConfigurationCheck; } finally { ConfigurationCheck = null; }
}
window.addEventListener('focus', CheckConfigurationRevision);
document.addEventListener('visibilitychange', () => { if (!document.hidden) CheckConfigurationRevision(); });
