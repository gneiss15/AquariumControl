// Dashboard.js, Version: 2.15
import {TDeviceStatusClient} from './StatusClient.js';
import {TFeedbackAlarm} from './FeedbackAlarm.js';


// Use browser-local time; clock ticks never request device or server data.
function UpdateBrowserClock() {
  const Clock = document.getElementById('BrowserClock');
  if (!Clock) return;
  const German = document.getElementById('MLang')?.textContent.trim() === 'DE';
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
let LastDashboardUpdateAt = null;
const OfflineCheckMilliseconds = 1000;
const DashboardHealthNormalMilliseconds = 15000;
const DashboardHealthInitialMilliseconds = 2000;
const DashboardHealthMaximumMilliseconds = 30000;
const DashboardRequestTimeoutMilliseconds = 6000;
const DashboardRecoveryReloadMilliseconds = 750;
const SyncNoticeHideMilliseconds = 4000;
const SyncNoticePollMilliseconds = 2500;
const DeviceActionTimeoutMilliseconds = 6000;
const LongPressConfirmMilliseconds = 4000;
const DefaultFrameWidth = 1100;
const DefaultMinimumScale = 0.72;
const ConfigurationLanguageName = 'LNr';
const DashboardLanguageStorageKey = 'AquariumDashboard.LNr';
const DashboardTabCookieName = 'MNr';
const FeedbackSoundCookieName = 'AquariumDashboard.FeedbackSound';
const ServerVersionApiPath = 'api/version';
const DeviceGroupOrder = new Map([
  ['Aq1', 1],
  ['Aq2', 2],
  ['Aq3', 3],
  ['System', 4]
]);
const Devices = [];
const DeviceGroups = new Map();
const SyncNoticeStates = new WeakMap();

let Translations = {};
let LangCnt = 0;
let LangNr = 1;

let DashboardHealthTimer = null;
let DashboardHealthDelay = DashboardHealthInitialMilliseconds;
let DashboardHealthRequestRunning = false;
let DashboardWasUnavailable = false;
let ActiveConfigDevice = null;
let CentralTimeStore = {Ver: 1, Sets: [], Times: []};
let CentralHeatProfiles = {Profs: []};
let CentralHeatProfilesSaved = {Profs: [], TimeStamp: ''};
let CentralHeatProfileStatus = {Sets: []};
let CentralChannelStore = {Chs: {}};
let CentralProfileAssignmentView = {Devices: []};
let CentralChannelDrafts = new Map();
let CentralTimeSortKey = null;
let CentralTimesDirty = false;
let OverviewOutputOrder = {};
let OverviewFitPending = false;
let SyncStatusRefreshTimer = null;
let AssignmentView = null;
let AssignmentTimer = null;
let AssignmentLoading = false;

class TDashboardDevice {
  constructor(Configuration) {
    this.Cfg = Configuration;
    this.TechnicalName = Configuration.Func || Configuration.Name || new URL(Configuration.BaseUrl, window.location.href).hostname;
    this.BaseUrl = new URL(Configuration.BaseUrl, window.location.href);
    this.CfgUrl = Configuration.CfgUrl
      ? new URL(Configuration.CfgUrl, this.BaseUrl)
      : null;
    this.Views = (Array.isArray(Configuration.GroupViews) && Configuration.GroupViews.length > 0
      ? Configuration.GroupViews
      : [{Group: Configuration.Group || 'Other'}])
      .map(ViewConfiguration => new TDashboardDeviceView(this, ViewConfiguration));
    this.Client = null;
    this.DeviceInfo = null;
    this.Status = null;
    this.LastStatusAt = null;
    this.UdpConnected = false;
    this.RetryTimer = null;
    this.ConfigTabId = `Config-${SanitizeId(this.TechnicalName || this.BaseUrl.host)}`;
    this.ConfigSelectorOption = null;
    this.ConfigPage = null;
    this.ConfigViewport = null;
    this.ConfigCanvas = null;
    this.ConfigFrame = null;
    this.ConfigResizeObserver = null;
    this.ConfigScale = 1;
    for (const View of this.Views)
      View.Card = CreateDeviceCard(View);
    if (Configuration.StatusOnly !== true && Configuration.Active !== false)
      CreateConfigurationTab(this);
  }

  get DisplayName() { return DeviceDisplayName(this.TechnicalName, this.Cfg); }

  async Start() {
    this.StopClient();
    SetConnectionState(this, 'Offline');
    if (this.Cfg.Active === false) return;
    try {
      this.Client = new TDeviceStatusClient(
        this.BaseUrl,
        (Status, Sequence) => this.HandleStatus(Status, Sequence),
        Connected => this.HandleConnectionChanged(Connected),
        this.Cfg.ExternalSourceId || null
      );
      await this.Client.Start();
      this.DeviceInfo = this.Client.Dev;
      UpdateDeviceInfo(this);
      this.ScheduleRetry(false);
    } catch (Error) {
      console.error(`Cannot initialize ${this.BaseUrl}`, Error);
      this.StopClient();
      ShowDeviceOffline(this);
      this.ScheduleRetry(true);
    }
  }

  StopClient() {
    if (this.Client !== null) {
      this.Client.Stop();
      this.Client = null;
    }
    this.UdpConnected = false;
  }

  ScheduleRetry(Enable) {
    if (this.RetryTimer !== null) {
      clearTimeout(this.RetryTimer);
      this.RetryTimer = null;
    }
    if (!Enable) return;
    this.RetryTimer = setTimeout(() => {
      this.RetryTimer = null;
      this.Start();
    }, 10000);
  }

  HandleStatus(Status, Sequence) {
    this.Status = Status;
    EnsureRoomTemperatureView(this, Status);
    if (Array.isArray(Status.FeedbackMonitors)) FeedbackAlarm.Update(Status.FeedbackMonitors);
    this.LastStatusAt = new Date();
    for (const View of this.Views)
      UpdateDeviceStatus(View, Sequence);
    UpdateDashboardErrors();
    UpdateDashboardTimestamp();
  }

  HandleConnectionChanged(Connected) {
    this.UdpConnected = Connected;
    SetConnectionState(this, Connected ? 'Live' : 'Offline');
    if (!Connected && this.Cfg.ExternalSourceId) ShowDeviceOffline(this);
    UpdateDashboardErrors();
  }
}

class TDashboardDeviceView {
  constructor(Device, Configuration) {
    this.Owner = Device;
    this.Cfg = Device.Cfg;
    this.Group = Configuration.Group || Device.Cfg.Group || 'Other';
    this.MeasurementIds = Array.isArray(Configuration.Meas) ? new Set(Configuration.Meas) : null;
    this.StatusFunction = Configuration.StatusFunction || null;
    this.TemperatureOnly = Configuration.TemperatureOnly === true;
    this.RoomTemperatureOnly = Configuration.RoomTemperatureOnly === true;
    this.ChannelIds = Array.isArray(Configuration.Chs)
      ? new Set(Configuration.Chs.map(String))
      : null;
    this.Card = null;
    this.StatusRow = null;
    this.MeasurementRows = [];
    this.ChannelRows = [];
  }

  get BaseUrl() { return this.Owner.BaseUrl; }
  get Client() { return this.Owner.Client; }
  get ConfigUrl() { return this.Owner.CfgUrl; }
  get DeviceInfo() { return this.Owner.DeviceInfo; }
  get DisplayName() { if (this.RoomTemperatureOnly) return Tr('RoomTemperature', 'Room temperature'); return this.StatusFunction ? `${this.TemperatureOnly ? Tr('TemperatureSource', 'Temperature source') + ' · ' : ''}${DeviceDisplayName(this.StatusFunction)}` : this.Owner.DisplayName; }
  get LastStatusAt() { return this.Owner.LastStatusAt; }
  get Status() { return this.Owner.Status; }
  get TechnicalName() { return this.Owner.TechnicalName; }
  get UdpConnected() { return this.Owner.UdpConnected; }
}

async function LoadTranslations() {
  const Url = new URL('Translations.json', window.location.href);
  Url.searchParams.set('_', Date.now().toString());
  const Response = await fetch(Url, {cache: 'no-store'});
  if (!Response.ok)
    throw new Error(`Translations: HTTP ${Response.status}`);
  Translations = await Response.json();
  LangCnt = Array.isArray(Translations.Lang) ? Translations.Lang.length : 0;
  if (LangCnt === 0)
    throw new Error('Translations: Lang missing/invalid');

  const Menu = document.getElementById('LanguageMenu');
  Menu.replaceChildren(...Translations.Lang.map((Language, Index) => {
    const Entry = CreateElement('a', 'MenuDdA', Language);
    Entry.id = `M${Language}`;
    Entry.tabIndex = 0;
    Entry.addEventListener('click', () => LSelect(Index));
    Entry.addEventListener('keydown', Event => {
      if (Event.key === 'Enter' || Event.key === ' ') {
        Event.preventDefault();
        LSelect(Index);
      }
    });
    return Entry;
  }));
}

function Tr(Name, DefaultText = undefined) {
  const Values = Translations[Name];
  if (!Array.isArray(Values))
    return DefaultText === undefined ? Name : DefaultText;
  return Values[LangNr] ?? Values[0] ?? (DefaultText === undefined ? Name : DefaultText);
}

function TrAll() {
  for (const Element of document.querySelectorAll('[data-tr]'))
    Element.textContent = Tr(Element.dataset.tr, Element.textContent);
  for (const Element of document.querySelectorAll('[data-tr-aria]'))
    Element.setAttribute('aria-label', Tr(Element.dataset.trAria, Element.getAttribute('aria-label')));
  for (const Element of document.querySelectorAll('[data-tr-title]')) {
    const Text = Tr(Element.dataset.trTitle, Element.title);
    Element.title = Text;
    Element.setAttribute('aria-label', Text);
  }
  document.documentElement.lang = LangNr === 1 ? 'de' : 'en';
  document.title = Tr('DashboardTitle', 'Aquarium Dashboard');
}

function DeviceDisplayName(Name, Configuration = null) {
  const Config = Configuration || Devices.find(Device => Device.TechnicalName === Name)?.Cfg;
  if (Config?.UseHwName && Config.HwName) return Config.HwName;
  if (Config?.EndpointFunction) return Tr(`Channel.${Config.EndpointFunction}`, Tr(`Device.${Config.EndpointFunction}`, Config.EndpointFunction));
  if (Name === 'GN-Pi') return Tr('Device.MultiTimer Aq1 & 2', 'Zeitschaltuhr Aq1 & Aq2');
  return Tr(`Device.${Name}`, Name);
}

function ChannelDisplayName(Channel, Device = null) {
  const RawName = Channel?.Name || Channel?.Id;
  const DeviceName = typeof Device === 'string' ? Device : Device?.TechnicalName;
  const Configuration = typeof Device === 'string'
    ? Devices.find(Item => Item.TechnicalName === Device)?.Cfg : Device?.Cfg;
  if (Configuration?.EndpointHardwareLabels?.[RawName]) return Configuration.EndpointHardwareLabels[RawName];
  if (Configuration?.UseHwName && Configuration.HwName) return Configuration.HwName;
  const DeviceKey = `Channel.${DeviceName}.${RawName}`;
  if (DeviceName && Array.isArray(Translations[DeviceKey]))
    return Tr(DeviceKey);
  if (RawName !== undefined && RawName !== null && String(RawName).length > 0)
    return Tr(`Channel.${RawName}`, String(RawName));
  return `${Tr('Channel', 'Channel')} ${Channel?.Id ?? ''}`.trim();
}

// Central editors need aquarium context; overview cards already have group headings.
function EditorDeviceLabel(Name, Configuration = null) {
  const Config = Configuration || Devices.find(Device => Device.TechnicalName === Name)?.Cfg;
  const Groups = [...new Set((Config?.GroupViews || [{Group: Config?.Group}]).map(View => View.Group).filter(Boolean))];
  return AssignmentFunctionLabel(Name, Groups, DeviceDisplayName(Name, Config));
}

function EditorChannelLabel(Channel, Device) {
  const Function = Channel?.Name || Channel?.Id || '';
  const Endpoint = AssignmentView?.Endpoints?.find(Item => Item.Func === Function);
  const Config = Devices.find(Item => Item.TechnicalName === Device)?.Cfg;
  const Views = Config?.GroupViews || [{Group: Config?.Group}];
  const Groups = Endpoint?.Groups || Views.filter(View => !View.Chs || View.Chs.includes(Function)).map(View => View.Group).filter(Boolean);
  const Aquarium = String(Function).match(/Aq\d+/g) || [];
  return AssignmentFunctionLabel(Function, Aquarium.length ? Aquarium : Groups, ChannelDisplayName(Channel, Device));
}

class TEndpointDirectory {
  Entries() {
    const View = CentralProfileAssignmentView || {};
    const Entries = [];
    for (const Endpoint of View.Endpoints || []) {
      if (!Endpoint.OutAddress) continue;
      Entries.push({
        Id: Endpoint.OutAddress,
        Name: AssignmentFunctionLabel(Endpoint.Func, Endpoint.Groups || [], Endpoint.UseHwName ? Endpoint.HwName : null),
        Aliases: [Endpoint.Func, Endpoint.HwName].filter(Boolean)
      });
    }
    for (const Device of View.Devices || []) {
      if (!Device.DevId) continue;
      const Sensor = (View.Endpoints || []).find(Endpoint =>
        Endpoint.OutAddress?.split('/')[0] === Device.DevId && Number(Endpoint.Caps) === 2);
      const Function = Sensor?.Func || Device.Func || Device.HwName || Device.DevId;
      Entries.push({
        Id: Device.DevId,
        Name: AssignmentFunctionLabel(Function, Sensor?.Groups || [], Sensor?.UseHwName ? Sensor.HwName : null),
        Aliases: [Device.Func, Device.HwName, Function].filter(Boolean)
      });
    }
    for (const Device of Devices) {
      if (!Device.Cfg?.ExternalSourceId) continue;
      Entries.push({Id: Device.Cfg.ExternalSourceId, Name: EditorDeviceLabel(Device.TechnicalName),
        Aliases: [Device.TechnicalName]});
    }
    return Entries;
  }

  IdToName(Id) {
    return this.Entries().find(Entry => Entry.Id === Id)?.Name || '';
  }

  IdToLabel(Id, Detail = '', FallbackName = '') {
    if (!Id) return '';
    const Name = this.IdToName(Id) || FallbackName;
    return Name ? `${Name}${Detail ? ` · ${Detail}` : ''} (${Id})` : Id;
  }

  NameToId(Value) {
    const Text = String(Value || '').trim();
    if (!Text) return '';
    const Entries = this.Entries();
    const IdMatch = /\s+\(([^()]+)\)$/.exec(Text);
    if (IdMatch && Entries.some(Entry => Entry.Id === IdMatch[1])) return IdMatch[1];
    const Matches = [...new Set(Entries.filter(Entry => Entry.Name === Text || Entry.Aliases.includes(Text)).map(Entry => Entry.Id))];
    return Matches.length === 1 ? Matches[0] : '';
  }
}

const EndpointDirectory = new TEndpointDirectory();

function RefreshTranslatedContent() {
  TrAll();
  for (const Device of Devices) {
    if (Device.ConfigSelectorOption !== null)
      Device.ConfigSelectorOption.textContent = EditorDeviceLabel(Device.TechnicalName);
    for (const View of Device.Views) {
      View.StatusRow.querySelector('.DeviceName').textContent = View.DisplayName;
      SetConnectionState(View, View.StatusRow.querySelector('.ConnectionBadge').classList.contains('Live')
        ? 'Live'
        : View.StatusRow.querySelector('.ConnectionBadge').classList.contains('Polling') ? 'Polling' : 'Offline');
      if (Device.Status !== null)
        UpdateDeviceStatus(View);
    }
  }
  for (const [GroupId, Group] of DeviceGroups)
    Group.Title.textContent = DeviceGroupDisplayName(GroupId);
  UpdateDashboardErrors();
  if (CentralTimeStore.Sets.length > 0 || CentralTimeStore.Times.length > 0) {
    CentralTimeStore.Times = CollectCentralTimes();
    RenderCentralTimes();
  }
  if (document.querySelector('#HeatProfilesEditor .HeatProfileCard')) {
    for (const Source of document.querySelectorAll('#HeatProfilesEditor select[data-profile-source]'))
      Source.UpdateOptions?.(true);
    RenderChannelProfiles(true);
    if (CentralHeatProfileStatus) RenderHeatProfileStatus(CentralHeatProfileStatus);
  }
  if (document.getElementById('Assignments').classList.contains('Active')) LoadAssignments();
  RenderDashboardTimestamp();
  if (!document.getElementById('DetailOverlay').hidden)
    CloseChannelDetails();
}

function LSelect(Nr) {
  if (!Number.isInteger(Nr) || Nr < 0 || Nr >= LangCnt)
    Nr = 0;
  LangNr = Nr;
  const Language = Translations.Lang[Nr];
  for (const Element of document.getElementsByClassName('MenuDdA'))
    Element.classList.remove('active');
  document.getElementById(`M${Language}`)?.classList.add('active');
  document.getElementById('MLang').textContent = Language;
  if (document.activeElement instanceof HTMLElement)
    document.activeElement.blur();
  try {
    localStorage.setItem(DashboardLanguageStorageKey, String(Nr));
  } catch (Error) {
    console.warn('Could not store dashboard language', Error);
  }
  RefreshTranslatedContent();
}

function LRestore() {
  let Stored = null;
  try {
    const Value = Number(localStorage.getItem(DashboardLanguageStorageKey));
    if (Number.isInteger(Value) && Value >= 0 && Value < LangCnt)
      Stored = Value;
  } catch (Error) {
    Stored = null;
  }
  LSelect(Stored === null ? 1 : Stored);
}

function CreateElement(TagName, ClassName = '', Text = '') {
  const Element = document.createElement(TagName);
  if (ClassName)
    Element.className = ClassName;
  if (Text)
    Element.textContent = Text;
  return Element;
}

function GetDeviceGroup(GroupId) {
  if (DeviceGroups.has(GroupId))
    return DeviceGroups.get(GroupId);

  const Section = CreateElement('section', 'AquariumGroup');
  Section.dataset.group = GroupId;
  Section.style.order = String(DeviceGroupOrder.get(GroupId) || 99);
  const Title = CreateElement('h2', 'AquariumGroupTitle', DeviceGroupDisplayName(GroupId));
  const StatusList = CreateElement('div', 'GroupStatusList');
  const Divider = CreateElement('hr', 'GroupStatusDivider');
  const Outputs = CreateElement('div', 'GroupOutputList');
  Section.append(Title, StatusList, Divider, Outputs);
  document.getElementById('DeviceGrid').appendChild(Section);
  const Group = {Section, Title, StatusList, Outputs, Views: []};
  DeviceGroups.set(GroupId, Group);
  return Group;
}

function DeviceGroupDisplayName(GroupId) {
  return Tr(`Group.${GroupId}`, GroupId);
}

function CreateDeviceCard(Device) {
  const Template = document.getElementById('DeviceCardTemplate');
  const Card = Template.content.firstElementChild.cloneNode(true);
  Card.querySelector('.DeviceName').textContent = Device.DisplayName;
  const Header = Card.querySelector('.DeviceHeader');
  Device.StatusRow = Header;
  const Group = GetDeviceGroup(Device.Group);
  Group.Views.push(Device);
  Group.StatusList.appendChild(Header);
  if (Device.Cfg.StatusOnly === true) {
    Card.querySelector('.ChannelList').hidden = true;
  }
  Card.classList.add('GroupOutputCard');
  return Card;
}

function ReadDashboardTabCookie() {
  try {
    const Prefix = `${DashboardTabCookieName}=`;
    for (const Entry of decodeURIComponent(document.cookie).split(';')) {
      const Cookie = Entry.trim();
      if (Cookie.startsWith(Prefix))
        return Cookie.slice(Prefix.length);
    }
  } catch (Error) {
    console.warn('Cannot read dashboard tab cookie', Error);
  }
  return '';
}

function StoreDashboardTabCookie(TabId) {
  try {
    document.cookie = `${DashboardTabCookieName}=${encodeURIComponent(TabId)}; SameSite=Lax`;
  } catch (Error) {
    console.warn('Cannot store dashboard tab cookie', Error);
  }
}

function ScheduleOverviewFit() {
  if (OverviewFitPending) return;
  OverviewFitPending = true;
  requestAnimationFrame(() => {
    OverviewFitPending = false;
    FitOverview();
  });
}

function FitOverview() {
  const Grid = document.getElementById('DeviceGrid');
  Grid.style.zoom = '1';
  if (!document.getElementById('Overview').classList.contains('Active') ||
      !window.matchMedia('(min-width: 30rem) and (pointer: coarse)').matches)
    return;

  const Main = document.getElementById('MainContent');
  const MainStyle = getComputedStyle(Main);
  const Viewport = window.visualViewport;
  const ViewportHeight = Viewport ? Viewport.height : window.innerHeight;
  const AvailableWidth = Main.clientWidth - parseFloat(MainStyle.paddingLeft) - parseFloat(MainStyle.paddingRight);
  const AvailableHeight = ViewportHeight - (Grid.getBoundingClientRect().top + window.scrollY) -
    parseFloat(MainStyle.paddingBottom) - 2;
  if (AvailableWidth <= 0 || AvailableHeight <= 0) return;

  let Scale = 1;
  for (let Attempt = 0; Attempt < 3; ++Attempt) {
    const Bounds = Grid.getBoundingClientRect();
    const Width = Math.max(Bounds.width, Grid.scrollWidth * Scale);
    const Height = Math.max(Bounds.height, Grid.scrollHeight * Scale);
    const Factor = Math.min(1, AvailableWidth / Math.max(1, Width), AvailableHeight / Math.max(1, Height));
    if (Factor >= 0.999) break;
    Scale *= Factor;
    Grid.style.zoom = String(Scale);
  }
}

function HandleDashboardResize() {
  RefitActiveFrame();
  ScheduleOverviewFit();
}

function UpdateDeviceInfo(Device) {
  if (Device.DeviceInfo === null) return;

  if (!Device.Cfg.Name) {
    Device.TechnicalName = Device.Cfg.Func || Device.DeviceInfo.Name || Device.DeviceInfo.DevId || Device.TechnicalName;
    if (Device.ConfigSelectorOption !== null)
      Device.ConfigSelectorOption.textContent = EditorDeviceLabel(Device.TechnicalName);
  }

  if (Device.DeviceInfo.CfgUrl)
    Device.CfgUrl = new URL(Device.DeviceInfo.CfgUrl, Device.BaseUrl);
  if (Device.ConfigSelectorOption !== null)
    Device.ConfigSelectorOption.disabled = Device.CfgUrl === null;
}

function SetConnectionState(Device, State) {
  Device.ConnectionState = State;
  for (const View of Device.Views || [Device]) {
    const Badge = View.StatusRow.querySelector('.ConnectionBadge');
    Badge.className = `ConnectionBadge ${State}`;
    switch (State) {
      case 'Live':
        Badge.textContent = Tr('Live', 'Live');
        break;
      case 'Polling':
        Badge.textContent = Tr('Polling', 'Polling');
        break;
      default:
        Badge.textContent = Tr('Offline', 'Offline');
        break;
    }
  }
}

function SetStateText(Element, Text, ClassName) {
  Element.textContent = Text;
  Element.className = ClassName;
}

function ShowDeviceOffline(Device) {
  SetConnectionState(Device, 'Offline');
  const ChangedGroups = new Set();
  for (const View of Device.Views || [Device]) {
    const Measurements = View.Card.querySelector('.MeasurementList');
    Measurements.hidden = true;
    Measurements.replaceChildren();

    const Channels = View.Card.querySelector('.ChannelList');
    Channels.hidden = true;
    Channels.replaceChildren();
    View.MeasurementRows = [];
    View.ChannelRows = [];
    ChangedGroups.add(View.Group);
  }
  for (const GroupId of ChangedGroups)
    UpdateGroupOutputs(DeviceGroups.get(GroupId));
  UpdateDashboardErrors();
}

function EnsureRoomTemperatureView(Device, Status) {
  if (Device.Cfg.DevType !== 'MultiTimer') return;
  const Present = (Status.Meas || []).some(Measurement => Measurement.Kind === 'RoomTemperature');
  let View = Device.Views.find(Item => Item.RoomTemperatureOnly);
  if (Present && !View) {
    View = new TDashboardDeviceView(Device, {Group: 'System', Meas: ['RoomTemperature'], Chs: [], RoomTemperatureOnly: true});
    Device.Views.push(View);
    View.Card = CreateDeviceCard(View);
    SetConnectionState(View, Device.ConnectionState || 'Offline');
  }
  if (View) View.StatusRow.hidden = !Present;
}

function UpdateDeviceStatus(Device) {
  const Status = Device.Status;
  if (Status === null) return;
  UpdateFeedbackRows(Device);
  UpdateMeasurements(Device);
  UpdateChannels(Device);
  UpdateGroupOutputs(DeviceGroups.get(Device.Group));
}

function UpdateFeedbackRows(Device) {
  if (Device.RoomTemperatureOnly) { Device.FeedbackRows = []; return; }
  Device.FeedbackRows = (Device.Status.FeedbackMonitors || []).map(Monitor => {
    const Row = CreateElement('div', 'ChannelRow FeedbackRow');
    Row.dataset.outputId = `Feedback.${Monitor.Id}`;
    const Labels = {OK: Tr('FeedbackOK', 'Zustand bestätigt'),
      Unknown: String(Monitor.Reason).startsWith('Invalid') ? Tr('FeedbackInvalid', 'Rückmeldung inkorrekt') : Tr('FeedbackUnknown', 'Rückmeldung fehlt'),
      Pending: Tr('FeedbackPending', 'Zustand wird geprüft'), Mismatch: Tr('FeedbackMismatch', 'Zustand nicht bestätigt')};
    Row.append(CreateElement('span', '', `- ${Tr('FeedbackLabel', 'Rückmeldung')}`),
      CreateElement('span', Monitor.State === 'Mismatch' ? 'Error' : '', Labels[Monitor.State] || Monitor.State));
    return Row;
  });
}

function ReadFeedbackSoundPreference() {
  try {
    const Prefix = `${FeedbackSoundCookieName}=`;
    const Cookie = document.cookie.split(';').map(Entry => Entry.trim()).find(Entry => Entry.startsWith(Prefix));
    if (!Cookie) return null;
    const Match = /^([01])(?:\.(\d+))?$/.exec(Cookie.slice(Prefix.length));
    if (!Match) return null;
    const Expires = Match[2] ? Number(Match[2]) : null;
    if (Expires !== null && (!Number.isSafeInteger(Expires) || Expires <= Date.now() / 1000)) return null;
    return {Enabled: Match[1] === '1', Expires};
  } catch (Error) {
    console.warn('Cannot read feedback sound preference', Error);
    return null;
  }
}

function ReadFeedbackSoundCookie() {
  return ReadFeedbackSoundPreference()?.Enabled === true;
}

function StoreFeedbackSoundCookie(Enabled) {
  try {
    // Store the expiry in the value too: document.cookie does not expose Max-Age.
    const Expires = Math.floor(Date.now() / 1000) + 31536000;
    document.cookie = `${FeedbackSoundCookieName}=${Enabled ? '1' : '0'}.${Expires}; Max-Age=31536000; Path=/; SameSite=Lax`;
  } catch (Error) {
    console.warn('Cannot store feedback sound preference', Error);
  }
}

function RenewFeedbackSoundCookie() {
  const Preference = ReadFeedbackSoundPreference();
  // Older 0/1 cookies have no expiry metadata; migrate them on first use.
  if (Preference && (Preference.Expires === null || Preference.Expires - Date.now() / 1000 <= 30 * 86400))
    StoreFeedbackSoundCookie(Preference.Enabled);
}

// Fixed local SVG symbols; labels describe the action, independently of audio permission.
function ActionIcon(Name) {
  const Paths = {
    Sound: '<path d="M11 5 6 9H3v6h3l5 4z"/><path d="M15 8a6 6 0 0 1 0 8m3-11a10 10 0 0 1 0 14"/>',
    Mute: '<path d="M11 5 6 9H3v6h3l5 4z"/><path d="m3 3 18 18"/>',
    Trash: '<path d="M3 7h18M9 7V4h6v3M6 7l1 14h10l1-14M10 11v6m4-6v6"/>'
  };
  return `<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true" focusable="false">${Paths[Name]}</svg>`;
}

const FeedbackAlarm = new TFeedbackAlarm(() => {
  const Toggle = document.getElementById('FeedbackSoundToggle');
  const Running = FeedbackAlarm.Enabled && FeedbackAlarm.Context?.state === 'running';
  const Key = Running ? 'AlarmSoundOff' : FeedbackAlarm.Requested ? 'AlarmSoundRelease' : 'AlarmSoundOn';
  Toggle.dataset.trTitle = Key;
  Toggle.title = Tr(Key); Toggle.setAttribute('aria-label', Tr(Key));
  Toggle.innerHTML = ActionIcon(Running ? 'Mute' : 'Sound');
  Toggle.setAttribute('aria-pressed', String(FeedbackAlarm.Enabled && FeedbackAlarm.Context?.state === 'running'));
  document.getElementById('FeedbackSoundAcknowledge').hidden = FeedbackAlarm.Active.size === 0;
  document.getElementById('FeedbackSoundAcknowledge').disabled = [...FeedbackAlarm.Active].every(Key => FeedbackAlarm.Acknowledged.has(Key));
});
document.getElementById('FeedbackSoundToggle').addEventListener('click', async () => {
  try {
    if (FeedbackAlarm.Enabled && FeedbackAlarm.Context?.state === 'running') {
      StoreFeedbackSoundCookie(false);
      FeedbackAlarm.Disable();
    } else {
      StoreFeedbackSoundCookie(true);
      await FeedbackAlarm.Enable();
    }
  } catch (Error) {
    ShowDashboardMessage(Tr('AlarmSoundBlocked', 'Ton konnte nicht aktiviert werden. Browserfreigabe und Gerätelautstärke prüfen.'));
  }
});
document.getElementById('FeedbackSoundAcknowledge').addEventListener('click', () => FeedbackAlarm.Acknowledge());

function UpdateDashboardErrors() {
  const Panel = document.getElementById('DeviceErrorMessages');
  const List = document.getElementById('DeviceErrorList');
  const Entries = [];
  for (const Device of Devices) {
    if (Device.Status === null || Device.ConnectionState === 'Offline') continue;
    const Errors = [...new Set((Device.Status.Errs || []).map(String))];
    if (Device.Status.LastErr?.Message)
      Errors.push(`${Tr('LastLogError', 'Last logged error')}: ${Device.Status.LastErr.Message}`);
    if (Errors.length)
      Entries.push({Name: DeviceDisplayName(Device.TechnicalName, Device.Cfg), Errors});
  }
  List.replaceChildren(...Entries.map(Entry => {
    const Row = CreateElement('div', 'DeviceErrorEntry');
    Row.append(CreateElement('strong', 'DeviceErrorSource', Entry.Name),
      CreateElement('span', 'DeviceErrorText', Entry.Errors.join(' · ')));
    return Row;
  }));
  Panel.hidden = Entries.length === 0;
}

function FormatMeasurement(Measurement) {
  const Unit = Measurement.Unit ? ` ${Measurement.Unit}` : '';
  const Value = Number(Measurement.Value);
  if (Measurement.Valid === false || Measurement.Value === null || !Number.isFinite(Value))
    return `—${Unit}`;

  const ConfiguredDecimals = Number(Measurement.Decimals);
  const Decimals = Number.isInteger(ConfiguredDecimals)
    ? Math.min(4, Math.max(0, ConfiguredDecimals))
    : 1;
  return `${Value.toFixed(Decimals)}${Unit}`;
}

function CreateMeasurementRow(Measurement) {
  const Row = CreateElement('div', 'MeasurementRow');
  Row.dataset.outputId = String(Measurement.Kind || '');
  Row.appendChild(CreateElement('span', 'MeasurementName', Measurement.Kind === 'RoomTemperature' ? Tr('RoomTemperature', 'Room temperature') : Measurement.Name || Measurement.Kind || 'Messwert'));
  const Value = CreateElement(
    'strong',
    Measurement.Valid === false ? 'MeasurementValue Invalid' : 'MeasurementValue',
    FormatMeasurement(Measurement)
  );
  if (Measurement.Err)
    Value.title = Measurement.Err;
  Row.appendChild(Value);
  return Row;
}

function CreatePowerMeterRow(PowerMeter, Name, Label = null) {
  const Row = CreateElement('div', 'PowerMeterRow');
  Row.dataset.outputId = 'Power';
  Row.dataset.powerLabel = Name;
  const Voltage = Number(PowerMeter?.Voltage);
  const Current = Number(PowerMeter?.Current);
  const ActivePower = Number(PowerMeter?.ActivePower);
  const Valid = PowerMeter?.Valid !== false &&
    Number.isFinite(Voltage) && Number.isFinite(Current) && Number.isFinite(ActivePower);

  if (!Valid) {
    Row.classList.add('Invalid');
    Row.append(
      CreateElement('span', 'PowerMeterName', Label ?? Tr(Name, Tr('Power', 'Power'))),
      CreateElement('strong', 'PowerMeterValue', Tr('MeasurementInvalid', 'Messwerte noch nicht gültig'))
    );
    return Row;
  }

  Row.append(
    CreateElement('span', 'PowerMeterName', Label ?? Tr(Name, Tr('Power', 'Power'))),
    CreateElement('strong', 'PowerMeterValue',
      `${Voltage.toFixed(1)}V, ${Current.toFixed(3)}A, ${ActivePower.toFixed(1)}W`)
  );
  return Row;
}

function UpdateMeasurements(Device) {
  const Container = Device.Card.querySelector('.MeasurementList');
  const Measurements = (Device.Status.Meas || []).filter(Measurement => (Measurement.Kind !== 'RoomTemperature' || Device.RoomTemperatureOnly) && (Device.MeasurementIds === null || Device.MeasurementIds.has(Measurement.Kind)));
  const Rows = Measurements.map(CreateMeasurementRow);
  if (Device.Cfg.ExternalSourceId && Object.hasOwn(Device.Status, 'BatteryPct')) {
    const Value = Device.Status.BatteryPct;
    const BatteryRow = CreateMeasurementRow({Kind: 'BatteryPercent',
      Name: Tr('LaptopBatteryLabel', 'Laptop Akku'), Value,
      Valid: typeof Value === 'number' && Number.isFinite(Value) && Value >= 0 && Value <= 100,
      Unit: '%', Decimals: 0});
    BatteryRow.dataset.outputId = 'LaptopBattery';
    Rows.push(BatteryRow);
  }
  if (Device.Status.PowerMeter)
    Rows.push(CreatePowerMeterRow(Device.Status.PowerMeter, Device.Cfg.PowerLabel || 'Power',
      Device.Cfg.UseHwName && Device.Cfg.HwName
        ? `${Tr('Power', 'Power')} ${Device.Cfg.HwName}` : Device.Cfg.EndpointFunction ? `${Tr('Power', 'Power')} ${ChannelDisplayName({Name: Device.Cfg.EndpointFunction})}` : null));
  Container.hidden = Rows.length === 0;
  Container.replaceChildren(...Rows);
  Device.MeasurementRows = Rows;
}

function UpdateChannels(Device) {
  const Container = Device.Card.querySelector('.ChannelList');
  if (Device.Cfg.StatusOnly === true) {
    Container.hidden = true;
    Container.replaceChildren();
    return;
  }
  Container.hidden = false;
  const AllChannels = Device.Status.Chs || [];
  const Type = Device.DeviceInfo?.DevType ?? Device.Client?.Dev?.DevType;
  const Feeder = Type === 'Feeder' || Type === 'FeederSimple';
  const Channels = (Device.ChannelIds === null
    ? AllChannels
    : AllChannels.filter(Channel => Device.ChannelIds.has(String(Channel.Name || Channel.Id))))
    .filter(Channel => !(Feeder && (Channel.HwCh === 0 || Channel.Id?.endsWith('/0'))) &&
      String(Channel.Name || '').toLowerCase() !== 'fütterung');
  const Rows = Channels.map(Channel => CreateChannelRow(Device, Channel));
  Container.replaceChildren(...Rows);
  Device.ChannelRows = Rows;
}

function UpdateGroupOutputs(Group) {
  if (!Group) return;
  const OutputRows = Group.Views.flatMap(View => [...View.MeasurementRows, ...View.ChannelRows, ...(View.FeedbackRows || [])]);
  const UsedRows = new Set();
  const OrderedIds = OverviewOutputOrder[Group.Section.dataset.group];
  let HasPowerSlot = false;
  Group.Outputs.replaceChildren();

  if (Array.isArray(OrderedIds)) {
    for (const OutputId of OrderedIds) {
      if (OutputId === 'Power') {
        HasPowerSlot = true;
        continue;
      }
      const Matches = OutputRows.filter(Row => !UsedRows.has(Row) && Row.dataset.outputId === OutputId);
      if (Matches.length === 0) {
        Group.Outputs.appendChild(CreateElement('div', 'GroupOutputGap'));
        continue;
      }
      for (const Row of Matches) {
        UsedRows.add(Row);
        Group.Outputs.appendChild(Row);
      }
    }
  }

  const OtherRows = OutputRows.filter(Row => !UsedRows.has(Row) && !Row.classList.contains('PowerMeterRow'));
  for (const Row of OtherRows) {
    UsedRows.add(Row);
    Group.Outputs.appendChild(Row);
  }

  const PowerOrder = {PowerHeater: 0, PowerLight: 1};
  const PowerRows = OutputRows.filter(Row => Row.classList.contains('PowerMeterRow'))
    .sort((Left, Right) => (PowerOrder[Left.dataset.powerLabel] ?? 2) -
      (PowerOrder[Right.dataset.powerLabel] ?? 2));
  if (HasPowerSlot && PowerRows.length === 0)
    Group.Outputs.appendChild(CreateElement('div', 'GroupOutputGap'));
  for (const Row of PowerRows) {
    UsedRows.add(Row);
    Group.Outputs.appendChild(Row);
  }
  if (Group.Section.dataset.group === 'System') {
    const LaptopOutputIds = ['LaptopSw', 'Feedback.LaptopPower', 'LaptopBattery'];
    const Children = [...Group.Outputs.children];
    const LaptopRows = new Map(LaptopOutputIds.map(Id =>
      [Id, Children.find(Row => Row.dataset.outputId === Id)]));
    const ExistingRows = new Set([...LaptopRows.values()].filter(Boolean));
    if (ExistingRows.size > 1) {
      const FirstIndex = Children.findIndex(Row => ExistingRows.has(Row));
      const Anchor = Children.slice(FirstIndex).find(Row => !ExistingRows.has(Row)) || null;
      const OrderedRows = document.createDocumentFragment();
      for (const Id of LaptopOutputIds) {
        const Row = LaptopRows.get(Id);
        if (Row) OrderedRows.appendChild(Row);
      }
      Group.Outputs.insertBefore(OrderedRows, Anchor);
    }
  }
  ScheduleOverviewFit();
}

function GetButtonAction(Device, Channel) {
  const Action = Device.DeviceInfo?.ButtonPress;
  if (!Action?.Url || !Channel.HasOut || !Number.isInteger(Channel.HwCh)) return null;

  const Presses = (Action.Presses || []).map(Press => String(Press).toLowerCase());
  return {
    Url: Action.Url,
    Short: Presses.includes('short'),
    Long: Presses.includes('long')
  };
}

function BuildButtonActionUrl(Device, Channel, Press) {
  const Action = GetButtonAction(Device, Channel);
  if (Action === null) throw new Error('Tasteraktion wird von diesem Kanal nicht unterstützt.');

  const Url = Action.Url
    .replace('{ChannelId}', encodeURIComponent(String(Channel.HwCh)))
    .replace('{Press}', encodeURIComponent(String(Press).toLowerCase()));
  return new URL(Url);
}

function ShowDashboardMessage(Message, IsError = true) {
  const Element = document.getElementById('DashboardMessage');
  Element.textContent = Message;
  Element.classList.toggle('Success', !IsError);
  Element.hidden = false;
  clearTimeout(ShowDashboardMessage.Timer);
  ShowDashboardMessage.Timer = setTimeout(() => {
    Element.hidden = true;
    Element.classList.remove('Success');
  }, 5000);
}

async function TriggerButtonPress(Device, Channel, Press, Button = null) {
  const OriginalText = Button?.textContent;
  let Succeeded = false;
  if (Button !== null) {
    Button.disabled = true;
    Button.classList.add('Busy');
    Button.setAttribute('aria-busy', 'true');
    if (Button.classList.contains('DetailActionButton'))
      Button.textContent = 'Wird ausgeführt …';
  }

  const Controller = new AbortController();
  const Timeout = setTimeout(() => Controller.abort(), DeviceActionTimeoutMilliseconds);
  try {
    const Response = await fetch(BuildButtonActionUrl(Device, Channel, Press), {
      method: 'POST',
      cache: 'no-store',
      signal: Controller.signal
    });
    if (!Response.ok) {
      const Text = (await Response.text()).trim();
      throw new Error(Text || `HTTP ${Response.status}`);
    }
    Succeeded = true;
    ShowDashboardMessage(
      `${ChannelDisplayName(Channel, Device)}: ${Press === 'long' ? 'langer' : 'kurzer'} Tastendruck ausgelöst.`,
      false
    );
    if (Button?.classList.contains('DetailActionButton'))
      Button.textContent = 'Ausgeführt';
    setTimeout(() => Device.Client?.PollStatus(), 150);
  } catch (Error) {
    const Message = Error.name === 'AbortError'
      ? `Zeitüberschreitung nach ${DeviceActionTimeoutMilliseconds / 1000} Sekunden`
      : Error.message;
    ShowDashboardMessage(`${ChannelDisplayName(Channel, Device)}: Aktion fehlgeschlagen: ${Message}`);
  } finally {
    clearTimeout(Timeout);
    if (Button !== null) {
      Button.disabled = false;
      Button.classList.remove('Busy');
      Button.removeAttribute('aria-busy');
      if (OriginalText !== undefined) {
        if (Succeeded && Button.classList.contains('DetailActionButton'))
          setTimeout(() => { Button.textContent = OriginalText; }, 900);
         else
          Button.textContent = OriginalText;
      }
    }
  }
}

function CreateChannelRow(Device, Channel) {
  const Row = CreateElement('div', 'ChannelRow');
  Row.dataset.outputId = String(Channel.Name || Channel.Id);
  const Details = CreateElement('button', 'ChannelDetailsButton');
  Details.type = 'button';
  Details.appendChild(CreateElement('span', 'ChannelName', ChannelDisplayName(Channel, Device)));
  Details.addEventListener('click', () => OpenChannelDetails(Device, Channel));
  Details.setAttribute('aria-label', `${ChannelDisplayName(Channel, Device)}: Details öffnen.`);
  Row.appendChild(Details);

  const State = Channel.HasOut ? Channel.State : Channel.InState;
  const Action = GetButtonAction(Device, Channel);
  const StateElement = CreateElement(Action?.Short ? 'button' : 'span', 'ChannelState');
  if (Action?.Short) {
    StateElement.type = 'button';
    StateElement.classList.add('Action');
    StateElement.addEventListener('click', () => TriggerButtonPress(Device, Channel, 'short', StateElement));
    StateElement.setAttribute(
      'aria-label',
      `${ChannelDisplayName(Channel, Device)}: ${FormatState(State, Channel)}. Kurzen Tastendruck auslösen.`
    );
    StateElement.title = 'Kurzen Tastendruck auslösen';
  }
  if (State === true)
    SetStateText(StateElement, FormatState(State, Channel), `${StateElement.className} On`.trim());
   else if (State === false)
    SetStateText(StateElement, FormatState(State, Channel), `${StateElement.className} Off`.trim());
   else
    SetStateText(StateElement, FormatState(State, Channel), `${StateElement.className} Unknown`.trim());
  Row.appendChild(StateElement);

  if (Channel.Err || Channel.OvrActive) {
    Row.classList.add('HasMarker');
    Row.appendChild(CreateElement('span', Channel.Err ? 'ChannelMarker Error' : 'ChannelMarker Override', '●'));
  }
  return Row;
}

function FormatState(State, Channel = null) {
  const Labels = Channel?.StateLabels || {};
  if (State === true) return Labels.True ? Tr(Labels.True) : Tr('On', 'EIN');
  if (State === false) return Labels.False ? Tr(Labels.False) : Tr('Off', 'AUS');
  return Labels.Unknown ? Tr(Labels.Unknown) : '–';
}

function FormatOverride(Channel) {
  const State = FormatState(Channel.OvrState, Channel);
  if (Channel.OvrUntil)
    return `${State} bis ${FormatDateTime(Channel.OvrUntil)}`;
  return `${State} ${Tr('UntilNextEvent', 'bis zum nächsten Ereignis')}`;
}

const NextChangeSource = Object.fromEntries(GnJson.Describe('TNextChangeSource').Enum.map(Item => [Item.Name, Item.Value]));

function FormatNextChange(Channel, NextChange) {
  if (NextChange.Src === NextChangeSource.FeedTime) {
    const Details = [FormatDateTime(NextChange.Time)];
    if (Number.isFinite(Number(NextChange.Portions))) {
      const Portions = Number(NextChange.Portions);
      Details.push(`${Portions} ${Portions === 1 ? Tr('Portion', 'Portion') : Tr('Portions', 'Portionen')}`);
    }
    return Details.join(' · ');
  }

  const SourceNames = {
    [NextChangeSource.OverrideEnd]: Tr('OverrideEnd', 'Override-Ende'),
    [NextChangeSource.SwitchTime]: Tr('SwitchTime', 'Schaltzeit'),
    [NextChangeSource.CalibrationOverride]: Tr('CalibrationOverride', 'Kalibrierübersteuerung')
  };
  const Source = SourceNames[NextChange.Src] || Tr('Change', 'Änderung');
  return `${FormatState(NextChange.State, Channel)} · ${FormatDateTime(NextChange.Time)} · ${Source}`;
}

function FormatDateTime(UnixTime) {
  if (!Number.isFinite(Number(UnixTime))) return '–';
  return new Intl.DateTimeFormat(LangNr === 1 ? 'de-DE' : 'en-GB', {
    dateStyle: 'short',
    timeStyle: 'medium'
  }).format(new Date(Number(UnixTime) * 1000));
}

function FormatTime(DateValue) {
  if (!(DateValue instanceof Date)) return '–';
  return new Intl.DateTimeFormat(LangNr === 1 ? 'de-DE' : 'en-GB', {timeStyle: 'medium'}).format(DateValue);
}

function FormatShortTime(DateValue) {
  if (!(DateValue instanceof Date)) return '–';
  return new Intl.DateTimeFormat(LangNr === 1 ? 'de-DE' : 'en-GB', {hour: '2-digit', minute: '2-digit'}).format(DateValue);
}

function SanitizeId(Value) {
  return String(Value).replace(/[^A-Za-z0-9_-]/g, '-');
}

function AddDetailRow(Container, Label, Value, ClassName = '') {
  if (Value === null || Value === undefined || Value === '') return;
  const Row = CreateElement('div', `DetailRow ${ClassName}`.trim());
  Row.append(
    CreateElement('span', 'DetailLabel', Label),
    CreateElement('strong', 'DetailValue', String(Value))
  );
  Container.appendChild(Row);
}

function OpenChannelDetails(Device, Channel) {
  const State = Channel.HasOut ? Channel.State : Channel.InState;
  const Body = document.getElementById('DetailBody');
  document.getElementById('DetailTitle').textContent = ChannelDisplayName(Channel, Device);
  Body.replaceChildren();

  AddDetailRow(Body, Tr('Device', 'Gerät'), Device.DisplayName);
  AddDetailRow(Body, Tr('State', 'Zustand'), FormatState(State, Channel), State === true ? 'Good' : '');
  if (Channel.HasOut && Channel.AutomaticState !== null && Channel.AutomaticState !== undefined)
    AddDetailRow(Body, Tr('Automatic', 'Automatik'), FormatState(Channel.AutomaticState, Channel));
  if (Channel.HasIn)
    AddDetailRow(Body, Tr('Input', 'Eingang'), FormatState(Channel.InState));
  if (Channel.OvrActive)
    AddDetailRow(Body, Tr('ManualOverride', 'Manuelle Vorgabe'), FormatOverride(Channel), 'Warning');
  if (Channel.NextChange)
    AddDetailRow(
      Body,
      Channel.NextChange.Src === NextChangeSource.FeedTime ? Tr('NextFeeding', 'Nächste Fütterung') : Tr('NextChange', 'Nächste Änderung'),
      FormatNextChange(Channel, Channel.NextChange)
    );
  if (Channel.Err)
    AddDetailRow(Body, Tr('Error', 'Fehler'), Channel.Err, 'Error');

  AddDetailRow(Body, Tr('Connection', 'Verbindung'),
    Device.Views[0]?.StatusRow.querySelector('.ConnectionBadge').textContent || Tr('Offline', 'Offline'));
  if (Device.Status !== null) {
    AddDetailRow(Body, Tr('System', 'System'), Device.Status.Healthy ? 'OK' : Tr('Error', 'Fehler'), Device.Status.Healthy ? 'Good' : 'Error');
    AddDetailRow(Body, Tr('TimeSync', 'Zeitsynchronisation'), Device.Status.TimeSynchronized ? Tr('Synchronized', 'Synchron') : Tr('NotSynchronized', 'Nicht synchron'));
  }
  AddDetailRow(Body, Tr('Updated', 'Aktualisiert'), FormatTime(Device.LastStatusAt));
  if (Device.DeviceInfo?.FirmwareVer)
    AddDetailRow(Body, 'Firmware', Device.DeviceInfo.FirmwareVer);
  if (Device.DeviceInfo?.DataVer)
    AddDetailRow(Body, 'Data', Device.DeviceInfo.DataVer);

  const Action = GetButtonAction(Device, Channel);
  if (Action?.Short || Action?.Long) {
    const Actions = CreateElement('section', 'DetailActions');
    Actions.appendChild(CreateElement('h3', '', Tr('Actions', 'Aktionen')));
    if (Action.Short) {
      const ShortButton = CreateElement('button', 'PrimaryButton DetailActionButton', Tr('ShortPress', 'Kurz drücken'));
      ShortButton.type = 'button';
      ShortButton.addEventListener('click', () => TriggerButtonPress(Device, Channel, 'short', ShortButton));
      Actions.appendChild(ShortButton);
    }
    if (Action.Long) {
      const LongButton = CreateElement('button', 'SecondaryButton DetailActionButton', Tr('LongPress', 'Lang drücken'));
      LongButton.type = 'button';
      LongButton.addEventListener('click', () => {
        if (LongButton.dataset.confirm === 'true') {
          delete LongButton.dataset.confirm;
          clearTimeout(LongButton.ConfirmTimer);
          LongButton.textContent = Tr('LongPress', 'Lang drücken');
          TriggerButtonPress(Device, Channel, 'long', LongButton);
          return;
        }
        LongButton.dataset.confirm = 'true';
        LongButton.textContent = Tr('ConfirmAgain', 'Nochmals bestätigen');
        LongButton.ConfirmTimer = setTimeout(() => {
          delete LongButton.dataset.confirm;
          LongButton.textContent = Tr('LongPress', 'Lang drücken');
        }, LongPressConfirmMilliseconds);
      });
      Actions.appendChild(LongButton);
    }
    Body.appendChild(Actions);
  }

  const Overlay = document.getElementById('DetailOverlay');
  Overlay.hidden = false;
  document.body.classList.add('DetailOpen');
  document.getElementById('DetailCloseButton').focus();
}

function CloseChannelDetails() {
  document.getElementById('DetailOverlay').hidden = true;
  document.body.classList.remove('DetailOpen');
}

function CreateConfigurationTab(Device) {
  const Option = CreateElement('option', '', EditorDeviceLabel(Device.TechnicalName, Device.Cfg));
  Option.value = Device.ConfigTabId;
  Option.disabled = Device.CfgUrl === null;
  document.getElementById('PageSelector').appendChild(Option);
  Device.ConfigSelectorOption = Option;

  const Page = CreateElement('section', 'TabPage ConfigPage');
  Page.id = Device.ConfigTabId;
  Page.dataset.deviceIndex = String(Devices.length);

  const Viewport = CreateElement('div', 'ConfigViewport');
  Viewport.appendChild(CreateElement('div', 'ConfigPlaceholder', Tr('LoadingConfiguration', 'Konfiguration wird beim Öffnen geladen.')));
  Page.appendChild(Viewport);
  document.getElementById('ConfigTabs').appendChild(Page);

  Device.ConfigPage = Page;
  Device.ConfigViewport = Viewport;
}

function GetConfigurationFrameUrl(Device) {
  const Url = new URL(Device.CfgUrl);
  if (Device.DeviceInfo?.DataVer)
    Url.searchParams.set('_DataVersion', Device.DeviceInfo.DataVer);
  return Url.toString();
}

function LoadConfigurationFrame(Device) {
  if (Device.ConfigFrame !== null || Device.CfgUrl === null) return;

  const Canvas = CreateElement('div', 'ConfigCanvas');
  const Frame = CreateElement('iframe', 'ConfigFrame');
  Frame.src = GetConfigurationFrameUrl(Device);
  Frame.title = `Konfiguration ${Device.DisplayName}`;
  Frame.loading = 'eager';
  Frame.addEventListener('load', () => {
    FitFrame(Device);
  });
  Canvas.appendChild(Frame);
  Device.ConfigViewport.replaceChildren(Canvas);

  Device.ConfigCanvas = Canvas;
  Device.ConfigFrame = Frame;
  if (ActiveConfigDevice === Device)
    document.getElementById('FrameReloadButton').disabled = false;
  if ('ResizeObserver' in window) {
    Device.ConfigResizeObserver = new ResizeObserver(() => FitFrame(Device));
    Device.ConfigResizeObserver.observe(Device.ConfigViewport);
  }
  requestAnimationFrame(() => FitFrame(Device));
}

function ReloadConfigurationFrame(Device) {
  if (Device.ConfigFrame === null) {
    LoadConfigurationFrame(Device);
    return;
  }
  Device.ConfigScale = 1;
  UpdateFrameScaleLabel(Device);
  Device.ConfigFrame.src = GetConfigurationFrameUrl(Device);
}

function NormalizeConfigurationSetting(Name, Value) {
  if (Name !== ConfigurationLanguageName) return null;
  const NumberValue = Number(Value);
  return Number.isInteger(NumberValue) && NumberValue >= 0 ? NumberValue : null;
}

function LegacyConfigurationLanguageKey(Device) {
  return `ConfigurationLanguage:${Device.ConfigTabId}`;
}

function ReadStoredConfigurationSetting(Name, PreferredDevice = null) {
  try {
    const RawStoredValue = localStorage.getItem(Name);
    const StoredValue = RawStoredValue === null ? null : NormalizeConfigurationSetting(Name, RawStoredValue);
    if (StoredValue !== null) return StoredValue;
    if (Name !== ConfigurationLanguageName) return null;

    const OrderedDevices = PreferredDevice === null
      ? Devices
      : [PreferredDevice, ...Devices.filter(Device => Device !== PreferredDevice)];
    let LegacyValue = null;
    for (const Device of OrderedDevices) {
      const RawValue = localStorage.getItem(LegacyConfigurationLanguageKey(Device));
      const Value = RawValue === null ? null : NormalizeConfigurationSetting(Name, RawValue);
      if (LegacyValue === null && Value !== null) LegacyValue = Value;
    }
    for (const Device of Devices)
      localStorage.removeItem(LegacyConfigurationLanguageKey(Device));
    if (LegacyValue !== null)
      localStorage.setItem(Name, String(LegacyValue));
    return LegacyValue;
  } catch (Error) {
    return null;
  }
}

function StoreConfigurationSetting(Name, Value) {
  const NormalizedValue = NormalizeConfigurationSetting(Name, Value);
  if (NormalizedValue === null) return null;
  try {
    localStorage.setItem(Name, String(NormalizedValue));
  } catch (Error) {
    console.warn(`Could not store configuration setting ${Name}`, Error);
  }
  return NormalizedValue;
}

function SendConfigurationSetting(TargetWindow, Name, Found, Value = null) {
  TargetWindow?.postMessage({
    Src: 'GnDashboard',
    Type: 'SettingValue',
    Name,
    Found,
    Value
  }, '*');
}

function BroadcastConfigurationSetting(Name, Value) {
  for (const Device of Devices)
    SendConfigurationSetting(Device.ConfigFrame?.contentWindow, Name, true, Value);
}

function UpdateFrameScaleLabel(Device) {
  if (ActiveConfigDevice !== Device) return;
  document.getElementById('FrameScaleLabel').textContent = `Auto ${Math.round(Device.ConfigScale * 100)} %`;
}

function HandleConfigurationMessage(Event) {
  const Device = Devices.find(Item => Item.ConfigFrame?.contentWindow === Event.source);
  if (Device === undefined || Event.data?.Src !== 'GnConfigPage') return;

  if (Event.data.Type === 'ViewportScale') {
    const Scale = Number(Event.data.Scale);
    if (!Number.isFinite(Scale) || Scale <= 0 || Scale > 1) return;
    Device.ConfigScale = Scale;
    UpdateFrameScaleLabel(Device);
    return;
  }

  if (Event.data.Type === 'GetSetting') {
    const Name = String(Event.data.Name || '');
    const Value = ReadStoredConfigurationSetting(Name, Device);
    SendConfigurationSetting(Event.source, Name, Value !== null, Value);
    return;
  }

  if (Event.data.Type === 'SetSetting') {
    const Name = String(Event.data.Name || '');
    const Value = StoreConfigurationSetting(Name, Event.data.Value);
    if (Value !== null)
      BroadcastConfigurationSetting(Name, Value);
  }
}

function FitFrame(Device) {
  if (Device.ConfigFrame === null || !Device.ConfigPage.classList.contains('Active')) return;

  const AvailableWidth = Device.ConfigViewport.clientWidth;
  const AvailableHeight = Device.ConfigViewport.clientHeight;
  if (AvailableWidth <= 0 || AvailableHeight <= 0) return;

  if (Device.Cfg.Responsive === true) {
    Device.ConfigCanvas.style.width = `${AvailableWidth}px`;
    Device.ConfigCanvas.style.height = `${AvailableHeight}px`;
    Device.ConfigFrame.style.width = `${AvailableWidth}px`;
    Device.ConfigFrame.style.height = `${AvailableHeight}px`;
    Device.ConfigFrame.style.transform = 'none';
    Device.ConfigViewport.classList.remove('HorizontalScroll');
    UpdateFrameScaleLabel(Device);
    return;
  }

  const ConfiguredWidth = Number(Device.Cfg.FrameWidth);
  const NaturalFrameWidth = Number.isFinite(ConfiguredWidth) && ConfiguredWidth >= 320
    ? ConfiguredWidth
    : DefaultFrameWidth;
  const FrameWidth = Math.max(NaturalFrameWidth, AvailableWidth);
  const ConfiguredMinimum = Number(Device.Cfg.MinimumScale);
  const MinimumScale = Number.isFinite(ConfiguredMinimum)
    ? Math.min(0.95, Math.max(0.5, ConfiguredMinimum))
    : DefaultMinimumScale;
  const DesiredScale = AvailableWidth / NaturalFrameWidth;
  const Scale = DesiredScale >= 1
    ? 1
    : Math.max(MinimumScale, DesiredScale);
  const NeedsHorizontalScroll = DesiredScale < MinimumScale;
  const VisualWidth = Math.ceil(FrameWidth * Scale);

  Device.ConfigCanvas.style.width = `${Math.max(AvailableWidth, VisualWidth)}px`;
  Device.ConfigCanvas.style.height = `${AvailableHeight}px`;
  Device.ConfigFrame.style.width = `${FrameWidth}px`;
  Device.ConfigFrame.style.height = `${Math.ceil(AvailableHeight / Scale)}px`;
  Device.ConfigFrame.style.transform = `scale(${Scale})`;
  Device.ConfigViewport.classList.toggle('HorizontalScroll', NeedsHorizontalScroll);
  if (ActiveConfigDevice === Device) {
    document.getElementById('FrameScaleLabel').textContent = NeedsHorizontalScroll
      ? `Auto ${Math.round(Scale * 100)} % · scrollen`
      : `Auto ${Math.round(Scale * 100)} %`;
  }
}


function NewTimeUuid() {
  if (crypto.randomUUID) return crypto.randomUUID();
  const Data = crypto.getRandomValues(new Uint8Array(16));
  Data[6] = (Data[6] & 15) | 64;
  Data[8] = (Data[8] & 63) | 128;
  const Hex = [...Data].map(Value => Value.toString(16).padStart(2, '0'));
  return `${Hex.slice(0,4).join('')}-${Hex.slice(4,6).join('')}-${Hex.slice(6,8).join('')}-${Hex.slice(8,10).join('')}-${Hex.slice(10).join('')}`;
}

function CentralSetKey(Device, Type) {
  return `${Device}\n${Type}`;
}

function FindCentralSet(Device, Type) {
  return CentralTimeStore.Sets.find(Set => Set.Dev === Device && Set.Type === Type) || null;
}

function UpdateTimeKindSelector() {
  const SetSelector = document.getElementById('TimeSetSelector');
  const KindSelector = document.getElementById('TimeKindSelector');
  const Selected = CentralTimeStore.Sets.find(Set => CentralSetKey(Set.Dev, Set.Type) === SetSelector.value);
  const Kinds = Selected?.Type === 'FeedTimes'
    ? ['FeedTime']
    : ['MyTimer', 'TmrSw'].includes(Selected?.Format) ? ['SwitchTime', 'Interval'] : ['SwitchTime'];
  KindSelector.replaceChildren(...Kinds.map(Kind => {
    const Option = CreateElement('option', '', Kind);
    Option.value = Kind;
    return Option;
  }));
}

function UpdateTimeSetSelector() {
  const Selector = document.getElementById('TimeSetSelector');
  const Previous = Selector.value;
  const Sets = [...CentralTimeStore.Sets].sort((Left, Right) =>
    String(Left.Dev).localeCompare(String(Right.Dev), 'de') || String(Left.Type).localeCompare(String(Right.Type), 'de'));
  Selector.replaceChildren(...Sets.map(Set => {
    const Option = CreateElement('option', '', EditorDeviceLabel(Set.Dev));
    Option.value = CentralSetKey(Set.Dev, Set.Type);
    return Option;
  }));
  if ([...Selector.options].some(Option => Option.value === Previous)) Selector.value = Previous;
  UpdateTimeKindSelector();
  document.getElementById('TimeAddButton').disabled = Sets.length === 0;
}

function CreateTimeStatusIndicator(Synced) {
  const Indicator = document.createElement('input');
  Indicator.type = 'checkbox';
  Indicator.checked = Synced;
  Indicator.disabled = true;
  Indicator.className = 'TimeStatusIndicator';
  Indicator.title = Synced ? Tr('Synced', 'synchron') : Tr('Pending', 'ausstehend');
  Indicator.setAttribute('aria-label', Indicator.title);
  return Indicator;
}

function UpdateTimeStatusIndicators(Sets) {
  const States = new Map(Sets.map(Set => [CentralSetKey(Set.Dev, Set.Type), Set]));
  for (const Row of document.getElementById('TimesData').rows) {
    const Set = States.get(CentralSetKey(Row.dataset.device, Row.dataset.type));
    const Synced = Set?.Synced === true;
    const Cell = Row.cells[3];
    Cell.className = `TimeStatusCell ${Synced ? 'TimeStatusSynced' : 'TimeStatusPending'}`;
    Cell.dataset.sortValue = Synced ? '1' : '0';
    Cell.replaceChildren(CreateTimeStatusIndicator(Synced));
  }
}

async function FetchDashboardJson(Path) {
  const Url = new URL(Path, window.location.href);
  Url.searchParams.set('_', Date.now().toString());
  const Response = await fetch(Url, {cache: 'no-store'});
  if (!Response.ok)
    throw new Error(`${Path}: HTTP ${Response.status}`);
  return Response.json();
}

async function RefreshDashboardSyncStatus() {
  const [Times, Profiles] = await Promise.all([
    FetchDashboardJson('api/times'),
    FetchDashboardJson('api/heatprofiles')
  ]);
  if (!Array.isArray(Times.Sets) || !Array.isArray(Profiles.Sets))
    throw new Error('Ungültiges Synchronisationsformat');
  CentralTimeStore = Times;
  UpdateTimeStatusIndicators(Times.Sets);
  RenderHeatProfileStatus(Profiles);
  return Times.Sets.every(Set => Set.Synced) && Profiles.Sets.every(Set => Set.Synced);
}

function ScheduleDashboardSyncStatusRefresh() {
  if (SyncStatusRefreshTimer !== null)
    clearTimeout(SyncStatusRefreshTimer);
  SyncStatusRefreshTimer = setTimeout(() => {
    SyncStatusRefreshTimer = null;
    RefreshDashboardSyncStatus().catch(Error =>
      console.debug('Could not refresh dashboard sync status', Error));
  }, 200);
}

function CancelSyncNotice(Message) {
  const State = SyncNoticeStates.get(Message);
  if (State === undefined) return;
  State.Cancelled = true;
  if (State.Timer !== null)
    clearTimeout(State.Timer);
  SyncNoticeStates.delete(Message);
}

function ShowSyncNotice(Message, Text, IsSynced, CheckSync) {
  CancelSyncNotice(Message);
  const State = {Cancelled: false, Timer: null};
  SyncNoticeStates.set(Message, State);
  Message.textContent = Text;
  Message.classList.remove('Error');
  Message.classList.add('Success');
  Message.hidden = false;

  const Hide = () => {
    if (State.Cancelled || SyncNoticeStates.get(Message) !== State) return;
    Message.hidden = true;
    SyncNoticeStates.delete(Message);
  };
  if (IsSynced) {
    State.Timer = setTimeout(Hide, SyncNoticeHideMilliseconds);
    return;
  }

  const Poll = async () => {
    if (State.Cancelled || SyncNoticeStates.get(Message) !== State || Message.hidden) return;
    try {
      if (await CheckSync()) {
        Message.textContent = Tr('SyncCompleted', 'Synchronisierung erfolgreich.');
        State.Timer = setTimeout(Hide, SyncNoticeHideMilliseconds);
        return;
      }
    } catch (Error) {
      console.debug('Could not refresh sync status', Error);
    }
    State.Timer = setTimeout(Poll, SyncNoticePollMilliseconds);
  };
  State.Timer = setTimeout(Poll, SyncNoticePollMilliseconds);
}

function ShowSyncError(Message, Text) {
  CancelSyncNotice(Message);
  Message.textContent = Text;
  Message.classList.remove('Success');
  Message.classList.add('Error');
  Message.hidden = false;
}

function CentralTimeValue(Entry) {
  return String(Array.isArray(Entry.Data) ? Entry.Data[0] : Entry.Data?.Time || '');
}

function CentralTimeChannelId(Entry) {
  if (Entry.Kind === 'FeedTime') return '';
  if (Entry.Kind === 'Interval') return Entry.Data?.[4] || '';
  return Entry.Data?.Ch || TimeChannelAddress(Entry.Dev, TimeChannelRows(Entry.Dev)[0]);
}

function CentralTimeChannelLabel(Entry, ChannelId = CentralTimeChannelId(Entry)) {
  if (!ChannelId) return '';
  const Channel = TimeChannelRows(Entry.Dev).find(Item => TimeChannelAddress(Entry.Dev, Item) === ChannelId) || {Id: ChannelId};
  return EditorChannelLabel(Channel, Entry.Dev);
}

function RenderCentralTimes() {
  const Body = document.getElementById('TimesData');
  CentralTimeSortKey = null;
  document.querySelectorAll('.TableSortButton').forEach(Button => { Button.dataset.direction = ''; Button.removeAttribute('aria-sort'); });
  const Rows = CentralTimeStore.Times.map((Entry, Index) => {
    const Row = document.createElement('tr');
    Row.dataset.id = Entry.Id;
    Row.dataset.device = Entry.Dev;
    Row.dataset.type = Entry.Type;
    Row.dataset.kind = Entry.Kind;
    const DeviceLabel = EditorDeviceLabel(Entry.Dev);
    Row.dataset.timeSort = CentralTimeValue(Entry);
    Row.appendChild(CreateElement('td', '', String(Index + 1)));
    const DeviceChannelCell = CreateElement('td', 'TimeDeviceChannel');
    DeviceChannelCell.appendChild(CreateElement('span', 'TimeDeviceName', DeviceLabel));
    const ChannelEditor = RenderTimeChannelEditor(Entry);
    if (ChannelEditor) DeviceChannelCell.appendChild(ChannelEditor);
    Row.appendChild(DeviceChannelCell);
    const DataCell = document.createElement('td');
    DataCell.dataset.sortValue = JSON.stringify(Entry.Data);
    DataCell.appendChild(RenderTimeDataEditor(Entry));
    Row.appendChild(DataCell);
    const Set = FindCentralSet(Entry.Dev, Entry.Type);
    const StatusCell = CreateElement('td', `TimeStatusCell ${Set?.Synced ? 'TimeStatusSynced' : 'TimeStatusPending'}`);
    StatusCell.dataset.sortValue = Set?.Synced ? '1' : '0';
    StatusCell.appendChild(CreateTimeStatusIndicator(Set?.Synced === true));
    Row.appendChild(StatusCell);
    const ActionCell = document.createElement('td');
    const Actions = CreateElement('span', 'TimeOrderButtons');
    const Up = CreateElement('button', 'SecondaryButton CompactButton', '↑');
    const Down = CreateElement('button', 'SecondaryButton CompactButton', '↓');
    const Delete = CreateElement('button', 'SecondaryButton CompactButton TrashButton');
    Delete.innerHTML = ActionIcon('Trash');
    Delete.dataset.trTitle = 'Delete'; Delete.title = Tr('Delete'); Delete.setAttribute('aria-label', Tr('Delete'));
    Up.type = Down.type = Delete.type = 'button';
    Up.disabled = CentralTimeSortKey !== null || Index === 0;
    Down.disabled = CentralTimeSortKey !== null || Index + 1 === CentralTimeStore.Times.length;
    Up.onclick = () => { CentralTimeStore.Times = CollectCentralTimes(); const Position = CentralTimeStore.Times.findIndex(Item => Item.Id === Entry.Id); if (Position > 0) [CentralTimeStore.Times[Position - 1], CentralTimeStore.Times[Position]] = [CentralTimeStore.Times[Position], CentralTimeStore.Times[Position - 1]]; RenderCentralTimes(); };
    Down.onclick = () => { CentralTimeStore.Times = CollectCentralTimes(); const Position = CentralTimeStore.Times.findIndex(Item => Item.Id === Entry.Id); if (Position >= 0 && Position + 1 < CentralTimeStore.Times.length) [CentralTimeStore.Times[Position], CentralTimeStore.Times[Position + 1]] = [CentralTimeStore.Times[Position + 1], CentralTimeStore.Times[Position]]; RenderCentralTimes(); };
    Delete.onclick = () => { CentralTimeStore.Times = CollectCentralTimes().filter(Item => Item.Id !== Entry.Id); RenderCentralTimes(); };
    Actions.append(Up, Down, Delete);
    ActionCell.appendChild(Actions);
    Row.appendChild(ActionCell);
    return Row;
  });
  Body.replaceChildren(...Rows);
  UpdateTimeSetSelector();
}

function SortCentralTimes(Column, Button) {
  if (Column === 'position') {
    CentralTimeStore.Times = CollectCentralTimes();
    RenderCentralTimes();
    Button.setAttribute('aria-sort', 'ascending');
    return;
  }
  const Body = document.getElementById('TimesData');
  const Rows = [...Body.rows];
  const CurrentEntries = new Map(CollectCentralTimes().map(Entry => [Entry.Id, Entry]));
  const Direction = Button.dataset.direction === 'asc' ? 'desc' : 'asc';
  document.querySelectorAll('.TableSortButton').forEach(Item => {
    Item.dataset.direction = '';
    Item.removeAttribute('aria-sort');
  });
  Button.dataset.direction = Direction;
  CentralTimeSortKey = Column;
  Button.setAttribute('aria-sort', Direction === 'asc' ? 'ascending' : 'descending');
  Rows.sort((Left, Right) => {
    let Result = 0;
    if (Column === 'time') {
      Result = CentralTimeValue(CurrentEntries.get(Left.dataset.id) || {Data: {}})
        .localeCompare(CentralTimeValue(CurrentEntries.get(Right.dataset.id) || {Data: {}}), undefined, {numeric: true});
    } else if (Column === 'device-channel') {
      const LeftEntry = CurrentEntries.get(Left.dataset.id) || {Dev: Left.dataset.device, Kind: Left.dataset.kind, Data: {}};
      const RightEntry = CurrentEntries.get(Right.dataset.id) || {Dev: Right.dataset.device, Kind: Right.dataset.kind, Data: {}};
      const LeftChannel = CentralTimeChannelId(LeftEntry);
      const RightChannel = CentralTimeChannelId(RightEntry);
      Result = EditorDeviceLabel(LeftEntry.Dev).localeCompare(EditorDeviceLabel(RightEntry.Dev), undefined, {numeric: true}) ||
        CentralTimeChannelLabel(LeftEntry, LeftChannel).localeCompare(CentralTimeChannelLabel(RightEntry, RightChannel), undefined, {numeric: true});
    }
    return Result * (Direction === 'asc' ? 1 : -1);
  });
  Body.replaceChildren(...Rows);
  Rows.forEach((Row, Index) => Row.cells[0].textContent = String(Index + 1));
  Body.querySelectorAll('.TimeOrderButtons button:nth-child(-n+2)').forEach(Action => Action.disabled = true);
}

async function LoadCentralTimes() {
  const Message = document.getElementById('TimeMessage');
  try {
    const [Store, Assignments] = await Promise.all([
      FetchDashboardJson('api/times'),
      FetchDashboardJson('api/device-assignments')
    ]);
    if (!Array.isArray(Store.Sets) || !Array.isArray(Store.Times)) throw new Error('Ungültiges Format');
    if (!Array.isArray(Assignments.Devices) || !Array.isArray(Assignments.Endpoints))
      throw new Error('Ungültige Gerätezuordnung');
    AssignmentView = Assignments;
    CentralProfileAssignmentView = Assignments;
    CentralTimeStore = Store;
    CentralTimesDirty = false;
    Message.hidden = true;
    RenderCentralTimes();
    return true;
  } catch (Error) {
    Message.textContent = `Zeitkonfiguration konnte nicht geladen werden: ${Error.message}`;
    Message.hidden = false;
    return false;
  }
}

function DefaultCentralTimeData(Set, Kind) {
  if (Kind === 'FeedTime') return ['00:00', 255, 1];
  const ChannelState = CentralTimeStore.Chs?.[Set?.Dev];
  const ChannelRows = Array.isArray(ChannelState) ? ChannelState : ChannelState?.Rows;
  const ChannelId = TimeChannelAddress(Set.Dev, ChannelRows?.[0]);
  if (Kind === 'Interval') {
    return ['00:00', '01:00', '00:15', 255, ChannelId];
  }
  return {Time:'00:00',Days:255,Ch:ChannelId,State:false};
}

function TimeChannelAddress(Device, Row) {
  const Service = AssignmentView?.Devices?.find(Item => Item.Func === Device || Item.DevId === Device)?.DevId;
  if (!Service || !Row) return '';
  const Position = /^Main ([1-8])$/.exec(Row.PhysPos || '');
  return `${Service}/${Position ? Position[1] : 0}`;
}

function TimeChannelRows(Device) {
  const State = CentralTimeStore.Chs?.[Device];
  return Array.isArray(State) ? State : State?.Rows || [];
}

function AddTimeField(Container, Key, LabelText, Type, Value, Disabled = false) {
  const Label = CreateElement('label', `TimeField TimeField-${Key}`);
  Label.appendChild(CreateElement('span', '', LabelText));
  const Input = document.createElement('input');
  Input.type = Type;
  Input.dataset.field = Key;
  if (Type === 'checkbox') Input.checked = Boolean(Value);
  else Input.value = Value ?? '';
  Input.disabled = Disabled;
  Label.appendChild(Input);
  Container.appendChild(Label);
  return Input;
}

function AddTimeSelect(Container, Key, LabelText, Options, Value, Disabled = false) {
  const Label = CreateElement('label', `TimeField TimeField-${Key}`);
  Label.appendChild(CreateElement('span', '', LabelText));
  const Select = document.createElement('select');
  Select.dataset.field = Key;
  for (const OptionValue of Options) {
    const Option = CreateElement('option', '', OptionValue.Label);
    Option.value = OptionValue.Value;
    Option.selected = String(OptionValue.Value) === String(Value);
    Select.appendChild(Option);
  }
  const SelectedIndex = Options.findIndex(Option => String(Option.Value) === String(Value));
  if (SelectedIndex >= 0) Select.selectedIndex = SelectedIndex;
  Select.disabled = Disabled;
  Label.appendChild(Select);
  Container.appendChild(Label);
  return Select;
}

function AddTimeMask(Container, Value) {
  const Wrapper = CreateElement('div', 'TimeField TimeDays');
  Wrapper.appendChild(CreateElement('span', '', Tr('Days', 'Tage')));
  const Days = CreateElement('div', 'TimeDayChecks');
  AddTimeField(Container, 'enabled', Tr('Active', 'Aktiv'), 'checkbox', Boolean(Value & 128));
  for (const [Label, Bit] of [['Mon', 1], ['Tue', 2], ['Wed', 4], ['Thu', 8], ['Fri', 16], ['Sat', 32], ['Sun', 64]]) {
    const Item = CreateElement('label', 'TimeDayOption');
    const Input = document.createElement('input');
    Input.type = 'checkbox';
    Input.dataset.field = 'day';
    Input.dataset.bit = String(Bit);
    Input.checked = Boolean(Value & Bit);
    Item.append(Input, CreateElement('span', '', Tr(`Weekday.${Label}`, Label)));
    Days.appendChild(Item);
  }
  Wrapper.appendChild(Days);
  Container.appendChild(Wrapper);
}

function RenderTimeDataEditor(Entry) {
  const Data = Entry.Data;
  const Editor = CreateElement('div', 'TimeDataEditor');
  const IsFeed = Entry.Kind === 'FeedTime';
  const IsInterval = Entry.Kind === 'Interval';

  if (IsInterval) {
    AddTimeField(Editor, 'time', Tr('Start', 'Start'), 'time', Data[0]);
    AddTimeField(Editor, 'period', Tr('Period', 'Periode'), 'time', Data[1]);
    AddTimeField(Editor, 'onTime', Tr('OnTime', 'Ein-Zeit'), 'time', Data[2]);
  } else {
    AddTimeField(Editor, 'time', Tr('Time', 'Zeit'), 'time', IsFeed ? Data[0] : Data.Time);
  }
  const MaskIndex = IsFeed ? 1 : IsInterval ? 3 : 1;
  AddTimeMask(Editor, Number(IsFeed || IsInterval ? Data[MaskIndex] : Data.Days) || 0);

  if (IsFeed) {
    AddTimeSelect(Editor, 'portion', Tr('Portions', 'Portionen'), Array.from({length: 9}, (_, Index) => ({Value: Index + 1, Label: String(Index + 1)})), Data[2]);
  } else if (!IsInterval) {
    AddTimeField(Editor, 'active', Tr('OnOff', 'An/Aus'), 'checkbox', Data.State);
  }
  return Editor;
}

let TimeChannelMeasureContext = null;
let TimeChannelMeasureFont = '';
let TimeChannelSelectOverhead = 0;
function FitTimeChannelSelect(Select) {
  const Font = getComputedStyle(document.body).font;
  if (!TimeChannelMeasureContext) {
    TimeChannelMeasureContext = document.createElement('canvas').getContext('2d');
    if (!TimeChannelMeasureContext) return;
  }
  if (Font !== TimeChannelMeasureFont) {
    TimeChannelMeasureFont = Font;
    TimeChannelMeasureContext.font = Font;
    const Probe = document.createElement('select');
    Probe.className = 'TimeChannelSelect';
    Probe.style.cssText = 'position:fixed;left:-10000px;top:0;visibility:hidden;width:auto';
    const Option = document.createElement('option');
    Option.textContent = 'M';
    Probe.appendChild(Option);
    document.body.appendChild(Probe);
    TimeChannelSelectOverhead = Math.max(0, Probe.getBoundingClientRect().width - TimeChannelMeasureContext.measureText('M').width);
    Probe.remove();
  }
  TimeChannelMeasureContext.font = Font;
  const TextWidth = Math.max(0, ...[...Select.options].map(Option => TimeChannelMeasureContext.measureText(Option.textContent).width));
  Select.style.width = `${Math.ceil(TextWidth + TimeChannelSelectOverhead + 1)}px`;
}

function RenderTimeChannelEditor(Entry) {
  if (Entry.Kind === 'FeedTime') return null;
  const Set = FindCentralSet(Entry.Dev, Entry.Type);
  const Data = Entry.Data;
  const IsInterval = Entry.Kind === 'Interval';
  const IsMyTimer = Set?.Format === 'MyTimer';
  const Rows = TimeChannelRows(Entry.Dev);
  const DefaultChannel = TimeChannelAddress(Entry.Dev, Rows[0]);
  const ChannelId = IsInterval ? (Data[4] || DefaultChannel) : (Data.Ch || DefaultChannel);
  if (!ChannelId) return null;

  const Options = IsMyTimer
    ? Rows.map(Row => ({Value: TimeChannelAddress(Entry.Dev, Row), Label: EditorChannelLabel(Row, Entry.Dev)}))
    : [{Value: ChannelId, Label: CentralTimeChannelLabel(Entry, ChannelId)}];
  const Select = document.createElement('select');
  Select.className = 'TimeChannelSelect';
  Select.dataset.field = 'channel';
  Select.setAttribute('aria-label', Tr('Channel', 'Kanal'));
  for (const Item of Options) {
    const Option = CreateElement('option', '', Item.Label);
    Option.value = Item.Value;
    Select.appendChild(Option);
  }
  FitTimeChannelSelect(Select);
  const SelectedIndex = Options.findIndex(Item => Item.Value === ChannelId);
  if (SelectedIndex >= 0) Select.selectedIndex = SelectedIndex;
  Select.disabled = !IsMyTimer;
  return Select;
}

function AddCentralTime() {
  const SetKey = document.getElementById('TimeSetSelector').value;
  const Set = CentralTimeStore.Sets.find(Item => CentralSetKey(Item.Dev, Item.Type) === SetKey);
  if (!Set) return;
  const Kind = document.getElementById('TimeKindSelector').value;
  CentralTimeStore.Times = CollectCentralTimes();
  CentralTimeStore.Times.push({
    Id: NewTimeUuid(),
    Dev: Set.Dev,
    Type: Set.Type,
    Kind,
    Data: DefaultCentralTimeData(Set, Kind)
  });
  CentralTimesDirty = true;
  RenderCentralTimes();
}

function CollectCentralTimes() {
  const Entries = new Map();
  for (const Row of document.getElementById('TimesData').rows) {
    const Existing = CentralTimeStore.Times.find(Item => Item.Id === Row.dataset.id);
    const Set = FindCentralSet(Row.dataset.device, Row.dataset.type);
    if (!Existing || !Set) continue;
    const Editor = Row.querySelector('.TimeDataEditor');
    const Channel = Row.querySelector('.TimeChannelSelect')?.value;
    const Field = Key => Editor.querySelector(`[data-field="${Key}"]`);
    let Mask = Field('enabled').checked ? 128 : 0;
    for (const Day of Editor.querySelectorAll('[data-field="day"]')) if (Day.checked) Mask |= Number(Day.dataset.bit);
    let Data;
    if (Existing.Kind === 'FeedTime') Data = [Field('time').value, Mask, Number(Field('portion').value), ...Existing.Data.slice(3)];
    else if (Existing.Kind === 'Interval') Data = [Field('time').value, Field('period').value, Field('onTime').value, Mask, Channel ?? Existing.Data[4] ?? '', ...Existing.Data.slice(5)];
    else Data = {...Existing.Data,Time:Field('time').value,Days:Mask,Ch:Channel || Existing.Data.Ch,State:Field('active').checked};
    Entries.set(Row.dataset.id, {Id: Row.dataset.id, Dev: Row.dataset.device, Type: Row.dataset.type, Kind: Row.dataset.kind, Data});
  }
  return CentralTimeStore.Times.map(Entry => Entries.get(Entry.Id)).filter(Boolean);
}

async function SaveCentralTimes() {
  const Message = document.getElementById('TimeMessage');
  const Button = document.getElementById('TimeSaveButton');
  if (Button.disabled) return;
  Button.disabled = true;
  try {
    const Times = CollectCentralTimes();
    const Response = await fetch(new URL('api/times', window.location.href), {
      method: 'PUT', cache: 'no-store', headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({Ver: 1, Times}, null, 2)
    });
    const Text = await Response.text();
    if (!Response.ok) throw new Error(Text || `HTTP ${Response.status}`);
    const Result = JSON.parse(Text);
    if (Result.Ok !== true) throw new Error(Result.Err || 'Speichern fehlgeschlagen');
    const Reloaded = await LoadCentralTimes();
    const Notice = Result.Changed === true
      ? 'Geänderte Zeit-/Intervallsätze gespeichert; Übertragung wurde angestoßen.'
      : 'Keine Änderungen erkannt; offene Übertragungen wurden geprüft.';
    ShowSyncNotice(Message, Notice, Reloaded && CentralTimeStore.Sets.every(Set => Set.Synced), async () => {
      const Store = await FetchDashboardJson('api/times');
      if (!Array.isArray(Store.Sets)) throw new Error('Ungültiges Synchronisationsformat');
      CentralTimeStore = Store;
      UpdateTimeStatusIndicators(Store.Sets);
      return Store.Sets.every(Set => Set.Synced);
    });
  } catch (Error) {
    ShowSyncError(Message, `Speichern fehlgeschlagen: ${Error.message}`);
  } finally {
    Button.disabled = false;
  }
}

function RenderHeatProfileStatus(Store) {
  CentralHeatProfileStatus = Store;
  const Message = document.getElementById('HeatProfilesMessage');
  Message.replaceChildren(CreateElement('span', '', `${Tr('Revision', 'Revision')}: ${Store.Rev || '?'}`));
  const Label = Participant => {
    const Config = Devices.find(Device => Device.TechnicalName === Participant.Dev)?.Cfg;
    const Name = DeviceDisplayName(Participant.Dev);
    const Groups = [...new Set((Config?.GroupViews || [{Group: Config?.Group}])
      .map(View => View.Group).filter(Group => /^Aq\d+$/.test(Group)))];
    return [Name, ...Groups.filter(Group => !new RegExp(`\\b${Group}\\b`).test(Name))].join(' ');
  };
  const Synced = Store.Sets.filter(Set => Set.Synced);
  const Pending = Store.Sets.filter(Set => !Set.Synced);
  if (Synced.length) Message.append(' · ', CreateElement('span', 'TimeStatusSynced',
    `${Tr('Synced', 'synchron')}: ${Synced.map(Label).join(', ')}`));
  if (Pending.length) Message.append(CreateElement('div', 'HeatSyncPending',
    `${Tr('NotSynced', 'nicht synchron')}: ${Pending.map(Set =>
      `${Label(Set)}${Set.Err ? ': ' + Set.Err : ''}`).join(', ')}`));
  if (!Store.Sets.length) Message.append(' · ', Tr('NoParticipants', 'Noch keine Geräte synchronisiert'));
  Message.classList.remove('Success', 'Error');
  Message.classList.add('HeatSyncSummary');
  Message.hidden = false;
}

async function LoadCentralHeatProfiles(PreserveChannelDrafts = false) {
  const Message = document.getElementById('HeatProfilesMessage');
  if (!PreserveChannelDrafts) CentralChannelDrafts.clear();
  try {
    const Store = await FetchDashboardJson('api/heatprofiles');
    if (!Array.isArray(Store.Profs) || !Array.isArray(Store.Sets)) throw new Error('Ungültiges Format');
    CentralHeatProfilesSaved = {Profs: JSON.parse(JSON.stringify(Store.Profs)), TimeStamp: Store.TimeStamp};
    CentralHeatProfiles = {Profs: JSON.parse(JSON.stringify(Store.Profs)), TimeStamp: Store.TimeStamp};
    const TimeResponse = await fetch(new URL('api/times', window.location.href), {cache: 'no-store'});
    if (!TimeResponse.ok) throw new Error(`Times HTTP ${TimeResponse.status}`);
    CentralChannelStore = await TimeResponse.json();
    const AssignmentResponse = await fetch(new URL('api/device-assignments', window.location.href), {cache: 'no-store'});
    if (!AssignmentResponse.ok) throw new Error(`Device assignments HTTP ${AssignmentResponse.status}`);
    CentralProfileAssignmentView = await AssignmentResponse.json();
    RenderHeatProfiles();
    RenderChannelProfiles(PreserveChannelDrafts);
    RenderHeatProfileStatus(Store);
    return true;
  } catch (Error) {
    Message.textContent = `${Tr('HeatProfilesLoadFailed', 'Heizprofile konnten nicht geladen werden')}: ${Error.message}`;
    Message.classList.remove('HeatSyncSummary');
    Message.classList.remove('Success');
    Message.classList.remove('Error');
    Message.hidden = false;
    return false;
  }
}

async function SaveCentralHeatProfiles() {
  const Message = document.getElementById('HeatProfilesNotice');
  const Button = document.getElementById('HeatProfilesSaveButton');
  if (Button.disabled) return;
  CaptureChannelProfileDrafts();
  if (HasUnsavedChannelProfileDrafts()) {
    ShowSyncError(Message, Tr('SaveChannelsFirst', 'Bitte zuerst die Kanalzuordnungen speichern.'));
    return;
  }
  Button.disabled = true;
  try {
    const Profiles = CollectHeatProfiles();
    const Response = await fetch(new URL('api/heatprofiles', window.location.href), {
      method: 'PUT', cache: 'no-store', headers: {'Content-Type': 'application/json'},
      body: GnJson.Write('TJsonProfiles', {Profs: Profiles, ...(CentralHeatProfiles.TimeStamp ? {TimeStamp: CentralHeatProfiles.TimeStamp} : {})}, 2)
    });
    const Text = await Response.text();
    if (!Response.ok) throw new Error(Text || `HTTP ${Response.status}`);
    const Result = JSON.parse(Text);
    if (Result.Ok !== true) throw new Error(Result.Err || 'Speichern fehlgeschlagen');
    const Reloaded = await LoadCentralHeatProfiles(true);
    const Action = Result.Changed === true
      ? Tr('ProfilesSaved', 'Heizprofile gespeichert; Verteilung wurde angestoßen.')
      : Tr('ProfilesUnchanged', 'Keine Profiländerung erkannt; es wurde nichts verteilt.');
    ShowSyncNotice(Message, Action, Reloaded && CentralHeatProfileStatus.Sets.every(Set => Set.Synced), async () => {
      const Store = await FetchDashboardJson('api/heatprofiles');
      if (!Array.isArray(Store.Sets)) throw new Error('Ungültiges Synchronisationsformat');
      RenderHeatProfileStatus(Store);
      return Store.Sets.every(Set => Set.Synced);
    });
  } catch (Error) {
    ShowSyncError(Message, `${Tr('SaveFailed', 'Speichern fehlgeschlagen')}: ${Error.message}`);
  } finally {
    Button.disabled = false;
  }
}

function ProfileReferences(ProfileId) {
  const References = [];
  for (const [Device, State] of Object.entries(CentralChannelStore.Chs || {})) {
    const Rows = Array.isArray(State) ? State : State?.Rows;
    for (const Row of Rows || [])
      if (Row.Prof === ProfileId) References.push(`${Device} / ${Row.Id || '?'}`);
  }
  return References;
}

function RenderHeatProfiles() {
  const Cards = CentralHeatProfiles.Profs.map((Profile, Index) => {
    const Card = CreateElement('fieldset', 'HeatProfileCard');
    const Legend = CreateElement('legend', '', Profile.ProfName || `${Tr('Profile', 'Profil')} ${Index + 1}`);
    Card.appendChild(Legend);
    const AddField = (Parent, Schema, Key, Controller = false, LabelKey = null) => {
      const Field = GnJson.Describe(Schema).Fields.find(Item => Item.Name === Key);
      const Label = CreateElement('label', 'HeatProfileField');
      const TranslationKey = LabelKey || (Controller ? 'Pi.' : '') + Key;
      const LabelText = CreateElement('span', '', Tr(TranslationKey, Field.Description || Key));
      LabelText.dataset.tr = TranslationKey;
      Label.appendChild(LabelText);
      const Input = document.createElement('input');
      const Values = Controller ? Profile.Heat || {} : Profile;
      Input.type = Field.Type === 'bool' ? 'checkbox' : /^(double|uint)/.test(Field.Type) ? 'number' : 'text';
      Input.dataset[Controller ? 'piKey' : 'key'] = Key;
      if (Input.type === 'checkbox') Input.checked = Values[Key] === true;
      else Input.value = Values[Key] ?? '';
      if (Input.type === 'number') Input.step = Field.Type === 'double' ? 'any' : '1';
      Label.appendChild(Input); Parent.appendChild(Label);
      return Input;
    };
    const Name = AddField(Card, 'TJsonProfile', 'ProfName');
    Name.addEventListener('input', () => { Legend.textContent = Name.value; });
    const AddSelect = (Parent, Key, LabelKey, Options, Value) => {
      const Label = CreateElement('label', 'HeatProfileField');
      const LabelText = CreateElement('span', '', Tr(LabelKey));
      LabelText.dataset.tr = LabelKey;
      Label.appendChild(LabelText);
      const Select = document.createElement('select');
      Select.dataset.key = Key;
      for (const Item of Options) {
        const Option = document.createElement('option');
        Option.value = Item.Value;
        Option.textContent = Item.Label;
        if (Item.TrKey) Option.dataset.tr = Item.TrKey;
        Select.appendChild(Option);
      }
      Select.value = Value ?? '';
      Label.appendChild(Select); Parent.appendChild(Label);
      return Select;
    };
    const Measurement = AddSelect(Card, 'Meas', 'ProfileType', [
      {Value: 'Temperature', Label: Tr('Temperature', 'Temperature'), TrKey: 'Temperature'},
      {Value: 'BatteryPercent', Label: Tr('BatteryPercent', 'Battery percentage'), TrKey: 'BatteryPercent'}
    ], Profile.Meas || 'Temperature');
    const Source = AddSelect(Card, 'MeasSrc', 'MeasSrc', [], '');
    if (Profile.MeasSrc) {
      const CurrentSource = document.createElement('option');
      CurrentSource.value = Profile.MeasSrc;
      CurrentSource.textContent = Profile.MeasSrc;
      Source.appendChild(CurrentSource);
      Source.value = Profile.MeasSrc;
    }
    Source.dataset.profileSource = 'true';
    const UpdateSources = (KeepSelection = true) => {
      const Previous = KeepSelection ? Source.value : '';
      const Options = [{Value: '', Label: Tr('SelectSource', 'Select source')}];
      if (Measurement.value === 'BatteryPercent') {
        for (const Device of Devices) {
          if (!Device.Cfg?.ExternalSourceId) continue;
          const Id = Device.Cfg.ExternalSourceId;
          Options.push({Value: Id, Label: EndpointDirectory.IdToLabel(Id,
            Tr('BatteryPercent', 'Battery percentage'), EditorDeviceLabel(Device.TechnicalName))});
        }
      } else {
        for (const Device of CentralProfileAssignmentView.Devices || []) {
          if (Number(Device.ServiceId) !== 3 || Number(Device.SensorCapabilityId) !== 2 || !Device.DevId) continue;
          const Endpoint = (CentralProfileAssignmentView.Endpoints || []).find(Item =>
            (Item.OutAddress || '').split('/')[0] === Device.DevId && Number(Item.Caps) === 2);
          const Function = Endpoint?.Func || Device.Func || Device.HwName || Device.DevId;
          const Label = AssignmentFunctionLabel(Function, Endpoint?.Groups || [], Endpoint?.UseHwName ? Endpoint.HwName : null);
          Options.push({Value: Device.DevId, Label: EndpointDirectory.IdToLabel(Device.DevId,
            Tr('Temperature', 'Temperature'), Label)});
        }
      }
      Source.replaceChildren(...Options.map(Item => {
        const Option = document.createElement('option');
        Option.value = Item.Value;
        Option.textContent = Item.Label;
        return Option;
      }));
      Source.value = Options.some(Item => Item.Value === Previous) ? Previous : '';
      if (Previous && !Source.value) {
        const Existing = document.createElement('option');
        Existing.value = Previous;
        Existing.textContent = `${Tr('UnavailableSource', 'Unavailable source')} (${Previous})`;
        Existing.disabled = true;
        Source.appendChild(Existing);
        Source.value = Previous;
      }
    };
    Source.UpdateOptions = UpdateSources;
    UpdateSources();
    AddField(Card, 'TJsonProfile', 'OnIfNoMeas', false, 'OnWithoutValue');
    const Charge = CreateElement('div', 'ChargeProfileFields'); Card.appendChild(Charge);
    AddField(Charge, 'TJsonProfile', 'ChargeStartPct');
    AddField(Charge, 'TJsonProfile', 'ChargeStopPct');
    const RunOn = AddField(Charge, 'TJsonProfile', 'ChargeRunOn');
    const Factor = AddField(Charge, 'TJsonProfile', 'ChargeSecPerPct');
    Factor.min = '1'; Factor.max = '4294967295'; Factor.step = '1';
    const Heat = CreateElement('div', 'HeatControllerFields'); Card.appendChild(Heat);
    const Active = AddField(Heat, 'TJsonHeatConfig', 'Active', true);
    const Parameters = CreateElement('div', 'HeatControllerParameters'); Heat.appendChild(Parameters);
    AddField(Parameters, 'TJsonHeatConfig', 'Setpoint', true);
    const Advanced = CreateElement('details', 'PiAdvanced');
    Advanced.appendChild(CreateElement('summary', '', Tr('PiParameters', 'PI-Parameter und Gerätezuordnung')));
    for (const Key of ['FloorW', 'MainMaxW', 'CapacityWhPerK', 'LossWPerK', 'Kp', 'TiSec', 'WindowSec', 'DelaySec'])
      AddField(Advanced, 'TJsonHeatConfig', Key, true);
    const AddEndpoint = Key => {
      const Label = CreateElement('label', 'HeatProfileField');
      const LabelText = CreateElement('span', '', Tr(`Pi.${Key}`));
      LabelText.dataset.tr = `Pi.${Key}`;
      Label.appendChild(LabelText);
      const Value = document.createElement('input');
      Value.type = 'text'; Value.readOnly = true;
      Value.dataset.endpointId = Profile.Heat?.[Key] || '';
      Value.value = EndpointDirectory.IdToLabel(Value.dataset.endpointId);
      Value.setAttribute('aria-readonly', 'true');
      Value.dataset.derivedEndpoint = Key;
      Value.dataset.piKey = Key;
      Label.appendChild(Value); Advanced.appendChild(Label);
    };
    AddEndpoint('MainEndpoint');
    AddEndpoint('FloorEndpoint');
    Parameters.appendChild(Advanced);
    const Refresh = () => {
      const Battery = Measurement.value.trim() === 'BatteryPercent';
      Charge.hidden = !Battery; Heat.hidden = Battery; Parameters.hidden = !Active.checked;
      Factor.parentElement.hidden = !RunOn.checked; Factor.disabled = !Battery || !RunOn.checked;
    };
    Measurement.addEventListener('change', () => { UpdateSources(false); Refresh(); });
    for (const Input of [RunOn, Active]) Input.addEventListener('input', Refresh);
    Refresh();
    const References = ProfileReferences(Profile.ProfName);
    const Delete = CreateElement('button', 'SecondaryButton CompactButton TrashButton HeatProfileDelete');
    Delete.innerHTML = ActionIcon('Trash');
    Delete.type = 'button'; Delete.disabled = References.length > 0;
    Delete.dataset.trTitle = 'Delete'; Delete.title = References.join(', ') || Tr('Delete');
    Delete.setAttribute('aria-label', Tr('Delete'));
    Delete.onclick = () => { CentralHeatProfiles.Profs.splice(Index, 1); RenderHeatProfiles(); };
    const Actions = CreateElement('div', 'HeatProfileActions');
    Actions.appendChild(Delete);
    Card.appendChild(Actions);
    return Card;
  });
  document.getElementById('HeatProfilesEditor').replaceChildren(...Cards);
}

function ChannelDraftKey(Device, Index) {
  return `${Device}\n${Index}`;
}

function CaptureChannelProfileDrafts() {
  for (const Row of document.querySelectorAll('.ChannelProfileRow'))
    CentralChannelDrafts.set(ChannelDraftKey(Row.dataset.device, Row.dataset.index), {
      Role: Row.querySelector('[data-field="role"]').value,
      Prof: Row.querySelector('[data-field="profile"]').value
    });
}

function HasUnsavedChannelProfileDrafts() {
  for (const [Device, State] of Object.entries(CentralChannelStore.Chs || {})) {
    const Channels = Array.isArray(State) ? State : State?.Rows || [];
    for (const [Index, Channel] of Channels.entries()) {
      const Draft = CentralChannelDrafts.get(ChannelDraftKey(Device, Index));
      if (!Draft) continue;
      if (Draft.Deleted) return true;
      const SavedRole = Channel.Role === 'HeaterMain' ? 'MainSw' : Channel.Role || '';
      const DraftRole = Draft.Role === 'HeaterMain' ? 'MainSw' : Draft.Role || '';
      const SavedProfile = ['MainSw', 'HeaterMain', 'HeaterFloor'].includes(SavedRole) ? Channel.Prof || '' : '';
      const DraftProfile = ['MainSw', 'HeaterMain', 'HeaterFloor'].includes(DraftRole) ? Draft.Prof || '' : '';
      if (SavedRole !== DraftRole || SavedProfile !== DraftProfile) return true;
    }
  }
  return false;
}

function UpdateDerivedProfileEndpoints() {
  CaptureChannelProfileDrafts();
  const Assignments = new Map();
  for (const [Device, State] of Object.entries(CentralChannelStore.Chs || {})) {
    const Channels = Array.isArray(State) ? State : State?.Rows || [];
    Channels.forEach((Channel, Index) => {
      const Draft = CentralChannelDrafts.get(ChannelDraftKey(Device, Index));
      if (Draft?.Deleted) return;
      const Role = Draft?.Role ?? Channel.Role ?? '';
      const ProfileName = Draft?.Prof ?? Channel.Prof ?? '';
      if (!ProfileName || !['MainSw', 'HeaterMain', 'HeaterFloor'].includes(Role)) return;
      const Endpoint = (CentralProfileAssignmentView.Endpoints || []).find(Item =>
        Item.DevName === Device && Number(Item.Index) === Index && Number(Item.Caps) === 1);
      if (!Endpoint?.OutAddress) return;
      const ProfileAssignments = Assignments.get(ProfileName) || {Main: [], Floor: []};
      ProfileAssignments[Role === 'HeaterFloor' ? 'Floor' : 'Main'].push(Endpoint.OutAddress);
      Assignments.set(ProfileName, ProfileAssignments);
    });
  }

  CentralHeatProfiles.Profs.forEach((Profile, Index) => {
    if (Profile.Meas === 'BatteryPercent' || !Profile.Heat) return;
    const ProfileAssignments = Assignments.get(Profile.ProfName) || {Main: [], Floor: []};
    const MainEndpoint = ProfileAssignments.Main.length === 1 ? ProfileAssignments.Main[0] : '';
    const FloorEndpoint = ProfileAssignments.Floor.length === 1 ? ProfileAssignments.Floor[0] : '';
    Profile.Heat.MainEndpoint = MainEndpoint;
    Profile.Heat.FloorEndpoint = FloorEndpoint;
    const Card = document.querySelectorAll('.HeatProfileCard')[Index];
    for (const [Key, Value] of [['MainEndpoint', MainEndpoint], ['FloorEndpoint', FloorEndpoint]]) {
      const Field = Card?.querySelector(`[data-derived-endpoint="${Key}"]`);
      if (Field) {
        Field.dataset.endpointId = Value;
        Field.value = EndpointDirectory.IdToLabel(Value);
      }
    }
  });
}

function UpdateChannelAssignmentOptions() {
  const Selector = document.getElementById('ChannelAssignmentSelector');
  if (!Selector) return;
  const Options = [{Value: '', Label: Tr('SelectChannel', 'Kanal auswählen')}];
  for (const [Device, State] of Object.entries(CentralChannelStore.Chs || {})) {
    const ChannelRows = Array.isArray(State) ? State : State?.Rows || [];
    ChannelRows.forEach((Channel, Index) => {
      const Draft = CentralChannelDrafts.get(ChannelDraftKey(Device, Index));
      const Role = Draft?.Deleted ? '' : (Draft?.Role || Channel.Role || '');
      if (['MainSw', 'HeaterMain', 'HeaterFloor'].includes(Role)) return;
      Options.push({
        Value: ChannelDraftKey(Device, Index),
        Label: EditorDeviceLabel(Device) === EditorChannelLabel(Channel, Device)
          ? EditorDeviceLabel(Device) : `${EditorDeviceLabel(Device)} · ${EditorChannelLabel(Channel, Device)}`
      });
    });
  }
  Selector.replaceChildren(...Options.map(Item => {
    const Option = document.createElement('option');
    Option.value = Item.Value;
    Option.textContent = Item.Label;
    return Option;
  }));
  document.getElementById('ChannelAssignmentAddButton').disabled = Options.length < 2;
}

function RenderChannelProfiles(PreserveDrafts = false) {
  const Editor = document.getElementById('ChannelProfilesEditor');
  const Rows = [];
  if (PreserveDrafts) CaptureChannelProfileDrafts();
  const Header = CreateElement('div', 'ChannelProfilesHeader');
  for (const Label of ['Dev', 'Channel', 'Role', 'HeatProfile'])
    Header.appendChild(CreateElement('strong', '', Tr(Label, Label)));
  Header.appendChild(CreateElement('span', '', ''));
  Rows.push(Header);
  for (const [Device, State] of Object.entries(CentralChannelStore.Chs || {})) {
    const ChannelRows = Array.isArray(State) ? State : State?.Rows || [];
    ChannelRows.forEach((Channel, Index) => {
      const Key = ChannelDraftKey(Device, Index);
      const Draft = CentralChannelDrafts.get(Key);
      if (Draft?.Deleted) return;
      const Role = Draft?.Role || Channel.Role;
      if (!['MainSw', 'HeaterMain', 'HeaterFloor'].includes(Role)) return;
      const Row = document.createElement('div');
      Row.className = 'ChannelProfileRow';
      Row.dataset.device = Device;
      Row.dataset.index = String(Index);
      Row.append(
        Object.assign(CreateElement('span', 'ChannelProfileDevice', EditorDeviceLabel(Device)), {title: Tr('Device', 'Device')}),
        CreateElement('span', 'ChannelProfileName', EditorChannelLabel(Channel, Device))
      );
      const RoleOptions = ['MainSw', 'HeaterFloor'].map(Item => ({Value: Item, Label: Tr(`Role.${Item}`, Item)}));
      const RoleSelect = AddTimeSelect(Row, 'role', Tr('Role', 'Funktion'), RoleOptions, Role === 'HeaterMain' ? 'MainSw' : Role);
      RoleSelect.setAttribute('aria-label', Tr('Role', 'Funktion'));
      const Options = [{Value: '', Label: Tr('SelectProfile', 'Profil auswählen')}];
      for (const Profile of CentralHeatProfiles.Profs)
        Options.push({Value: Profile.ProfName, Label: Profile.ProfName});
      const ProfileSelect = AddTimeSelect(Row, 'profile', Tr('HeatProfile', 'Heizprofil'), Options, Draft?.Prof ?? Channel.Prof ?? '');
      ProfileSelect.setAttribute('aria-label', Tr('HeatProfile', 'Heizprofil'));
      const UpdateDraft = () => {
        CaptureChannelProfileDrafts();
        UpdateDerivedProfileEndpoints();
        document.getElementById('HeatProfilesSaveButton').disabled = HasUnsavedChannelProfileDrafts();
      };
      RoleSelect.addEventListener('change', UpdateDraft);
      ProfileSelect.addEventListener('change', UpdateDraft);
      const Delete = CreateElement('button', 'SecondaryButton CompactButton TrashButton ChannelProfileDelete');
      Delete.type = 'button'; Delete.innerHTML = ActionIcon('Trash');
      Delete.dataset.trTitle = 'Delete'; Delete.title = Tr('Delete'); Delete.setAttribute('aria-label', Tr('Delete'));
      Delete.addEventListener('click', () => {
        CaptureChannelProfileDrafts();
        CentralChannelDrafts.set(Key, {Deleted: true});
        RenderChannelProfiles();
      });
      Row.children[0].dataset.label = Tr('Device', 'Device');
      Row.children[1].dataset.label = Tr('Channel', 'Channel');
      Row.appendChild(Delete);
      Rows.push(Row);
    });
  }
  Editor.replaceChildren(...Rows);
  UpdateChannelAssignmentOptions();
  UpdateDerivedProfileEndpoints();
  document.getElementById('HeatProfilesSaveButton').disabled = HasUnsavedChannelProfileDrafts();
}

function AddChannelProfileAssignment() {
  CaptureChannelProfileDrafts();
  const Selector = document.getElementById('ChannelAssignmentSelector');
  if (!Selector.value) return;
  const [Device, IndexText] = Selector.value.split('\n');
  const Index = Number(IndexText);
  CentralChannelDrafts.set(ChannelDraftKey(Device, Index), {
    Role: 'MainSw',
    Prof: CentralHeatProfiles.Profs[0]?.ProfName || ''
  });
  RenderChannelProfiles();
}

async function SaveChannelProfiles() {
  const Button = document.getElementById('ChannelProfilesSaveButton');
  const Message = document.getElementById('HeatProfilesNotice');
  if (Button.disabled) return;
  Button.disabled = true;
  try {
    CaptureChannelProfileDrafts();
    const Channels = JSON.parse(JSON.stringify(CentralChannelStore.Chs || {}));
    for (const [Device, State] of Object.entries(Channels)) {
      const ChannelRows = Array.isArray(State) ? State : State.Rows;
      ChannelRows.forEach((Channel, Index) => {
        const Draft = CentralChannelDrafts.get(ChannelDraftKey(Device, Index));
        if (!Draft || Draft.Deleted) return;
        Channel.Role = Draft.Role;
        if (['MainSw', 'HeaterMain', 'HeaterFloor'].includes(Draft.Role)) {
          if (!Draft.Prof) throw new Error(`${DeviceDisplayName(Device)} / ${ChannelDisplayName(Channel, Device)}: ${Tr('ProfileRequired', 'Heizprofil erforderlich')}`);
          Channel.Prof = Draft.Prof;
        } else delete Channel.Prof;
      });
      State.Rows = ChannelRows.filter((Channel, Index) => !CentralChannelDrafts.get(ChannelDraftKey(Device, Index))?.Deleted);
    }
    UpdateDerivedProfileEndpoints();
    const Response = await fetch(new URL('api/times', window.location.href), {
      method: 'PUT', cache: 'no-store', headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({Ver: 1, Times: CollectCentralTimes(), Chs: Channels,
        Profs: ChannelSaveProfiles(), ProfileTimeStamp: CentralHeatProfilesSaved.TimeStamp}, null, 2)
    });
    const Text = await Response.text();
    if (!Response.ok) throw new Error(Text || `HTTP ${Response.status}`);
    const Result = JSON.parse(Text);
    if (Result.Ok !== true) throw new Error(Result.Err || Tr('SaveFailed', 'Speichern fehlgeschlagen'));
    CentralChannelDrafts.clear();
    const TimesReloaded = await LoadCentralTimes();
    CentralChannelStore = CentralTimeStore;
    const ProfileStore = await FetchDashboardJson('api/heatprofiles');
    if (!Array.isArray(ProfileStore.Profs) || !Array.isArray(ProfileStore.Sets)) throw new Error('Ungültiges Profil-Synchronisationsformat');
    CentralHeatProfilesSaved = {Profs: JSON.parse(JSON.stringify(ProfileStore.Profs)), TimeStamp: ProfileStore.TimeStamp};
    CentralHeatProfiles = {Profs: JSON.parse(JSON.stringify(ProfileStore.Profs)), TimeStamp: ProfileStore.TimeStamp};
    RenderHeatProfiles();
    RenderChannelProfiles();
    RenderHeatProfileStatus(ProfileStore);
    const Action = Result.Changed === true
      ? Tr('ChannelsSaved', 'Kanalzuordnungen gespeichert; Verteilung wurde angestoßen.')
      : Tr('ChannelsUnchanged', 'Keine Kanaländerung erkannt; es wurde nichts verteilt.');
    const IsSynced = TimesReloaded && CentralTimeStore.Sets.every(Set => Set.Synced) && ProfileStore.Sets.every(Set => Set.Synced);
    ShowSyncNotice(Message, Action, IsSynced, RefreshDashboardSyncStatus);
  } catch (Error) {
    ShowSyncError(Message, `${Tr('SaveFailed', 'Speichern fehlgeschlagen')}: ${Error.message}`);
  } finally {
    Button.disabled = false;
  }
}

function ChannelSaveProfiles() {
  const Profiles = JSON.parse(JSON.stringify(CentralHeatProfilesSaved.Profs));
  const Drafts = new Map(CentralHeatProfiles.Profs.map(Profile => [Profile.ProfName, Profile]));
  for (const Profile of Profiles) {
    const Draft = Drafts.get(Profile.ProfName);
    if (!Profile.Heat || !Draft?.Heat || Profile.Meas === 'BatteryPercent') continue;
    for (const Key of ['MainEndpoint', 'FloorEndpoint']) {
      if (Draft.Heat[Key] === undefined) delete Profile.Heat[Key];
      else Profile.Heat[Key] = Draft.Heat[Key];
    }
  }
  return Profiles;
}

function AddHeatProfile() {
  CentralHeatProfiles.Profs.push({ProfName: '', MeasSrc: '', Meas: 'Temperature', OnIfNoMeas: false, Heat: {Active: false}});
  RenderHeatProfiles();
  RenderChannelProfiles(true);
}

function CollectHeatProfiles() {
  const Profiles = [], Names = new Set();
  for (const [Index, Card] of [...document.querySelectorAll('.HeatProfileCard')].entries()) {
    const Profile = {...CentralHeatProfiles.Profs[Index]};
    const ReadInput = Input => {
      if (Input.type === 'checkbox') return Input.checked;
      if (Input.type !== 'number') return Input.value.trim();
      if (!Input.value.trim()) return undefined;
      if (!Number.isFinite(Number(Input.value))) throw new Error(Tr('InvalidNumber', 'ungültige Zahl'));
      return Number(Input.value);
    };
    for (const Input of Card.querySelectorAll('[data-key]')) {
      const Value = ReadInput(Input), Key = Input.dataset.key;
      if (Value === undefined) delete Profile[Key]; else Profile[Key] = Value;
    }
    if (Profile.Meas === 'BatteryPercent') delete Profile.Heat;
    else {
      Profile.Heat = {...Profile.Heat};
      for (const Input of Card.querySelectorAll('[data-pi-key]')) {
        const Value = Input.dataset.endpointId !== undefined
          ? (EndpointDirectory.NameToId(Input.value) || Input.dataset.endpointId)
          : ReadInput(Input);
        const Key = Input.dataset.piKey;
        if (Value === undefined) delete Profile.Heat[Key]; else Profile.Heat[Key] = Value;
      }
      for (const Key of ['ChargeStartPct', 'ChargeStopPct', 'ChargeRunOn', 'ChargeSecPerPct']) delete Profile[Key];
    }
    if (!GnJson.Valid('TJsonProfile', Profile)) throw new Error(`${Tr('Profile', 'Profil')} ${Index + 1}: ${Tr('Invalid', 'ungültig')}`);
    if (Names.has(Profile.ProfName)) throw new Error(`${Profile.ProfName}: ${Tr('Duplicate', 'doppelt')}`);
    Names.add(Profile.ProfName); Profiles.push(Profile);
  }
  for (const Previous of CentralHeatProfiles.Profs)
    if (!Names.has(Previous.ProfName) && ProfileReferences(Previous.ProfName).length)
      throw new Error(`${Previous.ProfName}: ${Tr('ProfileInUse', 'Noch verwendet')}`);
  return Profiles;
}

function ActivateTab(TabId, Persist = true) {
  if (TabId === 'Assignments') LoadAssignments();
  else { clearTimeout(AssignmentTimer); AssignmentTimer = null; }
  const Device = Devices.find(Item => Item.ConfigTabId === TabId);
  if (Device?.ConfigSelectorOption.disabled) return;

  document.querySelectorAll('.TabPage').forEach(Page => {
    Page.classList.toggle('Active', Page.id === TabId);
  });
  document.getElementById('PageSelector').value = TabId;
  ScheduleOverviewFit();
  if (Persist) StoreDashboardTabCookie(TabId);

  ActiveConfigDevice = Device || null;
  const IsTimes = TabId === 'Times';
  const IsHeatProfiles = TabId === 'HeatProfiles';
  document.getElementById('MainContent').classList.toggle('ConfigMode', ActiveConfigDevice !== null);
  const TabTools = document.getElementById('TabTools');
  TabTools.hidden = ActiveConfigDevice === null;
  if (ActiveConfigDevice === null) {
    if (IsTimes) LoadCentralTimes();
    if (IsHeatProfiles) LoadCentralHeatProfiles();
    return;
  }

  document.getElementById('FrameScaleLabel').textContent = 'Auto';
  document.getElementById('FrameReloadButton').disabled = ActiveConfigDevice.ConfigFrame === null;
  LoadConfigurationFrame(ActiveConfigDevice);
  ActiveConfigDevice.ConfigFrame?.contentWindow?.postMessage({Src: 'GnDashboard', Type: 'ConfigurationActivated'}, '*');
  requestAnimationFrame(() => FitFrame(ActiveConfigDevice));
}

function RestoreDashboardTab() {
  const StoredTabId = ReadDashboardTabCookie();
  const StoredTab = document.getElementById(StoredTabId);
  const StoredOption = [...document.getElementById('PageSelector').options]
    .find(Option => Option.value === StoredTabId);
  ActivateTab(StoredTab?.classList.contains('TabPage') && StoredOption && !StoredOption.disabled
    ? StoredTabId : 'Overview', false);
}

function RefitActiveFrame() {
  const Device = Devices.find(Item => Item.ConfigPage?.classList.contains('Active'));
  if (Device !== undefined)
    requestAnimationFrame(() => FitFrame(Device));
}

function SetDashboardStatus(Text) {
  document.getElementById('LastDashboardUpdate').textContent = Text;
}

function RenderDashboardTimestamp() {
  SetDashboardStatus(LastDashboardUpdateAt ? `${Tr('Updated', 'Aktualisiert')} ${FormatShortTime(LastDashboardUpdateAt)}` : Tr('NoDataYet', 'Noch keine Daten'));
}

function UpdateDashboardTimestamp() {
  LastDashboardUpdateAt = new Date();
  RenderDashboardTimestamp();
}

function CheckOfflineDevices() {
  const Now = Date.now();
  for (const Device of Devices) {
    if (Device.LastStatusAt === null || Device.UdpConnected) continue;
    if (Now - Device.LastStatusAt.getTime() > 20000)
      SetConnectionState(Device, 'Offline');
  }
}

async function FetchDashboardConfiguration() {
  const Url = new URL('Devices.json', window.location.href);
  const Controller = new AbortController();
  const Timeout = setTimeout(() => Controller.abort(), DashboardRequestTimeoutMilliseconds);
  Url.searchParams.set('_', Date.now().toString());
  try {
    const Response = await fetch(Url, {
      cache: 'no-store',
      signal: Controller.signal
    });
    if (!Response.ok)
      throw new Error(`Devices.json: HTTP ${Response.status}`);
    return Response.json();
  } catch (Error) {
    if (Error.name === 'AbortError')
      throw new Error(`Zeitüberschreitung nach ${DashboardRequestTimeoutMilliseconds / 1000} Sekunden`);
    throw Error;
  } finally {
    clearTimeout(Timeout);
  }
}

async function FetchDashboardServerVersion() {
  const Url = new URL(ServerVersionApiPath, window.location.href);
  const Controller = new AbortController();
  const Timeout = setTimeout(() => Controller.abort(), DashboardRequestTimeoutMilliseconds);
  Url.searchParams.set('_', Date.now().toString());
  try {
    const Response = await fetch(Url, {
      cache: 'no-store',
      signal: Controller.signal
    });
    if (!Response.ok)
      throw new Error(`Server-Version: HTTP ${Response.status}`);
    const VersionInfo = await Response.json();
    return {
      Server: VersionInfo.ServerVersion || '?',
      Data: VersionInfo.DataVer || '?'
    };
  } catch (Error) {
    if (Error.name === 'AbortError')
      throw new Error(`Zeitüberschreitung nach ${DashboardRequestTimeoutMilliseconds / 1000} Sekunden`);
    throw Error;
  } finally {
    clearTimeout(Timeout);
  }
}

async function LoadDashboardConfiguration() {
  const [Configuration, VersionInfo] = await Promise.all([
    FetchDashboardConfiguration(),
    FetchDashboardServerVersion()
  ]);
  document.getElementById('DashboardTitle').textContent = Tr('DashboardTitle', Configuration.Name || 'Aquarium Dashboard');
  document.getElementById('DashboardVersionLine').textContent = `Server ${VersionInfo.Server} · Data ${VersionInfo.Data}`;
  OverviewOutputOrder = Configuration.OverviewOutputOrder || {};
  SetDashboardStatus(Tr('NoDataYet', 'Noch keine Daten'));
  return Configuration;
}

function SetDashboardConnectionState(State, Message) {
  const Banner = document.getElementById('ConnectionBanner');
  const Text = document.getElementById('ConnectionMessage');
  Banner.className = `ConnectionBanner ${State}`;
  Banner.hidden = State === 'Online';
  Text.textContent = Message;
}

function ScheduleDashboardHealthCheck(Delay) {
  if (DashboardHealthTimer !== null)
    clearTimeout(DashboardHealthTimer);
  DashboardHealthTimer = setTimeout(() => {
    DashboardHealthTimer = null;
    CheckDashboardHealth();
  }, Delay);
}

function MarkDashboardUnavailable(Message) {
  DashboardWasUnavailable = true;
  SetDashboardConnectionState('Offline', Message);
  ScheduleDashboardHealthCheck(DashboardHealthDelay);
  DashboardHealthDelay = Math.min(DashboardHealthDelay * 2, DashboardHealthMaximumMilliseconds);
}

async function CheckDashboardHealth() {
  if (DashboardHealthRequestRunning) return;

  if (!navigator.onLine) {
    MarkDashboardUnavailable(Tr('NetworkInterrupted'));
    return;
  }

  DashboardHealthRequestRunning = true;
  try {
    await FetchDashboardConfiguration();
    DashboardHealthDelay = DashboardHealthInitialMilliseconds;

    if (DashboardWasUnavailable) {
      SetDashboardConnectionState('Recovering', Tr('DashboardRecovering'));
      setTimeout(() => window.location.reload(), DashboardRecoveryReloadMilliseconds);
      return;
    }

    SetDashboardConnectionState('Online', '');
    ScheduleDashboardHealthCheck(DashboardHealthNormalMilliseconds);
  } catch (Error) {
    console.error('Dashboard health check failed', Error);
    MarkDashboardUnavailable(`${Tr('DashboardUnavailable')}: ${Error.message}. ${Tr('RetryAutomatic')}`);
  } finally {
    DashboardHealthRequestRunning = false;
  }
}

function HandleBrowserOffline() {
  for (const Device of Devices)
    SetConnectionState(Device, 'Offline');
  MarkDashboardUnavailable(Tr('NetworkInterrupted'));
}

function HandleBrowserOnline() {
  DashboardHealthDelay = DashboardHealthInitialMilliseconds;
  ScheduleDashboardHealthCheck(0);
}

async function StartDashboard() {
  const Message = document.getElementById('DashboardMessage');
  try {
    await LoadTranslations();
    LRestore();
    RenewFeedbackSoundCookie();
    setInterval(RenewFeedbackSoundCookie, 24 * 60 * 60 * 1000);
    // Do not await: a blocked AudioContext.resume() can remain pending until a tap.
    if (ReadFeedbackSoundCookie())
      FeedbackAlarm.Enable(false).catch(() => FeedbackAlarm.OnChanged());
    const Configuration = await LoadDashboardConfiguration();
    Message.hidden = true;
    const DeviceConfigurations = (Configuration.Devices || []).filter(Device => Device.Enabled !== false);
    for (const [Name, Id] of Object.entries(Configuration.ExternalSrcs || {}))
      DeviceConfigurations.push({Name, Func: Name, BaseUrl: '/', Group: 'System',
        StatusOnly: true, ExternalSourceId: Id});
    if (DeviceConfigurations.length === 0) {
      Message.textContent = 'In Devices.json sind keine aktiven Geräte eingetragen.';
      Message.hidden = false;
      RestoreDashboardTab();
      return;
    }
    for (const DeviceConfiguration of DeviceConfigurations) {
      if (!DeviceConfiguration.BaseUrl)
        continue;
      const Device = new TDashboardDevice(DeviceConfiguration);
      Devices.push(Device);
      Device.Start();
    }
    RestoreDashboardTab();
    ScheduleDashboardHealthCheck(DashboardHealthNormalMilliseconds);
  } catch (Error) {
    console.error('Cannot start dashboard', Error);
    Message.textContent = `Dashboard-Konfiguration konnte nicht geladen werden: ${Error.message}`;
    Message.hidden = false;
    MarkDashboardUnavailable(`${Tr('DashboardUnavailable')}: ${Error.message}. ${Tr('RetryAutomatic')}`);
  }
}

function ReloadDashboard() {
  window.location.reload();
}

function CheckDashboardNow() {
  DashboardHealthDelay = DashboardHealthInitialMilliseconds;
  ScheduleDashboardHealthCheck(0);
}

document.getElementById('PageSelector').addEventListener('change', Event => ActivateTab(Event.target.value));
document.getElementById('TimeSetSelector').addEventListener('change', UpdateTimeKindSelector);
document.getElementById('TimesData').addEventListener('input', () => { CentralTimesDirty = true; });
document.getElementById('TimesData').addEventListener('change', () => { CentralTimesDirty = true; });
document.getElementById('TimesData').addEventListener('click', Event => {
  if (Event.target.closest('.TimeOrderButtons')) CentralTimesDirty = true;
});
window.addEventListener('dashboard-times-changed', () => {
  ScheduleDashboardSyncStatusRefresh();
  if (!CentralTimesDirty && document.getElementById('Times').classList.contains('Active'))
    LoadCentralTimes();
});
document.getElementById('TimeAddButton').addEventListener('click', AddCentralTime);
document.getElementById('TimeSaveButton').addEventListener('click', SaveCentralTimes);
document.getElementById('TimeReloadButton').addEventListener('click', LoadCentralTimes);
document.querySelectorAll('.TableSortButton').forEach(Button => Button.addEventListener('click', () => SortCentralTimes(Button.dataset.sort, Button)));
document.getElementById('HeatProfilesSaveButton').addEventListener('click', SaveCentralHeatProfiles);
document.getElementById('HeatProfilesReloadButton').addEventListener('click', LoadCentralHeatProfiles);
document.getElementById('HeatProfileAddButton').addEventListener('click', AddHeatProfile);
document.getElementById('ChannelProfilesSaveButton').addEventListener('click', SaveChannelProfiles);
document.getElementById('ChannelAssignmentAddButton').addEventListener('click', AddChannelProfileAssignment);
document.getElementById('ReloadButton').addEventListener('click', ReloadDashboard);
document.getElementById('FrameReloadButton').addEventListener('click', () => {
  if (ActiveConfigDevice !== null)
    ReloadConfigurationFrame(ActiveConfigDevice);
});
document.getElementById('ConnectionRetryButton').addEventListener('click', CheckDashboardNow);
document.getElementById('DetailCloseButton').addEventListener('click', CloseChannelDetails);
document.getElementById('DetailOverlay').addEventListener('click', Event => {
  if (Event.target.id === 'DetailOverlay')
    CloseChannelDetails();
});
document.addEventListener('keydown', Event => {
  if (Event.key === 'Escape' && !document.getElementById('DetailOverlay').hidden)
    CloseChannelDetails();
});
window.addEventListener('message', HandleConfigurationMessage);
window.addEventListener('offline', HandleBrowserOffline);
window.addEventListener('online', HandleBrowserOnline);
window.addEventListener('resize', HandleDashboardResize);
window.addEventListener('orientationchange', HandleDashboardResize);
window.visualViewport?.addEventListener('resize', ScheduleOverviewFit);
if (typeof ResizeObserver !== 'undefined') {
  const OverviewChromeObserver = new ResizeObserver(ScheduleOverviewFit);
  for (const Element of [document.querySelector('.AppHeader'), document.getElementById('TabBar'),
    document.getElementById('ConnectionBanner'), document.getElementById('DashboardMessage')])
    OverviewChromeObserver.observe(Element);
}
document.addEventListener('visibilitychange', () => {
  if (!document.hidden) {
    CheckDashboardNow();
    RefitActiveFrame();
  }
});
setInterval(CheckOfflineDevices, OfflineCheckMilliseconds);
StartDashboard();


function EndpointDisplayName(Function) {
  return Tr(`Device.${Function}`, ChannelDisplayName({Name: Function}));
}

function AssignmentFunctionLabel(Function, Groups = [], HardwareName = null) {
  const Name = HardwareName || EndpointDisplayName(Function);
  const Suffix = [...new Set(Groups)].map(Group => /^Aq\d+$/.test(Group) ? Group : DeviceGroupDisplayName(Group))
    .filter(Group => !/^Aq\d+$/.test(Group) || !new RegExp(`\\b${Group}\\b`).test(Name));
  return [Name, ...Suffix].filter(Boolean).join(' · ');
}

function AssignmentChoices(View) {
  return [...(View?.Endpoints || []).filter(Endpoint => Endpoint.Caps !== 0).slice().sort((First, Second) => First.HwName.localeCompare(Second.HwName, undefined, {numeric: true})).map(Endpoint => ({
    ...Endpoint, Supported: Endpoint.Available !== false && (Endpoint.Caps === 2 || View.Devices.find(Device => Device.DevId === Endpoint.OutAddress?.split('/')[0])?.EndpointAssignments === 1), Value: Endpoint.OutAddress, Label: `${Endpoint.HwName} · ${AssignmentFunctionLabel(Endpoint.Func, Endpoint.Groups, Endpoint.UseHwName ? Endpoint.HwName : null)}`, Kind: 'Endpoint'
  })), ...(View?.Devices || []).filter(Device => Device.ServiceId === 3).map(Device => ({
    ...Device, Supported: Number.isInteger(Device.SensorCapabilityId) && Device.SensorCapabilityId >= 0, Value: `Device:${Device.Func}`, Label: `${Device.HwName || Device.Func} · ${Tr('FeederDeviceSwap', 'Feeder (whole device)')}`, Kind: 'Device'
  }))];
}

function AssignmentButtons() {
  const View = AssignmentView;
  const Choices = AssignmentChoices(View);
  const First = Choices.find(Item => Item.Value === document.getElementById('AssignmentFirst').value);
  const Second = Choices.find(Item => Item.Value === document.getElementById('AssignmentSecond').value);
  for (const Option of document.getElementById('AssignmentSecond').options) {
    const Candidate = Choices.find(Item => Item.Value === Option.value);
    Option.disabled = !First || !Candidate || First.Value === Candidate.Value || First.Kind !== Candidate.Kind ||
      (First.Kind === 'Endpoint' ? First.Caps !== Candidate.Caps || !Candidate.Supported : First.CapabilityId !== Candidate.CapabilityId || !Candidate.Supported);
  }
  const SafetyLabel = document.querySelector('#AssignmentDetached + span');
  SafetyLabel.dataset.tr = First?.Kind === 'Endpoint' && First.Caps === 2 ? 'SensorAssignmentsChecked' : 'ConsumersDetached';
  SafetyLabel.textContent = Tr(SafetyLabel.dataset.tr);
  document.getElementById('AssignmentResume').hidden = !View?.Pending;
  document.getElementById('AssignmentResume').disabled = AssignmentLoading;
  document.getElementById('AssignmentSwap').disabled = !View || !View.AllApplied || View.Pending || AssignmentLoading ||
    !First || !Second || First === Second || First.Kind !== Second.Kind ||
    (First.Kind === 'Endpoint' ? First.Caps !== Second.Caps || !First.Caps || !First.Supported || !Second.Supported : First.ServiceId !== Second.ServiceId || First.CapabilityId !== Second.CapabilityId || !First.Supported || !Second.Supported) ||
    !document.getElementById('AssignmentDetached').checked;
}

async function LoadAssignments() {
  if (AssignmentLoading) return;
  AssignmentLoading = true;
  clearTimeout(AssignmentTimer);
  AssignmentButtons();
  const Message = document.getElementById('AssignmentMessage');
  try {
    const Response = await fetch('api/device-assignments', {cache: 'no-store', signal: AbortSignal.timeout(10000)});
    if (!Response.ok) throw new Error(`HTTP ${Response.status}`);
    const View = await Response.json();
    AssignmentView = View;
    document.getElementById('AssignmentMode').textContent = 'MAC';
    const Rows = [...View.Devices.filter(Device => Device.ServiceId === 2 || Device.ServiceId === 3),
      ...(View.Endpoints || []).filter(Endpoint => Endpoint.Caps !== 0).slice().sort((First, Second) => First.HwName.localeCompare(Second.HwName, undefined, {numeric: true})).map(Endpoint => ({...View.Devices.find(Device => Device.DevId === Endpoint.OutAddress?.split('/')[0]),
        Func: Endpoint.Func, HwName: Endpoint.HwName, OutAddress: Endpoint.OutAddress, CapabilityId: Endpoint.Caps, EndpointGroups: Endpoint.Groups, UseHwName: Endpoint.UseHwName, EndpointAvailable: Endpoint.Available}))];
    document.getElementById('AssignmentRows').replaceChildren(...Rows.map(Device => {
      const Row = document.createElement('tr');
      const FirmwareReady = !Device.OutAddress || Device.CapabilityId !== 1 || Device.EndpointAssignments === 1;
      const Ready = FirmwareReady && Device.EndpointAvailable !== false && (Device.ServiceId === 2 || Device.Applied);
      const Address = Device.Url
        ? new URL(Device.Url, window.location.href).host
        : Device.BaseUrl
        ? new URL(Device.BaseUrl, window.location.href).host
        : Device.HttpPort && Device.HttpPort !== 80 ? `${Device.IP}:${Device.HttpPort}` : Device.IP;
      const FunctionLabel = Device.OutAddress
        ? `${AssignmentFunctionLabel(Device.Func, Device.EndpointGroups, Device.UseHwName ? Device.HwName : null)}${Device.CapabilityId === 2 ? ` · ${Tr('TemperatureSource', 'Temperature source')}` : ''}`
        : `${DeviceDisplayName(Device.Func)}${Device.ServiceId === 3 ? ` · ${Tr('FeedingFunction', 'Feeding')}` : ''}`;
      const CapabilityLabel = Device.OutAddress ? `${Device.CapabilityId}: ${Device.CapabilityId === 2 ? Tr('TemperatureSource', 'Temperature source') : Tr('SwitchCapability', 'Switching')}` : Device.ServiceId === 3 ? Tr('FeedingFunction', 'Feeding') : '—';
      for (const Text of [FunctionLabel, Address, Device.HwName ? `${Device.HwName} · ${Device.OutAddress || Device.DevId}` : Device.DevId, CapabilityLabel, Ready ? Tr('AssignmentReady') : Device.OutAddress && Device.EndpointAvailable === false ? Tr('SensorStateUnknown', 'Sensor state unknown') : !Device.Online ? Tr('Offline') : !FirmwareReady ? Tr('EndpointFirmwareUpdateRequired') : Tr('AssignmentNotReady')]) {
        const Cell = document.createElement('td');
        const Captions = [Tr('Function'), 'IP / Port', Tr('PhysicalDevice'), Tr('CapabilityId'), Tr('Status')];
        Cell.dataset.label = Captions[Row.children.length];
        Cell.textContent = Text; Row.appendChild(Cell);
      }
      return Row;
    }));
    document.getElementById('AssignmentRows').closest('table').ApplyColumnSort?.();
    for (const Id of ['AssignmentFirst', 'AssignmentSecond']) {
      const Selector = document.getElementById(Id), Previous = Selector.value;
      Selector.replaceChildren(...AssignmentChoices(View).map(Item => {
        const Option = document.createElement('option'); Option.value = Item.Value; Option.textContent = Item.Label; return Option;
      }));
      if ([...Selector.options].some(Option => Option.value === Previous)) Selector.value = Previous;
    }
    Message.classList.toggle('Success', !View.Err && !View.Pending && View.AllApplied);
    Message.textContent = View.Err || (View.Pending ? Tr('AssignmentPending') : View.AllApplied ? Tr('AssignmentComplete') : '');
    Message.hidden = !Message.textContent;
    if (View.Pending && document.getElementById('Assignments').classList.contains('Active'))
      AssignmentTimer = setTimeout(LoadAssignments, 3000);
  } catch (Error) {
    Message.textContent = Tr('AssignmentError') + Error.message; Message.hidden = false;
    AssignmentView = null;
  } finally { AssignmentLoading = false; AssignmentButtons(); DeviceEditorButtons(); }
}

async function ChangeAssignment(Operation) {
  if (AssignmentLoading || !AssignmentView) return;
  const Message = document.getElementById('AssignmentMessage');
  const Selected = document.getElementById('AssignmentFirst').value;
  if (Operation === 'Swap') Operation = Selected.startsWith('Device:') ? 'Swap' : 'SwapEndpoints';
  const ChoiceValue = Id => {
    const Value = document.getElementById(Id).value;
    return Operation === 'Swap' ? Value.replace(/^Device:/, '') : Value;
  };
  AssignmentLoading = true;
  AssignmentButtons();
  try {
    const Response = await fetch('api/device-assignments', {
      method: 'POST', headers: {'Content-Type': 'application/json'}, signal: AbortSignal.timeout(10000),
      body: JSON.stringify({Operation, ExpectedRevision: AssignmentView.Rev,
        First: ChoiceValue('AssignmentFirst'), Second: ChoiceValue('AssignmentSecond'),
        ConsumersDetached: document.getElementById('AssignmentDetached').checked})
    });
    const Result = await Response.json();
    if (!Response.ok || !Result.Ok) throw new Error(Result.Err || `HTTP ${Response.status}`);
    document.getElementById('AssignmentDetached').checked = false;
    Message.textContent = Tr('AssignmentPending'); Message.hidden = false;
    // Rebuild all device clients from the newly committed function/IP mapping.
    window.location.reload();
    return;
  } catch (Error) { Message.classList.remove('Success'); Message.textContent = Tr('AssignmentError') + Error.message; Message.hidden = false; return; }
  finally { AssignmentLoading = false; AssignmentButtons(); DeviceEditorButtons(); }
  await LoadAssignments();
}

document.getElementById('AssignmentReload').addEventListener('click', LoadAssignments);
document.getElementById('AssignmentResume').addEventListener('click', () => ChangeAssignment('Resume'));
document.getElementById('AssignmentSwap').addEventListener('click', () => ChangeAssignment('Swap'));
for (const Id of ['AssignmentFirst', 'AssignmentSecond', 'AssignmentDetached'])
  document.getElementById(Id).addEventListener('change', AssignmentButtons);

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


let DeviceEditorRevision = null;
let DeviceEditorDirty = false;
let DeviceEditorLoading = false;

function DeviceEditorButtons() {
  document.getElementById('DeviceEditorSave').disabled = !DeviceEditorDirty || DeviceEditorLoading || !AssignmentView || AssignmentView.Pending;
}

async function LoadDeviceEditor() {
  if (DeviceEditorLoading) return;
  DeviceEditorLoading = true;
  const Message = document.getElementById('DeviceEditorMessage');
  try {
    const Response = await fetch('api/device-editor', {cache: 'no-store', signal: AbortSignal.timeout(10000)});
    const Data = await Response.json();
    if (!Response.ok) throw new Error(`HTTP ${Response.status}`);
    DeviceEditorRevision = Data.Rev;
    document.getElementById('DeviceEditorText').value = FormatEditorJson({Devices: Data.Devices}) + '\n';
    DeviceEditorDirty = false; Message.textContent = '';
  } catch (Error) { Message.textContent = Tr('AssignmentError') + Error.message; }
  finally { DeviceEditorLoading = false; DeviceEditorButtons(); }
}

async function SaveDeviceEditor() {
  if (DeviceEditorLoading || !DeviceEditorDirty) return;
  const Message = document.getElementById('DeviceEditorMessage');
  DeviceEditorLoading = true; DeviceEditorButtons();
  try {
    const Text = document.getElementById('DeviceEditorText');
    const Data = JSON.parse(Text.value);
    if (!Data || Object.keys(Data).length !== 1 || !Array.isArray(Data.Devices)) throw new Error('Devices array required');
    Text.value = FormatEditorJson(Data) + '\n';
    const Response = await fetch('api/device-assignments', {
      method: 'POST', headers: {'Content-Type': 'application/json'}, signal: AbortSignal.timeout(30000),
      body: JSON.stringify({Operation: 'EditDevices', ExpectedRevision: DeviceEditorRevision, Devices: Data.Devices})
    });
    const Result = await Response.json();
    if (!Response.ok || !Result.Ok) throw new Error(Result.Err || `HTTP ${Response.status}`);
    window.location.reload();
  } catch (Error) { Message.textContent = Tr('AssignmentError') + Error.message; await LoadAssignments(); }
  finally { DeviceEditorLoading = false; DeviceEditorButtons(); }
}

document.getElementById('DeviceSwapToggle').addEventListener('click', () => {
  const Panel = document.getElementById('DeviceSwapPanel'); Panel.hidden = !Panel.hidden;
  document.getElementById('DeviceSwapToggle').setAttribute('aria-expanded', String(!Panel.hidden));
  if (Panel.hidden) {
    document.getElementById('DeviceEditorPanel').hidden = true;
    document.getElementById('DeviceEditorToggle').setAttribute('aria-expanded', 'false');
  }
});
document.getElementById('DeviceEditorToggle').addEventListener('click', () => {
  const Panel = document.getElementById('DeviceEditorPanel'); Panel.hidden = !Panel.hidden;
  document.getElementById('DeviceEditorToggle').setAttribute('aria-expanded', String(!Panel.hidden));
  if (!Panel.hidden && !DeviceEditorDirty) LoadDeviceEditor();
});
document.getElementById('DeviceEditorText').addEventListener('input', () => { DeviceEditorDirty = true; DeviceEditorButtons(); });
document.getElementById('DeviceEditorReload').addEventListener('click', LoadDeviceEditor);
document.getElementById('DeviceEditorSave').addEventListener('click', SaveDeviceEditor);

// Size columns from rendered content; manual widths survive data refreshes.
function EnableColumnSizing(Table, Key) {
  const Headers = [...Table.querySelectorAll('thead th')];
  if (!Headers.length) return;
  let Manual = {};
  try { Manual = JSON.parse(localStorage.getItem(Key) || '{}'); } catch (_) {}
  const Columns = document.createElement('colgroup');
  Headers.forEach(() => Columns.appendChild(document.createElement('col')));
  Table.prepend(Columns);
  let Queued = false;
  function FitColumns() {
    if (getComputedStyle(Table).display !== 'table' || !Table.getBoundingClientRect().width) return;
    const Copy = Table.cloneNode(true);
    Copy.querySelectorAll('[id]').forEach(Node => Node.removeAttribute('id'));
    Copy.querySelector('colgroup')?.remove();
    Copy.querySelectorAll('.ColumnResizeHandle').forEach(Node => Node.remove());
    Copy.classList.add('ColumnMeasure');
    Copy.style.width = 'max-content';
    Table.parentElement.appendChild(Copy);
    const Widths = [...Copy.querySelectorAll('thead th')].map(Header => Math.ceil(Header.getBoundingClientRect().width));
    Copy.remove();
    Widths.forEach((Width, Index) => Columns.children[Index].style.width = `${Math.max(48, Number(Manual[Index]) || Width)}px`);
    Table.style.width = `${Widths.reduce((Sum, Width, Index) => Sum + Math.max(48, Number(Manual[Index]) || Width), 0)}px`;
  }
  function QueueFit() {
    if (Queued) return;
    Queued = true;
    requestAnimationFrame(() => { Queued = false; FitColumns(); });
  }
  function Save() { try { localStorage.setItem(Key, JSON.stringify(Manual)); } catch (_) {} }
  const Handles = [];
  Headers.forEach((Header, Index) => {
    const Handle = document.createElement('span');
    Handle.className = 'ColumnResizeHandle'; Handle.tabIndex = 0;
    Handle.setAttribute('role', 'separator'); Handle.setAttribute('aria-orientation', 'vertical');
    Handle.setAttribute('aria-label', Header.textContent.trim());
    Handle.title = Tr('ResizeColumnHint', 'Drag to resize; double-click to fit content');
    Handle.addEventListener('click', Event => Event.stopPropagation());
    Handle.addEventListener('dblclick', Event => {
      Event.preventDefault(); Event.stopPropagation(); delete Manual[Index]; Save(); FitColumns();
    });
    Handle.addEventListener('keydown', Event => {
      if (Event.key === 'Enter') { delete Manual[Index]; Save(); FitColumns(); }
      else if (Event.key === 'ArrowLeft' || Event.key === 'ArrowRight') {
        Manual[Index] = Math.max(48, Header.getBoundingClientRect().width + (Event.key === 'ArrowLeft' ? -10 : 10));
        Save(); FitColumns();
      } else return;
      Event.preventDefault(); Event.stopPropagation();
    });
    Handle.addEventListener('pointerdown', Event => {
      if (Event.button !== 0) return;
      Event.preventDefault(); Event.stopPropagation();
      const Start = Event.clientX, Width = Header.getBoundingClientRect().width;
      Handle.setPointerCapture(Event.pointerId);
      const Move = Current => { Manual[Index] = Math.max(48, Width + Current.clientX - Start); FitColumns(); };
      const End = () => {
        Handle.removeEventListener('pointermove', Move);
        Handle.removeEventListener('pointerup', End); Handle.removeEventListener('pointercancel', End); Save();
      };
      Handle.addEventListener('pointermove', Move);
      Handle.addEventListener('pointerup', End); Handle.addEventListener('pointercancel', End);
    });
    Header.appendChild(Handle); Handles.push(Handle);
  });
  // Translation refreshes replace header text; restore the interactive dividers.
  new MutationObserver(() => {
    Headers.forEach((Header, Index) => {
      if (!Header.contains(Handles[Index])) Header.appendChild(Handles[Index]);
      Handles[Index].title = Tr('ResizeColumnHint', 'Drag to resize; double-click to fit content');
    });
    QueueFit();
  }).observe(Table.querySelector('thead'), {childList: true, subtree: true});
  Table.classList.add('ResizableColumns');
  new MutationObserver(QueueFit).observe(Table.tBodies[0] || Table, {childList: true, subtree: true, characterData: true});
  window.addEventListener('resize', QueueFit);
  document.getElementById('PageSelector').addEventListener('change', QueueFit);
  QueueFit();
}
document.querySelectorAll('.TimesTable').forEach((Table, Index) => EnableColumnSizing(Table, `DashboardColumnWidths:${Table.closest('section')?.id || Index}:${Index}`));

// Presentation sorting never changes the authoritative configuration order.
function EnableColumnSorting(Table) {
  if (Table.querySelector('.TableSortButton')) return;
  const Headers = [...Table.querySelectorAll('thead th')];
  let Column = -1, Direction = 1;
  const Collator = new Intl.Collator(undefined, {numeric: true, sensitivity: 'base'});
  Table.ApplyColumnSort = () => {
    if (Column < 0 || !Table.tBodies[0]) return;
    const Body = Table.tBodies[0];
    const Rows = [...Body.rows].sort((First, Second) => Direction * Collator.compare(
      First.cells[Column]?.dataset.sortValue ?? First.cells[Column]?.textContent ?? '',
      Second.cells[Column]?.dataset.sortValue ?? Second.cells[Column]?.textContent ?? ''));
    Body.append(...Rows);
  };
  Headers.forEach((Header, Index) => {
    const Button = document.createElement('button'); Button.type = 'button'; Button.className = 'ColumnSortButton';
    Button.textContent = Header.textContent;
    if (Header.dataset.tr) { Button.dataset.tr = Header.dataset.tr; Header.removeAttribute('data-tr'); }
    [...Header.childNodes].filter(Node => !Node.classList?.contains('ColumnResizeHandle')).forEach(Node => Node.remove());
    Header.prepend(Button);
    Header.setAttribute('aria-sort', 'none');
    Button.addEventListener('click', () => {
      Direction = Column === Index ? -Direction : 1; Column = Index;
      Headers.forEach((Item, I) => {
        Item.setAttribute('aria-sort', I === Index ? Direction === 1 ? 'ascending' : 'descending' : 'none');
        Item.querySelector('.ColumnSortButton').dataset.direction = I === Index ? Direction === 1 ? '▲' : '▼' : '';
      });
      Table.ApplyColumnSort();
    });
  });
}
document.querySelectorAll('.TimesTable').forEach(EnableColumnSorting);

// A separate, explicitly opened editor; no periodic configuration polling.
let FeedbackEditorPrevious = null;
async function LoadFeedbackEditor() {
  const Response = await fetch('api/feedback-monitors', {cache: 'no-store', signal: AbortSignal.timeout(6000)});
  if (!Response.ok) throw new Error(`HTTP ${Response.status}`);
  const Data = GnJson.Read('THttpFeedbackDocument', await Response.text());
  FeedbackEditorPrevious = Data.Cfg;
  document.getElementById('FeedbackEditorText').value = FormatEditorJson(Data.Cfg) + '\n';
  document.getElementById('FeedbackEditorMessage').textContent = '';
}
async function FeedbackEditorAction(Action) {
  const Save = document.getElementById('FeedbackEditorSave');
  const Reload = document.getElementById('FeedbackEditorReload');
  Save.disabled = Reload.disabled = true;
  try { await Action(); }
  catch (Error) { document.getElementById('FeedbackEditorMessage').textContent = Error.message; }
  finally { Save.disabled = Reload.disabled = false; }
}
document.getElementById('FeedbackEditorToggle').addEventListener('click', () => {
  const Panel = document.getElementById('FeedbackEditorPanel'); Panel.hidden = !Panel.hidden;
  document.getElementById('FeedbackEditorToggle').setAttribute('aria-expanded', String(!Panel.hidden));
  if (!Panel.hidden && FeedbackEditorPrevious === null) FeedbackEditorAction(LoadFeedbackEditor);
});
document.getElementById('FeedbackEditorReload').addEventListener('click', () => FeedbackEditorAction(LoadFeedbackEditor));
document.getElementById('FeedbackEditorSave').addEventListener('click', () => FeedbackEditorAction(async () => {
  const Configuration = GnJson.Read('TJsonFeedbackConfiguration', document.getElementById('FeedbackEditorText').value);
  const Response = await fetch('api/feedback-monitors', {method: 'PUT', headers: {'Content-Type': 'application/json'},
    signal: AbortSignal.timeout(6000), body: GnJson.Write('THttpFeedbackWrite', {Cfg: Configuration, Previous: FeedbackEditorPrevious})});
  if (!Response.ok) throw new Error(await Response.text());
  await LoadFeedbackEditor();
  document.getElementById('FeedbackEditorMessage').textContent = Tr('FeedbackSaved', 'Überwachung gespeichert.');
}));
