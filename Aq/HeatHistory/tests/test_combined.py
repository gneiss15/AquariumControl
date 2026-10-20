#!/usr/bin/env python3
# test_combined.py, Version: 1.00

import sys
import tempfile
import unittest
import urllib.request
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import HeatHistory
from CreateCharts import chart_server


class CombinedTests(unittest.TestCase):
  def test_browser_responds_while_logger_runs_and_server_closes(self):
    with tempfile.TemporaryDirectory() as directory:
      root = Path(directory)
      (root / 'test.csv').write_text('Time;Temp;Floor;Main\n2026-10-13T10:00:00Z;27.4;0;1\n')
      server = chart_server(root, '127.0.0.1', 0)
      class Monitor:
        data_dir = root
        udp_socket = None
        poll_interval = 10
        def poll_once(self):
          with urllib.request.urlopen(f'http://127.0.0.1:{server.server_port}/', timeout=2) as response:
            assert b'27.4' in response.read()
          raise KeyboardInterrupt
      with patch.object(HeatHistory, 'HeatHistoryMonitor', return_value=Monitor()), \
           patch.object(HeatHistory, 'chart_server', return_value=server), \
           patch.object(sys, 'argv', ['HeatHistory.py']):
        self.assertEqual(HeatHistory.main(), 0)
      self.assertEqual(server.socket.fileno(), -1)

  def test_once_never_starts_http(self):
    with patch.object(HeatHistory, 'HeatHistoryMonitor') as monitor, \
         patch.object(HeatHistory, 'chart_server') as server, \
         patch.object(sys, 'argv', ['HeatHistory.py', '--once']):
      self.assertEqual(HeatHistory.main(), 0)
      monitor.return_value.poll_once.assert_called_once()
      server.assert_not_called()


if __name__ == '__main__':
  unittest.main()
