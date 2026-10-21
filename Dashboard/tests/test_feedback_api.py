# test_feedback_api.py, Version: 1.00
# Loopback-only persistence, validation and stale-editor regression.
import json, urllib.error
from test_assignment_control import Fixture
f=Fixture()
try:
 original=f.request('/api/feedback-monitors')['Configuration']
 assert original=={'Version':1,'Monitors':[]}
 rule={'Version':1,'Monitors':[{'Id':'Test','SwitchFunction':'HeaterMainAq1','Source':'FeederAq1','StatusKind':'Power','FeedbackField':'AcOnline','GraceSeconds':60}]}
 def put(configuration,previous):
  return f.request('/api/feedback-monitors',{'Configuration':configuration,'Previous':previous},'PUT')
 assert put(rule,original)['Ok']
 assert json.loads((f.root/'data/FeedbackMonitors.json').read_text())==rule
 assert (f.root/'data/FeedbackMonitors.json').stat().st_mode&0o777==0o600
 try:put(original,original);raise AssertionError('Stale editor accepted')
 except urllib.error.HTTPError as e:assert e.code==409
 invalid=json.loads(json.dumps(rule));invalid['Monitors'][0]['GraceSeconds']=0
 try:put(invalid,rule);raise AssertionError('Invalid grace accepted')
 except urllib.error.HTTPError as e:assert e.code==400
 f.stop();f.start();assert f.request('/api/feedback-monitors')['Configuration']==rule
 assert put(original,rule)['Ok'];assert not list((f.root/'data').glob('.FeedbackMonitors.*'))
 print('Feedback API: save, private atomic file, stale/invalid rejection, restart persistence and removal passed.')
finally:f.close()
