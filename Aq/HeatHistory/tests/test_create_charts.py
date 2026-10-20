#!/usr/bin/env python3
# test_create_charts.py, Version: 1.04

import csv
import sys
import tempfile
import unittest
import threading
import urllib.request
import urllib.error
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from CreateCharts import ChartDataError, chart_data, create_report, read_csv, chart_server


class HeatHistoryChartTests(unittest.TestCase):
  def setUp(self) -> None:
    self.temp_dir = tempfile.TemporaryDirectory()
    self.root = Path(self.temp_dir.name)
    self.profile_dir = self.root / "HeatProfileAq1"
    self.profile_dir.mkdir()
    self.csv_file = self.profile_dir / "test.csv"
    with self.csv_file.open("w", encoding="utf-8", newline="") as output:
      writer = csv.writer(output, delimiter=";")
      writer.writerow(("Time", "Temp", "Floor", "Main"))
      writer.writerow(("2026-10-10T19:50:00.000Z", "27.5", 0, 0))
      writer.writerow(("2026-10-10T19:55:00.250Z", "27.2", 1, 0))
      writer.writerow(("2026-10-10T20:00:00.500Z", "27.0", 1, 1))

  def tearDown(self) -> None:
    self.temp_dir.cleanup()

  def test_reads_utc_timestamps_and_numeric_values(self) -> None:
    samples = read_csv(self.csv_file)
    self.assertEqual(len(samples), 3)
    self.assertEqual(samples[0].time.utcoffset().total_seconds(), 0)
    self.assertEqual(samples[-1].temp, 27.0)
    self.assertEqual(samples[-1].main, 1)

  def test_generates_one_interactive_chart_with_three_aligned_axes(self) -> None:
    output = self.root / "Charts.html"
    count = create_report(self.root, output)
    document = output.read_text(encoding="utf-8")
    self.assertEqual(count, 1)
    self.assertIn("plotly-4.1.1.min.js", document)
    self.assertNotIn("rangeslider", document)
    self.assertNotIn("linkTimeRanges", document)
    self.assertIn('id="series-0"', document)
    self.assertIn("yaxis2", document)
    self.assertIn("yaxis3", document)
    self.assertIn("domain: [0.39, 1]", document)
    self.assertIn("domain: [0.18, 0.30]", document)
    self.assertIn("domain: [0, 0.12]", document)
    self.assertIn("shape: 'hv'", document)
    self.assertIn("Zeit (UTC)", document)
    self.assertIn("2026-10-10 19:50:00 UTC", document)
    self.assertEqual(document.count("Plotly.newPlot(series, ["), 1)
    self.assertNotIn("<svg", document)

  def test_server_renders_fresh_data_and_rejects_file_access(self) -> None:
    with chart_server(self.root, "127.0.0.1", 0) as server:
      worker = threading.Thread(target=server.serve_forever, daemon=True)
      worker.start()
      url = f"http://127.0.0.1:{server.server_port}"
      try:
        with urllib.request.urlopen(url) as response:
          first = response.read().decode()
          self.assertEqual(response.headers["Cache-Control"], "no-store")
        self.assertIn("3 Messpunkte", first)
        with self.csv_file.open("a", encoding="utf-8") as output:
          output.write("2026-10-10T20:05:00.000Z;28.3;0;0\n")
        with urllib.request.urlopen(url + "/HeatHistoryCharts.html") as response:
          self.assertIn("4 Messpunkte", response.read().decode())
        self.assertFalse((self.root / "HeatHistoryCharts.html").exists())
        with urllib.request.urlopen(urllib.request.Request(url, method="HEAD")) as response:
          self.assertEqual(response.read(), b"")
        for path in ("/config.json", "/../config.json"):
          with self.assertRaises(urllib.error.HTTPError) as caught:
            urllib.request.urlopen(url + path)
          self.assertEqual(caught.exception.code, 404)
        self.csv_file.write_text("invalid\n", encoding="utf-8")
        with self.assertRaises(urllib.error.HTTPError) as caught:
          urllib.request.urlopen(url)
        self.assertEqual(caught.exception.code, 503)
      finally:
        server.shutdown()
        worker.join()

  def test_chart_payload_escapes_script_closing_characters(self) -> None:
    samples = read_csv(self.csv_file)
    payload = chart_data(samples, "</script><img src=x>")
    self.assertNotIn("</script>", payload)
    self.assertIn("\\u003c/script\\u003e", payload)

  def test_rejects_invalid_csv_header(self) -> None:
    self.csv_file.write_text("Timestamp;Temp;Floor;Main\n", encoding="utf-8")
    with self.assertRaises(ChartDataError):
      read_csv(self.csv_file)


if __name__ == "__main__":
  unittest.main()
