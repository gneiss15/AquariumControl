// StatusClient.js, Version: 1.05
const RequestTimeoutMilliseconds = 6000;
const PollIntervalMilliseconds = 1000;

// One same-origin event stream serves every device card on this page.
const Subscribers = new Set();
let StatusEvents = null;
let EventWatchdog = null;
let LastEventAt = 0;
let PushConnected = false;

function SubscribeStatus(Client) {
  Subscribers.add(Client);
  if (typeof EventSource === 'undefined') return;
  if (StatusEvents) { Client.UsePush(PushConnected); return; }
  try { StatusEvents = new EventSource(new URL('api/events', window.location.href)); }
  catch (Error) { console.error('Dashboard push unavailable', Error); return; }
  LastEventAt = Date.now();
  const Events = StatusEvents;
  Events.onmessage = Event => {
    if (StatusEvents !== Events) return;
    try {
      const Batch = JSON.parse(Event.data);
      if (!Array.isArray(Batch)) throw new Error('Invalid status stream');
      LastEventAt = Date.now();
      PushConnected = true;
      for (const Subscriber of Subscribers) Subscriber.UsePush(true);
      for (const Data of Batch)
        for (const Subscriber of Subscribers)
          if (Data.ID === Subscriber.ID) Subscriber.AcceptStatus(Data);
    } catch (Error) { console.error('Dashboard push failed', Error); }
  };
  Events.onerror = () => {
    if (StatusEvents !== Events) return;
    PushConnected = false;
    for (const Subscriber of Subscribers) Subscriber.UsePush(false);
  };
  EventWatchdog = setInterval(() => {
    if (Date.now() - LastEventAt <= 15000) return;
    PushConnected = false;
    for (const Subscriber of Subscribers) Subscriber.UsePush(false);
    StatusEvents?.close();
    StatusEvents = null;
    if (EventWatchdog !== null) clearInterval(EventWatchdog);
    EventWatchdog = null;
    const First = Subscribers.values().next().value;
    if (First) SubscribeStatus(First);
  }, 5000);
}

function UnsubscribeStatus(Client) {
  Subscribers.delete(Client);
  if (Subscribers.size) return;
  StatusEvents?.close();
  StatusEvents = null;
  if (EventWatchdog !== null) clearInterval(EventWatchdog);
  EventWatchdog = null;
  PushConnected = false;
}

export class TDeviceStatusClient {
  constructor(BaseUrl, OnStatus, OnConnectionChanged = () => {}, CachedId = null) {
    this.BaseUrl = new URL(BaseUrl, window.location.href);
    this.OnStatus = OnStatus;
    this.OnConnectionChanged = OnConnectionChanged;
    this.Device = null;
    this.ID = null;
    this.CachedId = CachedId;
    this.LastRevision = null;
    this.PollTimer = null;
    this.Running = false;
    this.Online = false;
    this.PushActive = false;
    this.Polling = false;
    this.Generation = 0;
  }

  async Start() {
    if (this.Running) return;
    this.Running = true;
    try {
      this.Device = this.CachedId ? {DeviceId: this.CachedId}
        : await this.GetJson('/api/v1/device', this.BaseUrl);
      this.ID = String(this.Device?.DeviceId || this.BaseUrl.hostname);
      await this.PollStatus();
      if (!this.Running) return;
      this.UsePush(false);
      SubscribeStatus(this);
    } catch (Error) {
      this.Running = false;
      throw Error;
    }
  }

  Stop() {
    this.Running = false;
    ++this.Generation;
    UnsubscribeStatus(this);
    if (this.PollTimer !== null) {
      clearInterval(this.PollTimer);
      this.PollTimer = null;
    }
    this.SetOnline(false);
  }

  SetOnline(Online) {
    if (this.Online === Online) return;
    this.Online = Online;
    this.OnConnectionChanged(Online);
  }

  async GetJson(Path, BaseUrl = window.location.href) {
    const Controller = new AbortController();
    const Timeout = setTimeout(() => Controller.abort(), RequestTimeoutMilliseconds);
    try {
      const Response = await fetch(new URL(Path, BaseUrl), {
        cache: 'no-store',
        signal: Controller.signal
      });
      if (!Response.ok) throw new Error(`HTTP ${Response.status}`);
      return Response.json();
    } catch (Error) {
      if (Error.name === 'AbortError')
        throw new Error(`Zeitüberschreitung nach ${RequestTimeoutMilliseconds / 1000} Sekunden`);
      throw Error;
    } finally {
      clearTimeout(Timeout);
    }
  }

  UsePush(Active) {
    if (Active && !this.PushActive) this.LastRevision = null;
    this.PushActive = Active;
    if (Active && this.PollTimer !== null) {
      clearInterval(this.PollTimer);
      this.PollTimer = null;
    } else if (!Active && this.Running && this.PollTimer === null) {
      this.PollTimer = setInterval(() => this.PollStatus(), PollIntervalMilliseconds);
    }
  }

  AcceptStatus(Data) {
    if (!this.Running) return;
    this.SetOnline(Data.Online === true);
    if (Data.Online !== true) {
      this.LastRevision = null;
      return;
    }
    if (Data.Status !== null && Data.Revision !== this.LastRevision) {
      this.LastRevision = Data.Revision;
      this.OnStatus(Data.Status, null);
    }
  }

  async PollStatus() {
    if (!this.Running || this.ID === null || this.Polling || this.PushActive) return;
    this.Polling = true;
    const Generation = this.Generation;
    try {
      const Data = await this.GetJson(`/api/device-status/${encodeURIComponent(this.ID)}`);
      if (!this.PushActive && Generation === this.Generation) this.AcceptStatus(Data);
    } catch (Error) {
      if (this.Running && !this.PushActive && Generation === this.Generation) {
        console.error(`Dashboard status cache failed for ${this.ID}`, Error);
        this.SetOnline(false);
      }
    } finally { this.Polling = false; }
  }
}
