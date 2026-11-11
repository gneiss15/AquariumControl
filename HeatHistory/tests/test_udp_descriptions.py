# test_udp_descriptions.py, Version: 1.04
import json
import pathlib
import sys
import unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]))
from UdpDescriptions import UdpDescriptions
from Messages import TMsgType, BITS

class DescriptionTests(unittest.TestCase):
  def frame(self, **changes):
    return dict(dict(Ver=3,Type=int(TMsgType.Status),ID='02:11:22:33:44:55@1',Compact=True,
                     InfoRev='0123456789abcdef',Status=dict(CurrentTime=100,Chs=[],Meas=[])),**changes)

  def info(self, frame, field, **changes):
    data={'Root':{},'Identity':{'Ver':1}}.get(field,{})
    return dict(dict(Ver=3,Type=int(TMsgType.Info),ID=frame['ID'],InfoRev=frame['InfoRev'],
                     Field=field,Data=json.dumps(data)),**changes)

  def test_malformed_frames_are_ignored(self):
    decoder=UdpDescriptions()
    for status in [None,dict(Chs=None),dict(Meas={}),dict(Chs=['invalid']),
                   dict(Chs=[dict(Id=0,Bits='bad')]),dict(Chs=[dict(Id=0,Bits=-1)]),
                   dict(Chs=[dict(Id={},Bits=0)]),dict(Meas=[dict(Kind=[])])]:
      self.assertEqual(decoder.OnUdpPacket(self.frame(Status=status),0,0),(None,None))
    for packet in [b'bad json',b'"\xff"',[],self.frame(Ver=True),self.frame(Ver=2.0),
                   self.frame(ID=[]),self.frame(ID=''),self.frame(Type=int(TMsgType.Request)),self.frame(ID='bad')]:
      self.assertEqual(decoder.OnUdpPacket(packet,0,0),(None,None))
    self.assertEqual(decoder.pending,{})

  def test_requested_metadata_preserves_both_receive_times(self):
    decoder=UdpDescriptions();frame=self.frame()
    sample,request=decoder.OnUdpPacket(json.dumps(frame).encode(),1,100)
    self.assertIsNone(sample);self.assertEqual(request['Fields'],['Identity','Root'])
    self.assertEqual(decoder.OnUdpPacket(self.info(frame,'Root'),500,600),(None,None))
    sample,request=decoder.OnUdpPacket(self.info(frame,'Identity'),1000,1100)
    self.assertIsNone(request)
    packet,received,wall=sample
    self.assertEqual((received,wall),(1,100))
    self.assertEqual(packet['Status']['CurrentTime'],100)
    self.assertEqual(decoder.OnUdpPacket(self.info(frame,'Identity'),2000,2100),(None,None))

  def test_legacy_double_wrapped_info_is_unwrapped(self):
    decoder=UdpDescriptions();frame=self.frame()
    decoder.OnUdpPacket(frame,1,100)
    root=self.info(frame,'Root',Data='{{}}')
    identity=self.info(frame,'Identity',Data='{{"Ver":1}}')
    self.assertEqual(decoder.OnUdpPacket(root,2,200),(None,None))
    sample,_=decoder.OnUdpPacket(identity,3,300)
    self.assertEqual(sample[0]['Status']['Identity'],{'Ver':1})
    self.assertEqual(sample[1:],(1,100))

  def test_assignment_filter_and_full_status(self):
    decoder=UdpDescriptions();frame=self.frame(Compact=False)
    self.assertEqual(decoder.OnUdpPacket(frame,1,2,[]),(None,None))
    self.assertEqual(decoder.OnUdpPacket(frame,1,2,[frame['ID']]),((frame,1,2),None))

  def test_revision_change_discards_old_sample(self):
    decoder=UdpDescriptions();old=self.frame();new=self.frame(InfoRev='fedcba9876543210',Status=dict(CurrentTime=200,Chs=[],Meas=[]))
    decoder.OnUdpPacket(old,1,2);decoder.OnUdpPacket(new,3,4)
    self.assertEqual(decoder.OnUdpPacket(self.info(old,'Root'),5,6),(None,None))
    decoder.OnUdpPacket(self.info(new,'Root'),5,6)
    sample,_=decoder.OnUdpPacket(self.info(new,'Identity'),7,8)
    self.assertEqual(sample[0]['Status']['CurrentTime'],200)
    self.assertEqual(sample[1:],(3,4))

  def test_switch_service_four_is_accepted(self):
    decoder=UdpDescriptions()
    sample,request=decoder.OnUdpPacket(self.frame(ID='02:11:22:33:44:55@4'),1,2)
    self.assertIsNone(sample)
    self.assertEqual(request['ID'],'02:11:22:33:44:55@4')

  def test_complete_info_and_retry_limits(self):
    decoder=UdpDescriptions();frame=self.frame()
    self.assertIsNotNone(decoder.OnUdpPacket(frame,1,2)[1])
    self.assertIsNone(decoder.OnUdpPacket(frame,2,3)[1])
    self.assertIsNotNone(decoder.OnUdpPacket(frame,4,5)[1])
    self.assertIsNone(decoder.OnUdpPacket(self.info(frame,'Root',Data='{'),5,6)[0])
    decoder.OnUdpPacket(self.info(frame,'Root'),6,7)
    sample,_=decoder.OnUdpPacket(self.info(frame,'Identity'),7,8)
    self.assertEqual(sample[1:],(4,5))

  def test_bit_mask_pair_updates_and_preserves_values(self):
    decoder=UdpDescriptions()
    channel='02:11:22:33:44:55@1/0'
    previous=BITS['InState']['Value']|BITS['InState']['Mask']|BITS['OvrActive']['Value']|BITS['OvrActive']['Mask']
    entry=dict(required={'Root','Identity','Channel:'+channel},
               parts={'Root':{},'Identity':{'Ver':1},'Channel:'+channel:{}},channel_bits={channel:previous})
    packet=self.frame(Status=dict(CurrentTime=100,Chs=[dict(Id=channel,Bits=BITS['OvrActive']['Mask'])],Meas=[]))
    expanded=decoder.ExpandStatus(packet,entry)
    row=expanded['Status']['Chs'][0]
    self.assertIs(row['InState'],True)
    self.assertIs(row['OvrActive'],False)

if __name__=='__main__':unittest.main()
