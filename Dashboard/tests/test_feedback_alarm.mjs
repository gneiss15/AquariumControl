// test_feedback_alarm.mjs, Version: 1.03
import assert from 'node:assert/strict';
import {readFile} from 'node:fs/promises';
const Source = await readFile(new URL('../FeedbackAlarm.js', import.meta.url), 'utf8');
const {TFeedbackAlarm} = await import(`data:text/javascript;base64,${Buffer.from(Source).toString('base64')}`);
let Now = 0, Tones = 0;
const Levels = [];
const Parameter = {setValueAtTime() {}, linearRampToValueAtTime(Value) { Levels.push(Value); }};
const Context = {state: 'suspended', currentTime: 0, destination: {},
  async resume() { this.state = 'running'; }, suspend() { this.state = 'suspended'; },
  createOscillator() { return {frequency: {}, connect() {}, disconnect() {}, start() { ++Tones; }, stop() {}}; },
  createGain() { return {gain: Parameter, connect() {}, disconnect() {}}; }};
const Alarm = new TFeedbackAlarm(() => {}, () => Context, () => Now);
const Fault = [{Id: 'A', Reason: 'OnWithoutFeedback', State: 'Mismatch'}];
Alarm.Update(Fault); assert.equal(Tones, 0); // No autoplay before a gesture.
await Alarm.Enable(); assert.equal(Tones, 3);
assert.equal(Math.max(...Levels), 1.0, 'confirmation and alarms use full audio level');
Now = 31000; Alarm.Update(Fault); assert.equal(Tones, 6);
Alarm.Acknowledge(); Now = 62000; Alarm.Update(Fault); assert.equal(Tones, 6);
Alarm.Update([{...Fault[0], State: 'OK'}]); Alarm.Update(Fault); assert.equal(Tones, 9);
Alarm.Disable(); Now = 100000; Alarm.Update(Fault); assert.equal(Tones, 9);
Alarm.Update([{Id: 'A', State: 'Unknown'}]); assert.equal(Alarm.Active.size, 0);
console.log('Alarm: opt-in, repeat, acknowledgement, recovery/new fault, disabling and unknown state passed.');

const Reloaded = new TFeedbackAlarm(() => {}, () => Context, () => Now);
Reloaded.Update(Fault); assert.equal(Reloaded.Enabled, false); assert.equal(Tones, 9);

const Restored = new TFeedbackAlarm(() => {}, () => Context, () => Now);
const BeforeRestore = Tones;
await Restored.Enable(false);
assert(Restored.Enabled && Restored.Requested);
assert.equal(Tones, BeforeRestore, 'automatic restoration must not play a confirmation');
Restored.Update(Fault); assert.equal(Tones, BeforeRestore + 3);
Restored.Disable(); assert.equal(Restored.Requested, false);
const BlockedContext = {...Context, state: 'suspended', async resume() {}};
const Blocked = new TFeedbackAlarm(() => {}, () => BlockedContext, () => Now);
await assert.rejects(Blocked.Enable(false), /AudioBlocked/);
assert(Blocked.Requested && !Blocked.Enabled, 'remember ON but show release required');
BlockedContext.resume = async function() { this.state = 'running'; };
await Blocked.Enable(); assert(Blocked.Enabled);
let Resume;
const PendingContext = {...Context, state: 'suspended', resume() { return new Promise(Resolve => { Resume = Resolve; }); }};
const Pending = new TFeedbackAlarm(() => {}, () => PendingContext, () => Now);
const Wait = Pending.Enable(false); Pending.Disable();
PendingContext.state = 'running'; Resume(); await Wait;
assert(!Pending.Enabled && !Pending.Requested, 'late resume must not override OFF');

const Dashboard = await readFile(new URL('../Dashboard.js', import.meta.url), 'utf8');
const {runInNewContext} = await import('node:vm');
const First = Dashboard.indexOf('function ReadFeedbackSoundPreference()');
const Last = Dashboard.indexOf('const FeedbackAlarm =', First);
let Cookie = '', Written = '';
const Document = {get cookie() {return Cookie;}, set cookie(Value) {Written = Value; Cookie = Value.split(';')[0];}};
const CookieContext = {document: Document, console};
runInNewContext("const FeedbackSoundCookieName='AquariumDashboard.FeedbackSound';\n" + Dashboard.slice(First, Last), CookieContext);
assert.equal(CookieContext.ReadFeedbackSoundCookie(), false);
CookieContext.StoreFeedbackSoundCookie(true);
assert(CookieContext.ReadFeedbackSoundCookie());
assert(Written.includes('Max-Age=31536000; Path=/; SameSite=Lax'));
CookieContext.StoreFeedbackSoundCookie(false);
assert.equal(CookieContext.ReadFeedbackSoundCookie(), false);
console.log('Preference: cookie ON/OFF, silent restore, blocked/released audio and late resume passed.');

let CurrentTime = 1800000000000;
CookieContext.Date = {now: () => CurrentTime};
Cookie = 'AquariumDashboard.FeedbackSound=1';
CookieContext.RenewFeedbackSoundCookie();
assert(CookieContext.ReadFeedbackSoundCookie(), 'legacy ON is retained');
const FirstExpiry = CookieContext.ReadFeedbackSoundPreference().Expires;
assert.equal(FirstExpiry, Math.floor(CurrentTime / 1000) + 31536000);
Written = '';
CurrentTime += 300 * 86400000;
CookieContext.RenewFeedbackSoundCookie(); assert.equal(Written, '', 'do not renew too early');
CurrentTime += 36 * 86400000;
CookieContext.RenewFeedbackSoundCookie();
assert(CookieContext.ReadFeedbackSoundPreference().Expires > FirstExpiry);
assert(CookieContext.ReadFeedbackSoundCookie());
CookieContext.StoreFeedbackSoundCookie(false);
CurrentTime += 336 * 86400000;
CookieContext.RenewFeedbackSoundCookie();
assert.equal(CookieContext.ReadFeedbackSoundCookie(), false, 'OFF is extended too');
Cookie = ''; Written = '';
CookieContext.RenewFeedbackSoundCookie(); assert.equal(Written, '', 'do not recreate deleted preferences');
Cookie = 'AquariumDashboard.FeedbackSound=garbage';
CookieContext.RenewFeedbackSoundCookie(); assert.equal(Written, '');
console.log('Cookie renewal: legacy migration, last-month threshold, ON/OFF and missing/invalid values passed.');
