"""
The output of `sensor-logging lpd433-listen` can be piped into this script. It
will show this output live. When the input ends or a SIGINT is received, the
median intercode gap, short pulse length, and long pulse length is computed and
printed for each unique code. The number of occurences is printed for every
code and the output is sorted from least to most occurences. The overall
aggregates and an aggregate of all rows without outliers (times outside the
10%-90% percentile range) is also printed. This allows sending an LPD433 code
multiple times from another device (e.g. a remote) and computing its optimal
parameters for replication via a connected LPD433 transmitter.
"""

import sys
import fileinput
import csv
import numpy as np

class Replacer(object):
  """
  File-like object transformation where substings `old` are replaced by `new`.
  """
  def __init__(self, f, old, new):
    self.f = f
    self.old = old
    self.new = new
  def write(self, s):
    self.f.write(s.replace(self.old, self.new))
  def close(self):
    self.f.close()
  def flush(self):
    self.f.flush()

def get_lpd433_code_parameters(itr, out, percentiles = [10, 90]):
  lines = []
  try:
    for line in itr:
      if len(line) > 0 and line[0] != "#":
        lines.append(line)
      print(line.rstrip())
  except KeyboardInterrupt:
    pass
  out.writelines(["\r\n", "# Per `code` and `n_bits`:\r\n"])
  reader = csv.reader(lines, skipinitialspace = True)
  names = next(reader)
  data = {name: [] for name in names}
  for row in reader:
    for name, str in zip(names, row):
      if str.find(".") == -1:
        value = int(str)
      else:
        value = float(str)
      data[name].append(value)
  for name in data:
    data[name] = np.array(data[name])
  codes, code_counts = np.unique(data["code"], return_counts = True)
  writer = csv.writer(Replacer(out, ",", ", "),
    quoting = csv.QUOTE_NONNUMERIC)
  header = ["count", "code", "n_bits", "intercode_gap", "pulse_length_short",
    "pulse_length_long"]
  writer.writerow(header)
  for _, code in sorted(zip(code_counts, codes)):
    code_mask = data["code"] == code
    n_bitss, n_bits_counts = np.unique(data["n_bits"][code_mask],
      return_counts = True)
    for n_bits_count, n_bits in sorted(zip(n_bits_counts, n_bitss)):
      n_bits_mask = np.logical_and(code_mask, data["n_bits"] == n_bits)
      intercode_gap = np.median(data["intercode_gap"][n_bits_mask])
      pulse_length_short = np.median(data["pulse_length_short"][n_bits_mask])
      pulse_length_long = np.median(data["pulse_length_long"][n_bits_mask])
      writer.writerow([n_bits_count, code, n_bits, intercode_gap,
        pulse_length_short, pulse_length_long])

  out.writelines(["\r\n", "# Overall aggregate:\r\n"])
  writer.writerow(header)
  if len(codes) > 0:
    n_bitss, n_bits_counts = np.unique(data["n_bits"], return_counts = True)
    count = len(data["code"])
    code = codes[np.argmax(code_counts)]
    n_bits = n_bitss[np.argmax(n_bits_counts)]
    intercode_gap = np.median(data["intercode_gap"])
    pulse_length_short = np.median(data["pulse_length_short"])
    pulse_length_long = np.median(data["pulse_length_long"])
    writer.writerow([count, code, n_bits, intercode_gap,
      pulse_length_short, pulse_length_long])

  out.writelines(["\r\n", "# Without outliers:\r\n"])
  writer.writerow(header)
  if len(codes) > 0:
    intercode_gap_percentiles = np.percentile(data["intercode_gap"], percentiles)
    pulse_length_short_percentiles = np.percentile(
      data["pulse_length_short"], percentiles)
    pulse_length_long_percentiles = np.percentile(
      data["pulse_length_long"], percentiles)
    mask = np.logical_and.reduce([
      data["intercode_gap"] >= intercode_gap_percentiles[0],
      data["intercode_gap"] <= intercode_gap_percentiles[1],
      data["pulse_length_short"] >= pulse_length_short_percentiles[0],
      data["pulse_length_short"] <= pulse_length_short_percentiles[1],
      data["pulse_length_long"] >= pulse_length_long_percentiles[0],
      data["pulse_length_long"] <= pulse_length_long_percentiles[1]])
    n_bitss, n_bits_counts = np.unique(data["n_bits"][mask],
      return_counts = True)
    count = len(data["code"][mask])
    code = codes[np.argmax(code_counts)]
    n_bits = n_bitss[np.argmax(n_bits_counts)]
    intercode_gap = np.median(data["intercode_gap"][mask])
    pulse_length_short = np.median(data["pulse_length_short"][mask])
    pulse_length_long = np.median(data["pulse_length_long"][mask])
    writer.writerow([count, code, n_bits, intercode_gap,
      pulse_length_short, pulse_length_long])

get_lpd433_code_parameters(fileinput.input(), sys.stdout)
