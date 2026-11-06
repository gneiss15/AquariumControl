// StatusClient.js, Version: 1.03
import './Messages.js';

export class TMultiTimerStatusClient {
  constructor(BaseUrl, OnStatus, OnConnectionChanged = () => {}) {
    this.BaseUrl = new URL(BaseUrl, window.location.href);
    this.OnStatus = OnStatus;
    this.OnConnectionChanged = OnConnectionChanged;
    this.Dev = null;
    this.Socket = null;
    this.PollTimer = null;
    this.ReconnectTimer = null;
    this.ReconnectDelay = 1000;
    this.Running = false;
  }

  async Start() {
    if (this.Running) return;
    this.Running = true;
    this.Dev = await this.GetJson('/api/v1/device');
    await this.PollStatus();
    this.Connect();
  }

  Stop() {
    this.Running = false;
    this.StopPolling();
    if (this.ReconnectTimer !== null) {
      clearTimeout(this.ReconnectTimer);
      this.ReconnectTimer = null;
    }
    if (this.Socket !== null) {
      this.Socket.onclose = null;
      this.Socket.close();
      this.Socket = null;
    }
    this.OnConnectionChanged(false);
  }

  GetJson(Path) {
    return fetch(new URL(Path, this.BaseUrl), {cache: 'no-store'}).then(Response => {
      if (!Response.ok) throw new Error(`HTTP ${Response.status}`);
      return Response.text().then(Text => GnJson.Read(Path === '/api/v1/device' ? 'THttpDevice' : 'TUdpStatusData', Text));
    });
  }

  Connect() {
    if (!this.Running || !this.Dev?.Events) return;
    const Events = this.Dev.Events;
    const Url = new URL(Events.Url);

    let Socket;
    try {
      Socket = new WebSocket(Url.toString());
    } catch (Error) {
      console.error('Cannot open MultiTimer WebSocket', Error);
      this.StartPolling();
      this.ScheduleReconnect();
      return;
    }
    this.Socket = Socket;
    Socket.onopen = () => {
      if (this.Socket !== Socket) return;
      this.ReconnectDelay = 1000;
      this.StopPolling();
      this.OnConnectionChanged(true);
    };
    Socket.onmessage = Event => {
      if (this.Socket !== Socket) return;
      try {
        const Message = GnJson.Read('TWsStatusEvent', Event.data);
        if (Message.Status)
          this.OnStatus(Message.Status, Message.Seq);
      } catch (Error) {
        console.error('Invalid MultiTimer WebSocket message', Error);
      }
    };
    Socket.onerror = () => Socket.close();
    Socket.onclose = () => {
      if (this.Socket !== Socket) return;
      this.Socket = null;
      this.OnConnectionChanged(false);
      this.StartPolling();
      this.ScheduleReconnect();
    };
  }

  ScheduleReconnect() {
    if (!this.Running || this.ReconnectTimer !== null) return;
    this.ReconnectTimer = setTimeout(() => {
      this.ReconnectTimer = null;
      this.Connect();
    }, this.ReconnectDelay);
    this.ReconnectDelay = Math.min(this.ReconnectDelay * 2, 30000);
  }

  StartPolling() {
    if (!this.Running || this.PollTimer !== null) return;
    const Seconds = this.Dev?.Events?.PollingFallbackSec || 5;
    this.PollTimer = setInterval(() => this.PollStatus(), Seconds * 1000);
    this.PollStatus();
  }

  StopPolling() {
    if (this.PollTimer === null) return;
    clearInterval(this.PollTimer);
    this.PollTimer = null;
  }

  async PollStatus() {
    if (!this.Running) return;
    try {
      this.OnStatus(await this.GetJson('/api/v1/status'), null);
    } catch (Error) {
      console.error('MultiTimer status polling failed', Error);
    }
  }
}
