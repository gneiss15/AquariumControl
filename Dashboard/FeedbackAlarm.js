// FeedbackAlarm.js, Version: 1.02
// A remembered preference may request audio; the browser still controls playback.
export class TFeedbackAlarm {
  constructor(OnChanged = () => {}, AudioFactory = () => new (window.AudioContext || window.webkitAudioContext)(), Now = () => performance.now()) {
    this.OnChanged = OnChanged;
    this.AudioFactory = AudioFactory;
    this.Now = Now;
    this.Context = null;
    this.Enabled = false;
    this.Requested = false;
    this.Active = new Set();
    this.Acknowledged = new Set();
    this.LastSound = -Infinity;
  }

  async Enable(Confirmation = true) {
    this.Requested = true;
    if (!this.Context || this.Context.state === 'closed') this.Context = this.AudioFactory();
    this.Context.onstatechange = () => this.OnChanged();
    this.OnChanged();
    await this.Context.resume();
    if (!this.Requested) return;
    if (this.Context.state !== 'running') throw new Error('AudioBlocked');
    this.Enabled = true;
    if (Confirmation) this.Beep(); // User action doubles as an audible test.
    this.OnChanged();
  }

  Disable() {
    this.Requested = false;
    this.Enabled = false;
    this.Context?.suspend();
    this.OnChanged();
  }

  Acknowledge() {
    this.Acknowledged = new Set(this.Active);
    this.OnChanged();
  }

  Update(Rows) {
    const Previous = this.Active;
    this.Active = new Set(Rows.filter(Row => Row.State === 'Mismatch').map(Row => `${Row.Id}:${Row.Reason}`));
    this.Acknowledged = new Set([...this.Acknowledged].filter(Key => this.Active.has(Key)));
    const NewFault = [...this.Active].some(Key => !Previous.has(Key));
    const Audible = [...this.Active].some(Key => !this.Acknowledged.has(Key));
    // No independent repeat timer: stale/disconnected dashboard data cannot keep beeping.
    if (this.Enabled && Audible && (NewFault || this.Now() - this.LastSound >= 30000)) this.Beep();
    this.OnChanged();
  }

  Beep() {
    if (!this.Enabled || this.Context?.state !== 'running') return;
    const Start = this.Context.currentTime;
    for (const Offset of [0, 0.25, 0.5]) {
      const Tone = this.Context.createOscillator();
      const Gain = this.Context.createGain();
      Tone.frequency.value = 880;
      Gain.gain.setValueAtTime(0, Start + Offset);
      Gain.gain.linearRampToValueAtTime(1.0, Start + Offset + 0.01);
      Gain.gain.linearRampToValueAtTime(0, Start + Offset + 0.16);
      Tone.connect(Gain);
      Gain.connect(this.Context.destination);
      Tone.start(Start + Offset);
      Tone.stop(Start + Offset + 0.18);
      Tone.onended = () => { Tone.disconnect(); Gain.disconnect(); };
    }
    this.LastSound = this.Now();
  }
}
