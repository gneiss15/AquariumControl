#!/usr/bin/env python3
# test_sender.py, Version: 1.05
"""Persistent controls and actual UDP payloads with simulated Linux sensor files."""
import concurrent.futures
import importlib
import json
import os
from pathlib import Path
import socket
import subprocess
import sys
import tempfile
import unittest
from unittest.mock import patch

Root = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Root))
import ChargeLearning as Learning
import BatteryUdpSender as Sender


class StopSender(Exception):
  pass


class LearningTests(unittest.TestCase):
  def setUp(self):
    self.Temporary = tempfile.TemporaryDirectory()
    self.addCleanup(self.Temporary.cleanup)
    self.State = Path(self.Temporary.name)/'state/ChargeLearning.json'
    self.BootId = '11111111-1111-4111-8111-111111111111'
    self.BootPatch = patch.object(Learning, 'ReadBootId', side_effect=lambda: self.BootId)
    self.BootPatch.start(); self.addCleanup(self.BootPatch.stop)

  def test_persisted_mode_and_boot(self):
    First = Learning.LoadState(self.State)
    self.assertTrue(First['ChargeLearnAllowed'])
    self.assertEqual(First['ChargeBootCnt'], 1)
    self.assertEqual(First, Learning.SetLearning(True, self.State))
    Off = Learning.SetLearning(False, self.State)
    self.assertFalse(Off['ChargeLearnAllowed'])
    self.assertEqual(Off['ChargeBootCnt'], First['ChargeBootCnt'])
    self.assertEqual(Off, Learning.SetLearning(False, self.State))
    self.assertEqual(Off, Learning.LoadState(self.State))
    self.assertEqual(self.State.stat().st_mode & 0o777, 0o600)
    self.BootId = '22222222-2222-4222-8222-222222222222'
    Next = Learning.LoadState(self.State)
    self.assertEqual(Next['ChargeBootCnt'], 2)
    self.assertFalse(Next['ChargeLearnAllowed'])
    self.assertEqual(Next, Learning.LoadState(self.State))
    On = Learning.SetLearning(True,self.State)
    self.assertEqual(On['ChargeBootCnt'],2)
    self.assertTrue(On['ChargeLearnAllowed'])

  def test_no_write_for_idempotent_commands(self):
    First = Learning.LoadState(self.State)
    with patch.object(Learning,'WriteState',side_effect=AssertionError('Unneeded write')):
      self.assertEqual(First, Learning.LoadState(self.State))
      self.assertEqual(First, Learning.SetLearning(True,self.State))

  def test_concurrent_toggles(self):
    self.BootId = Path("/proc/sys/kernel/random/boot_id").read_text().strip()
    First = Learning.LoadState(self.State)
    with concurrent.futures.ThreadPoolExecutor(max_workers=6) as Pool:
      list(Pool.map(lambda _:subprocess.run([sys.executable,str(Root/'ChargeLearning.py'),'toggle','--state-file',str(self.State)],check=True,capture_output=True),range(6)))
    After = Learning.LoadState(self.State)
    self.assertEqual(After['ChargeLearnAllowed'],First['ChargeLearnAllowed'])
    self.assertEqual(After['ChargeBootCnt'],First['ChargeBootCnt']) # All processes are in the same real boot.


  def test_invalid_state_not_replaced(self):
    Learning.LoadState(self.State)
    for Value in ['{', 'null', '{"ChargeLearnAllowed":1,"ChargeBootCnt":1}', '{"ChargeLearnAllowed":true,"ChargeBootCnt":true}', '{"ChargeLearnAllowed":true,"ChargeBootCnt":0}', '{"ChargeLearnAllowed":true,"ChargeBootCnt":4294967296}']:
      self.State.write_text(Value)
      with self.assertRaises(ValueError):Learning.LoadState(self.State)
      with self.assertRaises(ValueError):Learning.SetLearning(True,self.State)
      self.assertEqual(self.State.read_text(),Value)

  def test_uint32_wrap(self):
    Learning.LoadState(self.State)
    self.State.write_text(json.dumps({'ChargeLearnAllowed':True,'ChargeBootCnt':Learning.MaximumCounter,'LastBootId':self.BootId}))
    self.BootId = '33333333-3333-4333-8333-333333333333'
    self.assertEqual(Learning.LoadState(self.State)['ChargeBootCnt'],0)
    self.assertEqual(Learning.LoadState(self.State)['ChargeBootCnt'],0)

  def test_real_udp_and_corrupt_state(self):
    Directory=Path(self.Temporary.name)
    Inputs={'BatteryCapacityFile':'63','AcOnlineFile':'1','BatteryStateFile':'Charging','LaptopMacFile':'02:11:22:33:44:55'}
    for Key,Value in Inputs.items():(Directory/Key).write_text(Value)
    with socket.socket(socket.AF_INET,socket.SOCK_DGRAM) as Receiver:
      Receiver.bind(('127.0.0.1',0));Receiver.settimeout(3)
      def Packet():
        with patch.multiple(Sender,**{Key:str(Directory/Key) for Key in Inputs},BroadcastAddress='127.0.0.1',UdpPort=Receiver.getsockname()[1]), patch.object(Learning,'StatePath',return_value=self.State), patch.object(Sender.time,'sleep',side_effect=StopSender) as Sleep:
          with self.assertRaises(StopSender):Sender.Main()
          Sleep.assert_called_once_with(10)
        return json.loads(Receiver.recvfrom(4096)[0])
      First=Packet();self.assertEqual(First['Ver'],3);self.assertEqual(First['Type'],2);self.assertEqual(First['ID'],'02:11:22:33:44:55@3')
      self.assertEqual(First['Status']['BatteryPct'],63);self.assertTrue(First['Status']['AcOnline'])
      self.assertEqual(First['Status']['BatteryState'],'Charging');self.assertTrue(First['Status']['ChargeLearnAllowed'])
      Off=Learning.SetLearning(False,self.State);Second=Packet();self.assertFalse(Second['Status']['ChargeLearnAllowed'])
      self.assertEqual(Second['Status']['BootCnt'],Off['ChargeBootCnt'])
      Learning.SetLearning(True,self.State);Third=Packet();self.assertTrue(Third['Status']['ChargeLearnAllowed'])
      self.assertEqual(First['Status']['BootCnt'],Third['Status']['BootCnt'])
      self.State.write_text('{')
      Invalid=Packet();self.assertFalse(Invalid['Status']['ChargeLearnAllowed']);self.assertIsNone(Invalid['Status']['BootCnt'])
      self.assertEqual(Invalid['Status']['BatteryPct'],63);self.assertEqual(self.State.read_text(),'{')


if __name__=='__main__':unittest.main()
