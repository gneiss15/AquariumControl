// Dashboard.js, Version: 1.76
import {TDeviceStatusClient} from './StatusClient.js';
import {TFeedbackAlarm} from './FeedbackAlarm.js';

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
let CentralTimeStore = {Version: 1, Sets: [], Times: []};
let CentralHeatProfiles = {Profiles: []};
let CentralHeatProfileStatus = {Sets: []};
let CentralChannelStore = {Channels: {}};
let CentralChannelDrafts = new Map();
let CentralTimeSortKey = null;
let OverviewOutputOrder = {};
let OverviewFitPending = false;
let AssignmentView = null;
let AssignmentTimer = null;
let AssignmentLoading = false;

class TDashboardDevice {
  constructor(Configuration) {
    this.Configuration = Configuration;
    this.TechnicalName = Configuration.Function || Configuration.Name || new URL(Configuration.BaseUrl, window.location.href).hostname;
    this.BaseUrl = new URL(Configuration.BaseUrl, window.location.href);
    this.ConfigUrl = Configuration.ConfigUrl
      ? new URL(Configuration.ConfigUrl, this.BaseUrl)
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

  get DisplayName() { return DeviceDisplayName(this.TechnicalName, this.Configuration); }

  async Start() {
    this.StopClient();
    SetConnectionState(this, 'Offline');
    if (this.Configuration.Active === false) return;
    try {
      this.Client = new TDeviceStatusClient(
        this.BaseUrl,
        (Status, Sequence) => this.HandleStatus(Status, Sequence),
        Connected => this.HandleConnectionChanged(Connected),
        this.Configuration.ExternalSourceId || null
      );
      await this.Client.Start();
      this.DeviceInfo = this.Client.Device;
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
    if (Array.isArray(Status.FeedbackMonitors)) FeedbackAlarm.Update(Status.FeedbackMonitors);
    this.LastStatusAt = new Date();
    for (const View of this.Views)
      UpdateDeviceStatus(View, Sequence);
    UpdateDashboardTimestamp();
  }

  HandleConnectionChanged(Connected) {
    this.UdpConnected = Connected;
    SetConnectionState(this, Connected ? 'Live' : 'Offline');
    if (!Connected && this.Configuration.ExternalSourceId) ShowDeviceOffline(this);
  }
}

class TDashboardDeviceView {
  constructor(Device, Configuration) {
    this.Owner = Device;
    this.Configuration = Device.Configuration;
    this.Group = Configuration.Group || Device.Configuration.Group || 'Other';
    this.MeasurementIds = Array.isArray(Configuration.Measurements) ? new Set(Configuration.Measurements) : null;
    this.StatusFunction = Configuration.StatusFunction || null;
    this.TemperatureOnly = Configuration.TemperatureOnly === true;
    this.ChannelIds = Array.isArray(Configuration.Channels)
      ? new Set(Configuration.Channels.map(String))
      : null;
    this.Card = null;
    this.StatusRow = null;
    this.MeasurementRows = [];
    this.ChannelRows = [];
  }

  get BaseUrl() { return this.Owner.BaseUrl; }
  get Client() { return this.Owner.Client; }
  get ConfigUrl() { return this.Owner.ConfigUrl; }
  get DeviceInfo() { return this.Owner.DeviceInfo; }
  get DisplayName() { return this.StatusFunction ? `${this.TemperatureOnly ? Tr('TemperatureSource', 'Temperature source') + ' · ' : ''}${DeviceDisplayName(this.StatusFunction)}` : this.Owner.DisplayName; }
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
  for (const Element of document.querySelectorAll('[data-tr-title]')) {
    const Text = Tr(Element.dataset.trTitle, Element.title);
    Element.title = Text;
    Element.setAttribute('aria-label', Text);
  }
  document.documentElement.lang = LangNr === 1 ? 'de' : 'en';
  document.title = Tr('DashboardTitle', 'Aquarium Dashboard');
}

function DeviceDisplayName(Name, Configuration = null) {
  const Config = Configuration || Devices.find(Device => Device.TechnicalName === Name)?.Configuration;
  if (Config?.UseHardwareName && Config.HardwareName) return Config.HardwareName;
  if (Config?.EndpointFunction) return Tr(`Channel.${Config.EndpointFunction}`, Tr(`Device.${Config.EndpointFunction}`, Config.EndpointFunction));
  if (Name === 'GN-Pi') return Tr('Device.MultiTimer Aq1 & 2', 'Zeitschaltuhr Aq1 & Aq2');
  return Tr(`Device.${Name}`, Name);
}

function ChannelDisplayName(Channel, Device = null) {
  const RawName = Channel?.Name || Channel?.Id;
  const DeviceName = typeof Device === 'string' ? Device : Device?.TechnicalName;
  const Configuration = typeof Device === 'string'
    ? Devices.find(Item => Item.TechnicalName === Device)?.Configuration : Device?.Configuration;
  if (Configuration?.EndpointHardwareLabels?.[RawName]) return Configuration.EndpointHardwareLabels[RawName];
  if (Configuration?.UseHardwareName && Configuration.HardwareName) return Configuration.HardwareName;
  const DeviceKey = `Channel.${DeviceName}.${RawName}`;
  if (DeviceName && Array.isArray(Translations[DeviceKey]))
    return Tr(DeviceKey);
  if (RawName !== undefined && RawName !== null && String(RawName).length > 0)
    return Tr(`Channel.${RawName}`, String(RawName));
  return `${Tr('Channel', 'Channel')} ${Channel?.Id ?? ''}`.trim();
}

function RefreshTranslatedContent() {
  TrAll();
  for (const Device of Devices) {
    if (Device.ConfigSelectorOption !== null)
      Device.ConfigSelectorOption.textContent = DeviceDisplayName(Device.TechnicalName);
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
  if (CentralTimeStore.Sets.length > 0 || CentralTimeStore.Times.length > 0)
    RenderCentralTimes();
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
  if (Device.Configuration.StatusOnly === true) {
    const Measurements = Card.querySelector('.MeasurementList');
    const ErrorPanel = Card.querySelector('.ErrorPanel');
    Measurements.after(ErrorPanel);
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

  if (!Device.Configuration.Name) {
    Device.TechnicalName = Device.Configuration.Function || Device.DeviceInfo.Name || Device.DeviceInfo.DeviceId || Device.TechnicalName;
    if (Device.ConfigSelectorOption !== null)
      Device.ConfigSelectorOption.textContent = DeviceDisplayName(Device.TechnicalName);
  }

  if (Device.DeviceInfo.ConfigUrl)
    Device.ConfigUrl = new URL(Device.DeviceInfo.ConfigUrl, Device.BaseUrl);
  if (Device.ConfigSelectorOption !== null)
    Device.ConfigSelectorOption.disabled = Device.ConfigUrl === null;
}

function SetConnectionState(Device, State) {
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
    const Panel = View.Card.querySelector('.ErrorPanel');
    const List = View.Card.querySelector('.ErrorList');
    List.replaceChildren();
    Panel.hidden = true;

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
}

function ShowDeviceError(Device, Message) {
  const Panel = Device.Card.querySelector('.ErrorPanel');
  const List = Device.Card.querySelector('.ErrorList');
  List.replaceChildren(CreateElement('li', '', Message));
  Panel.hidden = false;
  const Measurements = Device.Card.querySelector('.MeasurementList');
  Measurements.hidden = true;
  Measurements.replaceChildren();
  const Channels = Device.Card.querySelector('.ChannelList');
  if (Device.Configuration.StatusOnly === true) {
    Channels.hidden = true;
    Channels.replaceChildren();
  } else {
    Channels.hidden = false;
    Channels.replaceChildren(CreateElement('div', 'EmptyChannels', Tr('NoStatusData', 'Keine Statusdaten verfügbar.')));
  }
}

function UpdateDeviceStatus(Device) {
  const Status = Device.Status;
  if (Status === null) return;
  UpdateErrors(Device);
  UpdateFeedbackRows(Device);
  UpdateMeasurements(Device);
  UpdateChannels(Device);
  UpdateGroupOutputs(DeviceGroups.get(Device.Group));
}

function UpdateFeedbackRows(Device) {
  Device.FeedbackRows = (Device.Status.FeedbackMonitors || []).map(Monitor => {
    const Row = CreateElement('div', 'ChannelRow FeedbackRow');
    Row.dataset.outputId = `Feedback.${Monitor.Id}`;
    const Labels = {OK: Tr('FeedbackOK', 'Zustand bestätigt'),
      Unknown: String(Monitor.Reason).startsWith('Invalid') ? Tr('FeedbackInvalid', 'Rückmeldung inkorrekt') : Tr('FeedbackUnknown', 'Rückmeldung fehlt'),
      Pending: Tr('FeedbackPending', 'Zustand wird geprüft'), Mismatch: Tr('FeedbackMismatch', 'Zustand nicht bestätigt')};
    Row.append(CreateElement('span', '', Monitor.SwitchFunction), CreateElement('span', Monitor.State === 'Mismatch' ? 'Error' : '', Labels[Monitor.State] || Monitor.State));
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

const FeedbackAlarm = new TFeedbackAlarm(() => {
  const Toggle = document.getElementById('FeedbackSoundToggle');
  const Running = FeedbackAlarm.Enabled && FeedbackAlarm.Context?.state === 'running';
  Toggle.textContent = Running ? Tr('AlarmSoundOff', 'Warnton ausschalten')
    : FeedbackAlarm.Requested ? Tr('AlarmSoundRelease', 'Warnton freigeben') : Tr('AlarmSoundOn', 'Warnton aktivieren');
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

function UpdateErrors(Device) {
  const Panel = Device.Card.querySelector('.ErrorPanel');
  const List = Device.Card.querySelector('.ErrorList');
  const Errors = [...(Device.Status.Errors || [])];

  if (Device.Status.LastError?.Message)
    Errors.push(`Letzter Logfehler: ${Device.Status.LastError.Message}`);
  if (Errors.length === 0) {
    Panel.hidden = true;
    List.replaceChildren();
    return;
  }

  List.replaceChildren(...Errors.map(Error => CreateElement('li', '', Error)));
  Panel.hidden = false;
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
  Row.dataset.outputId = String(Measurement.Id || '');
  Row.appendChild(CreateElement('span', 'MeasurementName', Measurement.Name || Measurement.Id || 'Messwert'));
  const Value = CreateElement(
    'strong',
    Measurement.Valid === false ? 'MeasurementValue Invalid' : 'MeasurementValue',
    FormatMeasurement(Measurement)
  );
  if (Measurement.Error)
    Value.title = Measurement.Error;
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
  const Measurements = (Device.Status.Measurements || []).filter(Measurement => Device.MeasurementIds === null || Device.MeasurementIds.has(Measurement.Id));
  const Rows = Measurements.map(CreateMeasurementRow);
  if (Device.Configuration.ExternalSourceId && Object.hasOwn(Device.Status, 'BatteryPercent')) {
    const Value = Device.Status.BatteryPercent;
    Rows.push(CreateMeasurementRow({Id: 'BatteryPercent',
      Name: `${Tr('BatteryLevel', 'Akkustand')} ${Device.DisplayName}`, Value,
      Valid: typeof Value === 'number' && Number.isFinite(Value) && Value >= 0 && Value <= 100,
      Unit: '%', Decimals: 0}));
  }
  if (Device.Status.PowerMeter)
    Rows.push(CreatePowerMeterRow(Device.Status.PowerMeter, Device.Configuration.PowerLabel || 'Power',
      Device.Configuration.UseHardwareName && Device.Configuration.HardwareName
        ? `${Tr('Power', 'Power')} ${Device.Configuration.HardwareName}` : Device.Configuration.EndpointFunction ? `${Tr('Power', 'Power')} ${ChannelDisplayName({Name: Device.Configuration.EndpointFunction})}` : null));
  Container.hidden = Rows.length === 0;
  Container.replaceChildren(...Rows);
  Device.MeasurementRows = Rows;
}

function UpdateChannels(Device) {
  const Container = Device.Card.querySelector('.ChannelList');
  if (Device.Configuration.StatusOnly === true) {
    Container.hidden = true;
    Container.replaceChildren();
    return;
  }
  Container.hidden = false;
  const AllChannels = Device.Status.Channels || [];
  const Channels = (Device.ChannelIds === null
    ? AllChannels
    : AllChannels.filter(Channel => Device.ChannelIds.has(String(Channel.Name || Channel.Id))))
    .filter(Channel => String(Channel.Name || '').toLowerCase() !== 'fütterung');
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

  for (const View of Group.Views) {
    const Panel = View.Card.querySelector('.ErrorPanel');
    if (!Panel.hidden) {
      const ErrorRow = Panel.cloneNode(true);
      ErrorRow.querySelector('strong').textContent = View.DisplayName;
      Group.Outputs.appendChild(ErrorRow);
    }
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
  ScheduleOverviewFit();
}

function GetButtonAction(Device, Channel) {
  const Action = Device.DeviceInfo?.Actions?.ButtonPress;
  if (!Action?.Path || !Channel.HasOutput) return null;

  const Presses = (Action.Presses || []).map(Press => String(Press).toLowerCase());
  return {
    Path: Action.Path,
    Short: Presses.includes('short'),
    Long: Presses.includes('long')
  };
}

function BuildButtonActionUrl(Device, Channel, Press) {
  const Action = GetButtonAction(Device, Channel);
  if (Action === null) throw new Error('Tasteraktion wird von diesem Kanal nicht unterstützt.');

  const Path = Action.Path
    .replace('{ChannelId}', encodeURIComponent(String(Channel.Id)))
    .replace('{Press}', encodeURIComponent(String(Press).toLowerCase()));
  return new URL(Path, Device.BaseUrl);
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

  const State = Channel.HasOutput ? Channel.State : Channel.InputState;
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

  if (Channel.Error || Channel.OverrideActive) {
    Row.classList.add('HasMarker');
    Row.appendChild(CreateElement('span', Channel.Error ? 'ChannelMarker Error' : 'ChannelMarker Override', '●'));
  }
  return Row;
}

function FormatState(State, Channel = null) {
  const Labels = Channel?.StateLabels || {};
  if (State === true) return Labels.True || Tr('On', 'EIN');
  if (State === false) return Labels.False || Tr('Off', 'AUS');
  return Labels.Unknown || '–';
}

function FormatOverride(Channel) {
  const State = FormatState(Channel.OverrideState, Channel);
  if (Channel.OverrideUntil)
    return `${State} bis ${FormatDateTime(Channel.OverrideUntil)}`;
  return `${State} ${Tr('UntilNextEvent', 'bis zum nächsten Ereignis')}`;
}

function FormatNextChange(Channel, NextChange) {
  if (NextChange.Source === 'FeedTime') {
    const Details = [FormatDateTime(NextChange.Time)];
    if (Number.isFinite(Number(NextChange.Portions))) {
      const Portions = Number(NextChange.Portions);
      Details.push(`${Portions} ${Portions === 1 ? Tr('Portion', 'Portion') : Tr('Portions', 'Portionen')}`);
    }
    return Details.join(' · ');
  }

  const SourceNames = {
    Interval: Tr('Interval', 'Intervall'),
    OverrideEnd: Tr('OverrideEnd', 'Override-Ende'),
    SwitchTime: Tr('SwitchTime', 'Schaltzeit')
  };
  const Source = SourceNames[NextChange.Source] || NextChange.Source || Tr('Change', 'Änderung');
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
  const State = Channel.HasOutput ? Channel.State : Channel.InputState;
  const Body = document.getElementById('DetailBody');
  document.getElementById('DetailTitle').textContent = ChannelDisplayName(Channel, Device);
  Body.replaceChildren();

  AddDetailRow(Body, Tr('Device', 'Gerät'), Device.DisplayName);
  AddDetailRow(Body, Tr('State', 'Zustand'), FormatState(State, Channel), State === true ? 'Good' : '');
  if (Channel.HasOutput && Channel.AutomaticState !== null && Channel.AutomaticState !== undefined)
    AddDetailRow(Body, Tr('Automatic', 'Automatik'), FormatState(Channel.AutomaticState, Channel));
  if (Channel.HasInput)
    AddDetailRow(Body, Tr('Input', 'Eingang'), FormatState(Channel.InputState));
  if (Channel.OverrideActive)
    AddDetailRow(Body, Tr('ManualOverride', 'Manuelle Vorgabe'), FormatOverride(Channel), 'Warning');
  if (Channel.NextChange)
    AddDetailRow(
      Body,
      Channel.NextChange.Source === 'FeedTime' ? Tr('NextFeeding', 'Nächste Fütterung') : Tr('NextChange', 'Nächste Änderung'),
      FormatNextChange(Channel, Channel.NextChange)
    );
  if (Channel.Error)
    AddDetailRow(Body, Tr('Error', 'Fehler'), Channel.Error, 'Error');

  AddDetailRow(Body, Tr('Connection', 'Verbindung'),
    Device.Views[0]?.StatusRow.querySelector('.ConnectionBadge').textContent || Tr('Offline', 'Offline'));
  if (Device.Status !== null) {
    AddDetailRow(Body, Tr('System', 'System'), Device.Status.Healthy ? 'OK' : Tr('Error', 'Fehler'), Device.Status.Healthy ? 'Good' : 'Error');
    AddDetailRow(Body, Tr('TimeSync', 'Zeitsynchronisation'), Device.Status.TimeSynchronized ? Tr('Synchronized', 'Synchron') : Tr('NotSynchronized', 'Nicht synchron'));
  }
  AddDetailRow(Body, Tr('Updated', 'Aktualisiert'), FormatTime(Device.LastStatusAt));
  if (Device.DeviceInfo?.FirmwareVersion)
    AddDetailRow(Body, 'Firmware', Device.DeviceInfo.FirmwareVersion);
  if (Device.DeviceInfo?.DataVersion)
    AddDetailRow(Body, 'Data', Device.DeviceInfo.DataVersion);

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
  const Option = CreateElement('option', '', Device.DisplayName);
  Option.value = Device.ConfigTabId;
  Option.disabled = Device.ConfigUrl === null;
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
  const Url = new URL(Device.ConfigUrl);
  if (Device.DeviceInfo?.DataVersion)
    Url.searchParams.set('_DataVersion', Device.DeviceInfo.DataVersion);
  return Url.toString();
}

function LoadConfigurationFrame(Device) {
  if (Device.ConfigFrame !== null || Device.ConfigUrl === null) return;

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
    Source: 'GnDashboard',
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
  if (Device === undefined || Event.data?.Source !== 'GnConfigPage') return;

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

  if (Device.Configuration.Responsive === true) {
    Device.ConfigCanvas.style.width = `${AvailableWidth}px`;
    Device.ConfigCanvas.style.height = `${AvailableHeight}px`;
    Device.ConfigFrame.style.width = `${AvailableWidth}px`;
    Device.ConfigFrame.style.height = `${AvailableHeight}px`;
    Device.ConfigFrame.style.transform = 'none';
    Device.ConfigViewport.classList.remove('HorizontalScroll');
    UpdateFrameScaleLabel(Device);
    return;
  }

  const ConfiguredWidth = Number(Device.Configuration.FrameWidth);
  const NaturalFrameWidth = Number.isFinite(ConfiguredWidth) && ConfiguredWidth >= 320
    ? ConfiguredWidth
    : DefaultFrameWidth;
  const FrameWidth = Math.max(NaturalFrameWidth, AvailableWidth);
  const ConfiguredMinimum = Number(Device.Configuration.MinimumScale);
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
  return CentralTimeStore.Sets.find(Set => Set.Device === Device && Set.Type === Type) || null;
}

function UpdateTimeKindSelector() {
  const SetSelector = document.getElementById('TimeSetSelector');
  const KindSelector = document.getElementById('TimeKindSelector');
  const Selected = CentralTimeStore.Sets.find(Set => CentralSetKey(Set.Device, Set.Type) === SetSelector.value);
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
    String(Left.Device).localeCompare(String(Right.Device), 'de') || String(Left.Type).localeCompare(String(Right.Type), 'de'));
  Selector.replaceChildren(...Sets.map(Set => {
    const Option = CreateElement('option', '', DeviceDisplayName(Set.Device));
    Option.value = CentralSetKey(Set.Device, Set.Type);
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
  const States = new Map(Sets.map(Set => [CentralSetKey(Set.Device, Set.Type), Set]));
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

function RenderCentralTimes() {
  const Body = document.getElementById('TimesData');
  CentralTimeSortKey = null;
  document.querySelectorAll('.TableSortButton').forEach(Button => { Button.dataset.direction = ''; Button.removeAttribute('aria-sort'); });
  const Rows = CentralTimeStore.Times.map((Entry, Index) => {
    const Row = document.createElement('tr');
    Row.dataset.id = Entry.Id;
    Row.dataset.device = Entry.Device;
    Row.dataset.type = Entry.Type;
    Row.dataset.kind = Entry.Kind;
    Row.appendChild(CreateElement('td', '', String(Index + 1)));
    Row.appendChild(CreateElement('td', '', DeviceDisplayName(Entry.Device)));
    const DataCell = document.createElement('td');
    DataCell.dataset.sortValue = JSON.stringify(Entry.Data);
    DataCell.appendChild(RenderTimeDataEditor(Entry));
    Row.appendChild(DataCell);
    const Set = FindCentralSet(Entry.Device, Entry.Type);
    const StatusCell = CreateElement('td', `TimeStatusCell ${Set?.Synced ? 'TimeStatusSynced' : 'TimeStatusPending'}`);
    StatusCell.dataset.sortValue = Set?.Synced ? '1' : '0';
    StatusCell.appendChild(CreateTimeStatusIndicator(Set?.Synced === true));
    Row.appendChild(StatusCell);
    const ActionCell = document.createElement('td');
    const Actions = CreateElement('span', 'TimeOrderButtons');
    const Up = CreateElement('button', 'SecondaryButton CompactButton', '↑');
    const Down = CreateElement('button', 'SecondaryButton CompactButton', '↓');
    const Delete = CreateElement('button', 'SecondaryButton CompactButton', '×');
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
  const Direction = Button.dataset.direction === 'asc' ? 'desc' : 'asc';
  document.querySelectorAll('.TableSortButton').forEach(Item => {
    Item.dataset.direction = '';
    Item.removeAttribute('aria-sort');
  });
  Button.dataset.direction = Direction;
  CentralTimeSortKey = Column;
  Button.setAttribute('aria-sort', Direction === 'asc' ? 'ascending' : 'descending');
  const CellIndex = {device: 1, data: 2, status: 3}[Column];
  const EditedData = Column === 'data' ? new Map(CollectCentralTimes().map(Entry => [Entry.Id, JSON.stringify(Entry.Data)])) : null;
  Rows.sort((Left, Right) => {
    const A = Column === 'data' ? EditedData.get(Left.dataset.id) : Column === 'status' ? Left.cells[CellIndex].dataset.sortValue : Left.cells[CellIndex].textContent;
    const B = Column === 'data' ? EditedData.get(Right.dataset.id) : Column === 'status' ? Right.cells[CellIndex].dataset.sortValue : Right.cells[CellIndex].textContent;
    return String(A).localeCompare(String(B), undefined, {numeric: true}) * (Direction === 'asc' ? 1 : -1);
  });
  Body.replaceChildren(...Rows);
  Rows.forEach((Row, Index) => Row.cells[0].textContent = String(Index + 1));
  Body.querySelectorAll('.TimeOrderButtons button:nth-child(-n+2)').forEach(Action => Action.disabled = true);
}

async function LoadCentralTimes() {
  const Message = document.getElementById('TimeMessage');
  try {
    const Url = new URL('api/times', window.location.href);
    Url.searchParams.set('_', Date.now().toString());
    const Response = await fetch(Url, {cache: 'no-store'});
    if (!Response.ok) throw new Error(`HTTP ${Response.status}`);
    const Store = await Response.json();
    if (!Array.isArray(Store.Sets) || !Array.isArray(Store.Times)) throw new Error('Ungültiges Format');
    CentralTimeStore = Store;
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
  const ChannelState = CentralTimeStore.Channels?.[Set?.Device];
  const ChannelRows = Array.isArray(ChannelState) ? ChannelState : ChannelState?.Rows;
  const ChannelId = ChannelRows?.[0]?.Id || '';
  if (Kind === 'Interval') {
    return ['00:00', '01:00', '00:15', 255, ChannelId];
  }
  return Set?.Format === 'TmrSw' ? ['00:00', 255, false] : ['00:00', 255, ChannelId, false];
}

function TimeChannelRows(Device) {
  const State = CentralTimeStore.Channels?.[Device];
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
  const Set = FindCentralSet(Entry.Device, Entry.Type);
  const Data = Entry.Data;
  const Editor = CreateElement('div', 'TimeDataEditor');
  const IsFeed = Entry.Kind === 'FeedTime';
  const IsInterval = Entry.Kind === 'Interval';
  const IsMyTimer = Set?.Format === 'MyTimer';
  const ChannelRows = TimeChannelRows(Entry.Device);
  const DefaultChannel = ChannelRows[0]?.Id || '';

  if (IsInterval) {
    AddTimeField(Editor, 'time', Tr('Start', 'Start'), 'time', Data[0]);
    AddTimeField(Editor, 'period', Tr('Period', 'Periode'), 'time', Data[1]);
    AddTimeField(Editor, 'onTime', Tr('OnTime', 'Ein-Zeit'), 'time', Data[2]);
  } else {
    AddTimeField(Editor, 'time', Tr('Time', 'Zeit'), 'time', Data[0]);
  }
  const MaskIndex = IsFeed ? 1 : IsInterval ? 3 : 1;
  AddTimeMask(Editor, Number(Data[MaskIndex]) || 0);

  if (IsMyTimer) {
    const ChannelIndex = IsInterval ? 4 : 2;
    AddTimeSelect(Editor, 'channel', Tr('Channel', 'Kanal'), ChannelRows.map(Row => ({Value: Row.Id, Label: ChannelDisplayName(Row)})), Data[ChannelIndex] || DefaultChannel);
  } else if (IsInterval || (!IsFeed && ChannelRows.length > 0)) {
    const ChannelId = IsInterval ? (Data[4] || DefaultChannel) : DefaultChannel;
    if (ChannelId)
      AddTimeSelect(Editor, 'channel', Tr('Channel', 'Kanal'), [{Value: ChannelId, Label: ChannelDisplayName({Id: ChannelId})}], ChannelId, true);
  }

  if (IsFeed) {
    AddTimeSelect(Editor, 'portion', Tr('Portions', 'Portionen'), Array.from({length: 9}, (_, Index) => ({Value: Index + 1, Label: String(Index + 1)})), Data[2]);
  } else if (!IsInterval) {
    AddTimeField(Editor, 'active', Tr('OnOff', 'An/Aus'), 'checkbox', Data[IsMyTimer ? 3 : 2]);
  }
  return Editor;
}

function AddCentralTime() {
  const SetKey = document.getElementById('TimeSetSelector').value;
  const Set = CentralTimeStore.Sets.find(Item => CentralSetKey(Item.Device, Item.Type) === SetKey);
  if (!Set) return;
  const Kind = document.getElementById('TimeKindSelector').value;
  CentralTimeStore.Times = CollectCentralTimes();
  CentralTimeStore.Times.push({
    Id: NewTimeUuid(),
    Device: Set.Device,
    Type: Set.Type,
    Kind,
    Data: DefaultCentralTimeData(Set, Kind)
  });
  RenderCentralTimes();
}

function CollectCentralTimes() {
  const Entries = new Map();
  for (const Row of document.getElementById('TimesData').rows) {
    const Existing = CentralTimeStore.Times.find(Item => Item.Id === Row.dataset.id);
    const Set = FindCentralSet(Row.dataset.device, Row.dataset.type);
    if (!Existing || !Set) continue;
    const Editor = Row.querySelector('.TimeDataEditor');
    const Field = Key => Editor.querySelector(`[data-field="${Key}"]`);
    let Mask = Field('enabled').checked ? 128 : 0;
    for (const Day of Editor.querySelectorAll('[data-field="day"]')) if (Day.checked) Mask |= Number(Day.dataset.bit);
    let Data;
    if (Existing.Kind === 'FeedTime') Data = [Field('time').value, Mask, Number(Field('portion').value)];
    else if (Existing.Kind === 'Interval') Data = [Field('time').value, Field('period').value, Field('onTime').value, Mask, Field('channel')?.value ?? Existing.Data[4] ?? ''];
    else if (Set.Format === 'MyTimer') Data = [Field('time').value, Mask, Field('channel').value, Field('active').checked];
    else {
      const Active = Field('active').checked;
      const OriginalActive = Existing.Data[2];
      Data = [Field('time').value, Mask, typeof OriginalActive === 'number' ? Number(Active) : Active];
    }
    Entries.set(Row.dataset.id, {Id: Row.dataset.id, Device: Row.dataset.device, Type: Row.dataset.type, Kind: Row.dataset.kind, Data});
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
      body: JSON.stringify({Version: 1, Times}, null, 2)
    });
    const Text = await Response.text();
    if (!Response.ok) throw new Error(Text || `HTTP ${Response.status}`);
    const Result = JSON.parse(Text);
    if (Result.Ok !== true) throw new Error(Result.Error || 'Speichern fehlgeschlagen');
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
  Message.replaceChildren(CreateElement('span', '', `${Tr('Revision', 'Revision')}: ${Store.Revision || '?'}`));
  const Label = Participant => {
    const Config = Devices.find(Device => Device.TechnicalName === Participant.Device)?.Configuration;
    const Name = DeviceDisplayName(Participant.Device);
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
      `${Label(Set)}${Set.Error ? ': ' + Set.Error : ''}`).join(', ')}`));
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
    if (!Array.isArray(Store.Profiles) || !Array.isArray(Store.Sets)) throw new Error('Ungültiges Format');
    CentralHeatProfiles = {Profiles: Store.Profiles};
    const TimeResponse = await fetch(new URL('api/times', window.location.href), {cache: 'no-store'});
    if (!TimeResponse.ok) throw new Error(`Times HTTP ${TimeResponse.status}`);
    CentralChannelStore = await TimeResponse.json();
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
  Button.disabled = true;
  try {
    const Profiles = CollectHeatProfiles();
    const Response = await fetch(new URL('api/heatprofiles', window.location.href), {
      method: 'PUT', cache: 'no-store', headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({Profiles}, null, 2)
    });
    const Text = await Response.text();
    if (!Response.ok) throw new Error(Text || `HTTP ${Response.status}`);
    const Result = JSON.parse(Text);
    if (Result.Ok !== true) throw new Error(Result.Error || 'Speichern fehlgeschlagen');
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
  for (const [Device, State] of Object.entries(CentralChannelStore.Channels || {})) {
    const Rows = Array.isArray(State) ? State : State?.Rows;
    for (const Row of Rows || [])
      if (Row.Profile === ProfileId) References.push(`${Device} / ${Row.Id || '?'}`);
  }
  return References;
}

function RenderHeatProfiles() {
  const Editor = document.getElementById('HeatProfilesEditor');
  const Cards = CentralHeatProfiles.Profiles.map((Profile, Index) => {
    const Card = CreateElement('fieldset', 'HeatProfileCard');
    const Legend = CreateElement('legend', '', Profile.Id || `${Tr('Profile', 'Profil')} ${Index + 1}`);
    Card.appendChild(Legend);
    for (const [Key, Type] of [['Id', 'text'], ['TempId', 'text'], ['Measurement', 'text'], ['FloorOn', 'number'], ['FloorOff', 'number'], ['MainOn', 'number'], ['MainOff', 'number'], ['NoTempOn', 'checkbox']]) {
      const Label = CreateElement('label', 'HeatProfileField');
      Label.appendChild(CreateElement('span', '', Tr(Key, Key)));
      const Input = document.createElement('input');
      Input.dataset.key = Key;
      Input.type = Type;
      if (Type === 'checkbox') Input.checked = Profile[Key] === true;
      else { Input.value = Profile[Key] ?? ''; if (Key === 'Measurement') Input.placeholder = 'Temperature'; if (Type === 'number') Input.step = 'any'; }
      Input.addEventListener('input', () => { if (Key === 'Id') Legend.textContent = Input.value; });
      Label.appendChild(Input);
      if (Key === 'Measurement') {
        const Choices = document.createElement('datalist');
        Choices.id = `HeatProfileMeasurement${Index}`;
        Input.setAttribute('list', Choices.id);
        for (const Field of ['Temperature', 'BatteryPercent']) {
          const Option = document.createElement('option');
          Option.value = Field;
          Choices.appendChild(Option);
        }
        Label.appendChild(Choices);
      }
      Card.appendChild(Label);
    }
    const References = ProfileReferences(Profile.Id);
    const Delete = CreateElement('button', 'SecondaryButton CompactButton', Tr('Delete', 'Löschen'));
    Delete.type = 'button';
    Delete.disabled = References.length > 0;
    Delete.title = References.length ? `${Tr('ProfileInUse', 'Noch verwendet von')}: ${References.join(', ')}` : '';
    Delete.onclick = () => { CentralHeatProfiles.Profiles.splice(Index, 1); RenderHeatProfiles(); };
    Card.appendChild(Delete);
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
      Profile: Row.querySelector('[data-field="profile"]').value
    });
}

function UpdateChannelAssignmentOptions() {
  const Selector = document.getElementById('ChannelAssignmentSelector');
  if (!Selector) return;
  const Options = [{Value: '', Label: Tr('SelectChannel', 'Kanal auswählen')}];
  for (const [Device, State] of Object.entries(CentralChannelStore.Channels || {})) {
    const ChannelRows = Array.isArray(State) ? State : State?.Rows || [];
    ChannelRows.forEach((Channel, Index) => {
      const Draft = CentralChannelDrafts.get(ChannelDraftKey(Device, Index));
      const Role = Draft?.Deleted ? '' : (Draft?.Role || Channel.Role || '');
      if (['HeaterMain', 'HeaterFloor'].includes(Role)) return;
      Options.push({
        Value: ChannelDraftKey(Device, Index),
        Label: `${DeviceDisplayName(Device)} · ${ChannelDisplayName(Channel, Device)}`
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
  for (const Label of ['Device', 'Channel', 'Role', 'HeatProfile'])
    Header.appendChild(CreateElement('strong', '', Tr(Label, Label)));
  Header.appendChild(CreateElement('span', '', ''));
  Rows.push(Header);
  for (const [Device, State] of Object.entries(CentralChannelStore.Channels || {})) {
    const ChannelRows = Array.isArray(State) ? State : State?.Rows || [];
    ChannelRows.forEach((Channel, Index) => {
      const Key = ChannelDraftKey(Device, Index);
      const Draft = CentralChannelDrafts.get(Key);
      if (Draft?.Deleted) return;
      const Role = Draft?.Role || Channel.Role;
      if (!['HeaterMain', 'HeaterFloor'].includes(Role)) return;
      const Row = document.createElement('div');
      Row.className = 'ChannelProfileRow';
      Row.dataset.device = Device;
      Row.dataset.index = String(Index);
      Row.append(
        CreateElement('span', 'ChannelProfileDevice', DeviceDisplayName(Device)),
        CreateElement('span', 'ChannelProfileName', ChannelRows.length > 1 ? ChannelDisplayName(Channel, Device) : '-')
      );
      const RoleOptions = ['HeaterMain', 'HeaterFloor'].map(Item => ({Value: Item, Label: Tr(`Role.${Item}`, Item)}));
      const RoleSelect = AddTimeSelect(Row, 'role', Tr('Role', 'Funktion'), RoleOptions, Role);
      RoleSelect.setAttribute('aria-label', Tr('Role', 'Funktion'));
      const Options = [{Value: '', Label: Tr('SelectProfile', 'Profil auswählen')}];
      for (const Profile of CentralHeatProfiles.Profiles)
        Options.push({Value: Profile.Id, Label: Profile.Id});
      const ProfileSelect = AddTimeSelect(Row, 'profile', Tr('HeatProfile', 'Heizprofil'), Options, Draft?.Profile ?? Channel.Profile ?? '');
      ProfileSelect.setAttribute('aria-label', Tr('HeatProfile', 'Heizprofil'));
      const Delete = CreateElement('button', 'SecondaryButton CompactButton ChannelProfileDelete', Tr('Delete', 'Löschen'));
      Delete.type = 'button';
      Delete.addEventListener('click', () => {
        CaptureChannelProfileDrafts();
        CentralChannelDrafts.set(Key, {Deleted: true});
        RenderChannelProfiles();
      });
      Row.appendChild(Delete);
      Rows.push(Row);
    });
  }
  Editor.replaceChildren(...Rows);
  UpdateChannelAssignmentOptions();
}

function AddChannelProfileAssignment() {
  CaptureChannelProfileDrafts();
  const Selector = document.getElementById('ChannelAssignmentSelector');
  if (!Selector.value) return;
  const [Device, IndexText] = Selector.value.split('\n');
  const Index = Number(IndexText);
  CentralChannelDrafts.set(ChannelDraftKey(Device, Index), {
    Role: 'HeaterMain',
    Profile: CentralHeatProfiles.Profiles[0]?.Id || ''
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
    const Channels = JSON.parse(JSON.stringify(CentralChannelStore.Channels || {}));
    for (const [Device, State] of Object.entries(Channels)) {
      const ChannelRows = Array.isArray(State) ? State : State.Rows;
      ChannelRows.forEach((Channel, Index) => {
        const Draft = CentralChannelDrafts.get(ChannelDraftKey(Device, Index));
        if (!Draft || Draft.Deleted) return;
        Channel.Role = Draft.Role;
        if (['HeaterMain', 'HeaterFloor'].includes(Draft.Role)) {
          if (!Draft.Profile) throw new Error(`${DeviceDisplayName(Device)} / ${ChannelDisplayName(Channel, Device)}: ${Tr('ProfileRequired', 'Heizprofil erforderlich')}`);
          Channel.Profile = Draft.Profile;
        } else delete Channel.Profile;
      });
      State.Rows = ChannelRows.filter((Channel, Index) => !CentralChannelDrafts.get(ChannelDraftKey(Device, Index))?.Deleted);
    }
    const Response = await fetch(new URL('api/times', window.location.href), {
      method: 'PUT', cache: 'no-store', headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({Version: 1, Times: CollectCentralTimes(), Channels}, null, 2)
    });
    const Text = await Response.text();
    if (!Response.ok) throw new Error(Text || `HTTP ${Response.status}`);
    const Result = JSON.parse(Text);
    if (Result.Ok !== true) throw new Error(Result.Error || Tr('SaveFailed', 'Speichern fehlgeschlagen'));
    CentralChannelDrafts.clear();
    const TimesReloaded = await LoadCentralTimes();
    CentralChannelStore = CentralTimeStore;
    RenderChannelProfiles();
    const ProfileStore = await FetchDashboardJson('api/heatprofiles');
    if (!Array.isArray(ProfileStore.Sets)) throw new Error('Ungültiges Profil-Synchronisationsformat');
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

function AddHeatProfile() {
  CentralHeatProfiles.Profiles.push({Id: '', TempId: '', FloorOn: 0, FloorOff: 1, MainOn: 0, MainOff: 1, NoTempOn: false});
  RenderHeatProfiles();
  RenderChannelProfiles(true);
}

function CollectHeatProfiles() {
  const Profiles = [];
  const Ids = new Set();
  for (const [Index, Card] of [...document.querySelectorAll('.HeatProfileCard')].entries()) {
    const Profile = {};
    for (const Input of Card.querySelectorAll('[data-key]')) {
      if (Input.type === 'checkbox') Profile[Input.dataset.key] = Input.checked;
      else if (Input.type === 'number') {
        if (Input.value.trim() === '' || !Number.isFinite(Number(Input.value))) throw new Error(`${Input.dataset.key}: ${Tr('InvalidNumber', 'ungültige Zahl')}`);
        Profile[Input.dataset.key] = Number(Input.value);
      } else Profile[Input.dataset.key] = Input.value.trim();
    }
    if (!Profile.Measurement) delete Profile.Measurement;
    else if (!/^[A-Za-z0-9_]{1,64}$/.test(Profile.Measurement) || Profile.Measurement === 'Kind') throw new Error('Measurement: ' + Tr('Invalid', 'ungültig'));
    if (!Profile.Id || !Profile.TempId) throw new Error(`${Tr('Profile', 'Profil')} ${Index + 1}: Id/TempId ${Tr('Required', 'erforderlich')}`);
    if (Ids.has(Profile.Id)) throw new Error(`${Tr('Profile', 'Profil')}: ${Profile.Id} ${Tr('Duplicate', 'doppelt')}`);
    if (!(Profile.FloorOn < Profile.FloorOff) || !(Profile.MainOn < Profile.MainOff))
      throw new Error(`${Profile.Id}: ${Tr('ThresholdsInvalid', 'Ein-Temperatur muss unter Aus-Temperatur liegen')}`);
    Ids.add(Profile.Id);
    Profiles.push(Profile);
  }
  const NewIds = new Set(Profiles.map(Profile => Profile.Id));
  for (const Profile of CentralHeatProfiles.Profiles) {
    if (!NewIds.has(Profile.Id)) {
      const References = ProfileReferences(Profile.Id);
      if (References.length) throw new Error(`${Profile.Id} ${Tr('ProfileInUse', 'Noch verwendet von')}: ${References.join(', ')}`);
    }
  }
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
  ActiveConfigDevice.ConfigFrame?.contentWindow?.postMessage({Source: 'GnDashboard', Type: 'ConfigurationActivated'}, '*');
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

function UpdateDashboardTimestamp() {
  SetDashboardStatus(`${Tr('Updated', 'Aktualisiert')} ${FormatShortTime(new Date())}`);
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
      Data: VersionInfo.DataVersion || '?'
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
    MarkDashboardUnavailable('WLAN oder Netzwerkverbindung ist unterbrochen. Automatische Wiederverbindung läuft.');
    return;
  }

  DashboardHealthRequestRunning = true;
  try {
    await FetchDashboardConfiguration();
    DashboardHealthDelay = DashboardHealthInitialMilliseconds;

    if (DashboardWasUnavailable) {
      SetDashboardConnectionState('Recovering', 'Verbindung wiederhergestellt. Dashboard wird neu geladen …');
      setTimeout(() => window.location.reload(), DashboardRecoveryReloadMilliseconds);
      return;
    }

    SetDashboardConnectionState('Online', '');
    ScheduleDashboardHealthCheck(DashboardHealthNormalMilliseconds);
  } catch (Error) {
    console.error('Dashboard health check failed', Error);
    MarkDashboardUnavailable(`Dashboard-Server nicht erreichbar: ${Error.message}. Neuer Versuch läuft automatisch.`);
  } finally {
    DashboardHealthRequestRunning = false;
  }
}

function HandleBrowserOffline() {
  for (const Device of Devices)
    SetConnectionState(Device, 'Offline');
  MarkDashboardUnavailable('WLAN oder Netzwerkverbindung ist unterbrochen. Automatische Wiederverbindung läuft.');
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
    for (const [Name, Id] of Object.entries(Configuration.ExternalSources || {}))
      DeviceConfigurations.push({Name, Function: Name, BaseUrl: '/', Group: 'System',
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
    MarkDashboardUnavailable(`Dashboard-Server nicht erreichbar: ${Error.message}. Neuer Versuch läuft automatisch.`);
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
  return [...(View?.Endpoints || []).filter(Endpoint => Endpoint.Capabilities !== 0).slice().sort((First, Second) => First.HardwareName.localeCompare(Second.HardwareName, undefined, {numeric: true})).map(Endpoint => ({
    ...Endpoint, Supported: Endpoint.Available !== false && (Endpoint.Capabilities === 2 || View.Devices.find(Device => Device.DeviceId === Endpoint.DeviceId)?.EndpointAssignments === 1), Value: Endpoint.EndpointId, Label: `${Endpoint.HardwareName} · ${AssignmentFunctionLabel(Endpoint.Function, Endpoint.Groups, Endpoint.UseHardwareName ? Endpoint.HardwareName : null)}`, Kind: 'Endpoint'
  })), ...(View?.Devices || []).filter(Device => Device.ServiceId === 3).map(Device => ({
    ...Device, Supported: Number.isInteger(Device.SensorCapabilityId) && Device.SensorCapabilityId >= 0, Value: `Device:${Device.Function}`, Label: `${Device.HardwareName || Device.Function} · ${Tr('FeederDeviceSwap', 'Feeder (whole device)')}`, Kind: 'Device'
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
      (First.Kind === 'Endpoint' ? First.Capabilities !== Candidate.Capabilities || !Candidate.Supported : First.CapabilityId !== Candidate.CapabilityId || !Candidate.Supported);
  }
  const SafetyLabel = document.querySelector('#AssignmentDetached + span');
  SafetyLabel.dataset.tr = First?.Kind === 'Endpoint' && First.Capabilities === 2 ? 'SensorAssignmentsChecked' : 'ConsumersDetached';
  SafetyLabel.textContent = Tr(SafetyLabel.dataset.tr);
  document.getElementById('AssignmentResume').hidden = !View?.Pending;
  document.getElementById('AssignmentResume').disabled = AssignmentLoading;
  document.getElementById('AssignmentSwap').disabled = !View || !View.AllApplied || View.Pending || AssignmentLoading ||
    !First || !Second || First === Second || First.Kind !== Second.Kind ||
    (First.Kind === 'Endpoint' ? First.Capabilities !== Second.Capabilities || !First.Capabilities || !First.Supported || !Second.Supported : First.ServiceId !== Second.ServiceId || First.CapabilityId !== Second.CapabilityId || !First.Supported || !Second.Supported) ||
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
      ...(View.Endpoints || []).filter(Endpoint => Endpoint.Capabilities !== 0).slice().sort((First, Second) => First.HardwareName.localeCompare(Second.HardwareName, undefined, {numeric: true})).map(Endpoint => ({...View.Devices.find(Device => Device.DeviceId === Endpoint.DeviceId),
        Function: Endpoint.Function, HardwareName: Endpoint.HardwareName, EndpointId: Endpoint.EndpointId, CapabilityId: Endpoint.Capabilities, EndpointGroups: Endpoint.Groups, UseHardwareName: Endpoint.UseHardwareName, EndpointAvailable: Endpoint.Available}))];
    document.getElementById('AssignmentRows').replaceChildren(...Rows.map(Device => {
      const Row = document.createElement('tr');
      const FirmwareReady = !Device.EndpointId || Device.CapabilityId !== 1 || Device.EndpointAssignments === 1;
      const Ready = FirmwareReady && Device.EndpointAvailable !== false && (Device.ServiceId === 2 || Device.Applied);
      const Address = Device.HttpPort && Device.HttpPort !== 80 ? `${Device.IP}:${Device.HttpPort}` : Device.IP;
      const FunctionLabel = Device.EndpointId
        ? `${AssignmentFunctionLabel(Device.Function, Device.EndpointGroups, Device.UseHardwareName ? Device.HardwareName : null)}${Device.CapabilityId === 2 ? ` · ${Tr('TemperatureSource', 'Temperature source')}` : ''}`
        : `${DeviceDisplayName(Device.Function)}${Device.ServiceId === 3 ? ` · ${Tr('FeedingFunction', 'Feeding')}` : ''}`;
      const CapabilityLabel = Device.EndpointId ? `${Device.CapabilityId}: ${Device.CapabilityId === 2 ? Tr('TemperatureSource', 'Temperature source') : Tr('SwitchCapability', 'Switching')}` : Device.ServiceId === 3 ? Tr('FeedingFunction', 'Feeding') : '—';
      for (const Text of [FunctionLabel, Address, Device.HardwareName ? `${Device.HardwareName} · ${Device.EndpointId || Device.DeviceId}` : Device.DeviceId, CapabilityLabel, Ready ? Tr('AssignmentReady') : Device.EndpointId && Device.EndpointAvailable === false ? Tr('SensorStateUnknown', 'Sensor state unknown') : !Device.Online ? Tr('Offline') : !FirmwareReady ? Tr('EndpointFirmwareUpdateRequired') : Tr('AssignmentNotReady')]) {
        const Cell = document.createElement('td'); Cell.textContent = Text; Row.appendChild(Cell);
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
    Message.classList.toggle('Success', !View.Error && !View.Pending && View.AllApplied);
    Message.textContent = View.Error || (View.Pending ? Tr('AssignmentPending') : View.AllApplied ? Tr('AssignmentComplete') : '');
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
      body: JSON.stringify({Operation, ExpectedRevision: AssignmentView.Revision,
        First: ChoiceValue('AssignmentFirst'), Second: ChoiceValue('AssignmentSecond'),
        ConsumersDetached: document.getElementById('AssignmentDetached').checked})
    });
    const Result = await Response.json();
    if (!Response.ok || !Result.Ok) throw new Error(Result.Error || `HTTP ${Response.status}`);
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
    DeviceEditorRevision = Data.Revision;
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
    if (!Response.ok || !Result.Ok) throw new Error(Result.Error || `HTTP ${Response.status}`);
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
  }).observe(Table.tHead, {childList: true, subtree: true});
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
  const Data = await Response.json();
  FeedbackEditorPrevious = Data.Configuration;
  document.getElementById('FeedbackEditorText').value = FormatEditorJson(Data.Configuration) + '\n';
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
  const Configuration = JSON.parse(document.getElementById('FeedbackEditorText').value);
  const Response = await fetch('api/feedback-monitors', {method: 'PUT', headers: {'Content-Type': 'application/json'},
    signal: AbortSignal.timeout(6000), body: JSON.stringify({Configuration, Previous: FeedbackEditorPrevious})});
  if (!Response.ok) throw new Error(await Response.text());
  await LoadFeedbackEditor();
  document.getElementById('FeedbackEditorMessage').textContent = Tr('FeedbackSaved', 'Überwachung gespeichert.');
}));
