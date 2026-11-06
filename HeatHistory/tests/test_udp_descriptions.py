# test_udp_descriptions.py, Version: 1.02
import json
import pathlib
import sys
import unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]))
from UdpDescriptions import UdpDescriptions

class DescriptionTests(unittest.TestCase):
  def frame(self, **changes):
    return dict(dict(Version=2,Type='Status',ID='02:11:22:33:44:55@1',Compact=True,
                     InfoRevision='0123456789abcdef',Status=dict(CurrentTime=100)),**changes)

  def info(self, frame, field, **changes):
    return dict(dict(Version=2,Type='Info',ID=frame['ID'],InfoRevision=frame['InfoRevision'],
                     Field=field,Data='{}'),**changes)

  def test_malformed_frames_are_ignored(self):
    decoder=UdpDescriptions()
    for status in [None,dict(Channels=None),dict(Measurements={}),dict(Channels=['invalid']),
                   dict(Channels=[dict(Id=0,Bits='bad')]),dict(Channels=[dict(Id=0,Bits=-1)]),
                   dict(Channels=[dict(Id={},Bits=0)]),dict(Measurements=[dict(Id=[])])]:
      self.assertEqual(decoder.OnUdpPacket(self.frame(Status=status),0,0),(None,None))
    for packet in [b'bad json',b'"\xff"',[],self.frame(Version=True),self.frame(Version=2.0),
                   self.frame(ID=[]),self.frame(ID=''),self.frame(Type='Request'),self.frame(ID='bad')]:
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

  def test_assignment_filter_and_full_status(self):
    decoder=UdpDescriptions();frame=self.frame(Compact=False)
    self.assertEqual(decoder.OnUdpPacket(frame,1,2,[]),(None,None))
    self.assertEqual(decoder.OnUdpPacket(frame,1,2,[frame['ID']]),((frame,1,2),None))

  def test_revision_change_discards_old_sample(self):
    decoder=UdpDescriptions();old=self.frame();new=self.frame(InfoRevision='fedcba9876543210',Status=dict(CurrentTime=200))
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

if __name__=='__main__':unittest.main()
