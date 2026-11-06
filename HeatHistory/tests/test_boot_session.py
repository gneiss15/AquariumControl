# test_boot_session.py, Version: 1.00
import json
import pathlib
import sys
import tempfile
import unittest
sys.path.insert(0,str(pathlib.Path(__file__).resolve().parents[1]))
from BootSession import latest_segment,last_row

class BootSessionTests(unittest.TestCase):
  def test_partial_tail_is_ignored_and_not_reopened(self):
    with tempfile.TemporaryDirectory() as directory:
      root=pathlib.Path(directory);path=root/'20261102.energy.csv'
      path.write_bytes(b'Time;Value\n1;42\n2;4')
      path.with_suffix('.json').write_text(json.dumps({'HostBootId':'boot'}))
      original=path.read_bytes()
      self.assertEqual(last_row(path,('Time','Value')),{'Time':'1','Value':'42'})
      self.assertIsNone(latest_segment(root,'.energy.csv',{'HostBootId':'boot'}))
      self.assertEqual(path.read_bytes(),original)

  def test_complete_segment_can_resume_only_matching_configuration(self):
    with tempfile.TemporaryDirectory() as directory:
      root=pathlib.Path(directory);path=root/'20261102.energy.csv'
      path.write_text('Time;Value\n1;42\n')
      path.with_suffix('.json').write_text(json.dumps({'HostBootId':'boot'}))
      self.assertEqual(latest_segment(root,'.energy.csv',{'HostBootId':'boot'}),path)
      self.assertIsNone(latest_segment(root,'.energy.csv',{'HostBootId':'new'}))

  def test_complete_broken_row_remains_an_error(self):
    with tempfile.TemporaryDirectory() as directory:
      path=pathlib.Path(directory)/'test.csv'
      path.write_text('Time;Value\n1\n')
      with self.assertRaises(ValueError):last_row(path,('Time','Value'))

if __name__=='__main__':unittest.main()
