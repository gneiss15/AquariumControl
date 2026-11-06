# test_pi_profiles.py, Version: 1.02
"""Central API validates optional PI observation fields and preserves extensions."""
import copy,json,pathlib,urllib.error
from test_assignment_control import Fixture
f=Fixture()
try:
    profiles=f.request('/api/heatprofiles')['Profiles']
    profile=dict(profiles[0],Pi={'Mode':'Shadow','Setpoint':28,'FloorDevice':'02:11:22:33:44:55@1',
      'FloorChannel':'floor','FloorWatts':40,'MainMaxWatts':350,'CapacityWhPerK':590,
      'LossWattsPerK':27,'Kp':270,'TiSeconds':31200,'WindowSeconds':600,'DelaySeconds':600})
    profile['Pi']['FutureField']={'keep':True}
    assert f.request('/api/heatprofiles',{'Profiles':[profile]},'PUT')['Ok']
    assert f.request('/api/heatprofiles')['Profiles'][0]['Pi']==profile['Pi']
    for key,value in [('Mode','Active'),('Kp',True),('TiSeconds',0),('FloorDevice','GN-Pi')]:
        bad=copy.deepcopy(profile);bad['Pi'][key]=value
        try:
            f.request('/api/heatprofiles',{'Profiles':[bad]},'PUT')
            raise AssertionError('Invalid PI accepted')
        except urllib.error.HTTPError as e:assert e.code==400
    profile['Pi']['Mode']='Active';profile['Pi']['MainDevice']='02:11:22:33:44:55@4'
    assert f.request('/api/heatprofiles',{'Profiles':[profile]},'PUT')['Ok']
    print('PI profile API validation/preservation passed')
finally:f.close()
