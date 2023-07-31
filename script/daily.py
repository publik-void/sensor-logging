"""The daily archiver for the `sensor-logging` project.

See usage message for further documentation.
"""

import os
# Make this script have low scheduling importance.
# NOTE: The static scheduling priority for the non-realtime scheduling policies
# such as `SCHED_OTHER`, `SCHED_BATCH`, and `SCHED_IDLE` is always 0, it seems.
# Nevertheless I let `os` find the minimum possible priority. More info on
# these things in the manpage sched(7).
os.nice(40)
policy = (os.SCHED_IDLE if hasattr(os, "SCHED_IDLE") else
  os.SCHED_BATCH if hasattr(os, "SCHED_BATCH") else os.SCHED_OTHER)
os.sched_setscheduler(0, policy, os.sched_param(
  os.sched_get_priority_min(os.sched_getscheduler(policy))))

import argparse
import sys
import subprocess
import socket
import datetime
import time
import itertools
import numbers
import re
import textwrap
import tarfile
import lzma

localhostname = socket.gethostname()

# Parse arguments
argparser = argparse.ArgumentParser(
  description = "Scans the `data/shortly` directory and gathers data files "
    "into archives for each day in the `data/daily` directory, while leaving "
    "the newest files alone. Writes a log into the `logs/daily` directory in "
    "TOML format, to be accessible for both humans and machines.") 
# TODO: Add epilog argument to the parser where it says that this has been
# tested with PyPy version X and Python version Y
argparser.add_argument("--log-file",
  metavar = "<path>",
  help = "where to write the TOML-formatted log (appends to file if it exists,"
    " creates a new file in the `log` directory if omitted, writes to stdout "
    "if `-` is given)")
argparser.add_argument("--min-age",
  type = int,
  default = 2,
  metavar = "<number_of_days>",
  help = "data files with dates newer than this many days ago will not be "
    "archived (default: 2 (for security, should there ever be some dating "
    "issues))")
argparser.add_argument("--file-extension",
  default = "csv",
  metavar = "<extension>",
  help = "the file extension (case-sensitive, without dot) of the data files "
  "to archive (default: csv)")
argparser.add_argument("--hostname",
  default = localhostname,
  metavar = "<name>",
  help = "archive data files identified with this hostname (default is the "
    "name of the local host)")
argparser.add_argument("--keep",
  action = "store_true",
  help = "do not delete the uncompressed data files")
argparser.add_argument("--dry",
  action = "store_true",
  help = "perform a dry run (implies `--keep` and does not create an archive)")
argparser.add_argument("--verbose",
  action = "store_true",
  help = "write the individual names of all archived files (in order) into "
    "the log")
argparser.add_argument("base_path",
  help = "the `sensor-logging` root directory (must be passed explicitly in "
    "order to avoid errors due to some bad default)")
argparser.add_argument("name",
  nargs = "*",
  help = "the name of the physical instance of a sensor as given in the data "
    "file name and the time series names")

args = argparser.parse_args()
lzma.__spec__
# Configuration parameters
config = {
  "dry_run": args.dry,
  "keep_files": args.keep or args.dry,
  "verbose": args.verbose,
  "localhostname": localhostname,
  "hostname": args.hostname,
  "base_path": os.path.abspath(args.base_path),
  "log_file": "-" if args.log_file == "-" else
    None if args.log_file is None else
    os.path.abspath(args.log_file),
  "sensors_physical_instance_names": args.name,
  "file_extension": args.file_extension,
  "min_age_days": args.min_age,

  "tar_args": {"format": tarfile.GNU_FORMAT},
  "lzma_args": {
    "format": lzma.FORMAT_XZ,
    "check": lzma.CHECK_CRC64,
    "filters": [{
      "id": lzma.FILTER_LZMA2,
      "dict_size": 16777216,
      "lc": 4,
      "lp": 0,
      "pb": 0,
      "mf": lzma.MF_HC4,
      "mode": lzma.MODE_NORMAL,
      "nice_len": 273,
      "depth": 200}]}}

# NOTE: About the compression:
# When I first coded this back in 2021, I tried a bunch of compression settings
# on the Raspberry Pi 4B with 4GB memory with some CSV files that contained
# DHT22 and Sensorhub readings sampled at 1Hz. I think I have not archived the
# testing scripts anywhere, maybe I should have…
#
# So I don't know anymore if the choice of using LZMA was based on test results
# or not, but my guess is that it's a good choice either way.
#
# This paragraph contains the notes about the compression I had written back
# then:
# The maximum nice value of 273 seems to produce the best results.
# Increasing depth increases the runtime approximately linearly, but squeezes a
# few KiB more out of each archive. Since the Raspberry Pi hangs around with
# idle CPUs anyway, I don't think it's a problem to run an expensive
# compression once daily for a few minutes for this admittedly minor
# improvement in file size. I only see very minor improvements when increasing
# the depth from 200 to 300, so I guess I won't bother to go even higher.
# I tried using the xz delta filter, but it seems to produce worse results,
# even if I limit the input to one device. One issue is that the CSV entries
# don't always have the same length – maybe delta compression would be better
# if they did.
# Sorting input files improves compression. Sorting by timestamp first seems to
# produce favorable results as compared to sorting by device first.
#
# After almost two years of archiving 2880 files daily with the above settings,
# the logs indicate that the archiving times have been varying over the months,
# but are typically somewhere in the range of 120-260s, it seems.
# TODO: See how long it takes on the Raspberry Pi Zero.
#
# As of 2023-07, I have made the CSV entries always have the same length.
# TODO: Re-check delta filter performance.
#
# It is of course conceivable that a potentially long-running task like this
# introduces elevated temperature readings for sensors in the proximity of the
# board. But I think if I worry about that, I would have to manage all kinds of
# stuff that may or may not be running at times on the Pis. It may rather be a
# question of physically separating the sensors from the board.
#
# As a final comment, there is obviously a lot of theoretical room for
# improvement in compressed file size. Converting the CSVs to a binary format
# and compressing e.g. à la FLAC would surely put the disk usage into a whole
# other league. But the main point here is not necessarily to minimize storage
# space requirements, but rather to archive the data in a simple, robust and
# well-supported way, thus CSV, TAR, XZ. As such, I also think my old comments
# on using a depth of 300 instead of 200, trading a 1.5 factor in runtime for a
# .99 or so factor of file size, are a bit ill-advised. But in the end, it's
# not like any of this matters all that much anyway.

# Indentation helpers
shiftwidth = 2
def indent_str(shifts):
  return " " * (shifts * shiftwidth)
def indent(str, shifts):
  return textwrap.indent(str, indent_str(shifts))

# (Very) basic TOML serialization helper
def as_toml(x, shifts = 0, contd = False):
  if isinstance(x, dict):
    # NOTE: Unsafe, because it never quotes keys
    str = "".join([f"{k} = {as_toml(v, shifts, True)}\n"
      for k, v in x.items()])
  elif isinstance(x, list):
    str = "[\n" if bool(x) else "["
    str += ",\n".join([as_toml(v, 1) for v in x])
    str += "]"
  elif isinstance(x, time.struct_time):
    # NOTE: Assumes UTC time
    str = time.strftime("%Y-%m-%d %H:%M:%SZ", x)
  elif isinstance(x, datetime.date):
    str = x.isoformat()
  elif isinstance(x, numbers.Number):
    str = f"{x}"
  else:
    str = repr(x).replace('"', '\\"').replace("'", "\"")
  return str if contd else indent(str, shifts)

# Convenience constants
error_prefix = f"{sys.argv[0]}: ERROR:"
info_prefix = f"{sys.argv[0]}: INFO:"
relpaths = [
  os.path.join("data", "shortly"),
  os.path.join("data", "daily"),
  os.path.join("logs", "daily")]

# "Body" of the script
# NOTE: Putting this into a function mainly to easily abstract over file
# descriptors with optional usage of `with` … `as` using a recursive call. Also
# to separate configuration from execution.
def daily(p, f = None, time_point_startup = time.gmtime(), pathss = None,
    log_file_exists = False):
  time_identifier = time.strftime("%Y-%m-%d-%H-%M-%S", time_point_startup)

  # Construct path strings and check/create directories
  if pathss is None:
    paths = [os.path.join(p["base_path"], relpath) for relpath in relpaths]
    host_paths = [os.path.join(path, p["hostname"]) for path in paths]
    pathss = [paths, host_paths]

    for relpath, path, host_path in zip(relpaths, paths, host_paths):
      if not os.path.isdir(path):
        sys.exit(f"{error_prefix} `{relpath}` is not a directory under "
          f"`{p['base_path']}`.")
      if not os.path.isdir(host_path):
        sys.stderr.write(f"{info_prefix} creating directory `{host_path}`.\n")
        os.mkdir(host_path)
  else:
    paths, host_paths = pathss

  [data_shortly_path, data_daily_path, log_daily_path] = paths
  [data_shortly_host_path, data_daily_host_path, log_daily_host_path
    ] = host_paths

  # Put a file descriptor into `f` argument
  if f is None:
    if p["log_file"] == "-":
      f = sys.stdout
    else:
      if p["log_file"] is None:
        log_file_path = os.path.join(log_daily_host_path,
          time_identifier + ".toml")
      else:
        log_file_path = p["log_file"]
      log_file_exists = os.path.isfile(log_file_path)
      with open(log_file_path, "a", encoding = "utf-8") as f:
        return daily(p, f, time_point_startup, pathss, log_file_exists)

  version_strings = {
    "python": sys.version,
    "tarfile": tarfile.version}
    # Not sure how to get the `lzma` version or if there even is one seperate
    # from the Python version

  try:
    version_strings["git_commit"] = subprocess.check_output(
      ["git", "rev-parse", "HEAD"], cwd = p["base_path"], text = True).rstrip()
  except subprocess.CalledProcessError as e:
    version_strings["git_commit_retrieval_error"] = e

  # Log startup and config
  log_identifier = f"{p['localhostname']}.{time_identifier}"
  f.write(("\n" if log_file_exists else "") +
    f"[{log_identifier}] # New log started here\n" +
    as_toml({"time_point_startup": time_point_startup}) +
    "\n" +
    indent(f"[{log_identifier}.version_strings]\n", 1) +
    as_toml(version_strings, 1) +
    "\n" +
    indent(f"[{log_identifier}.config]\n", 1) +
    as_toml({k: v for k, v in p.items()
      if not k in ["tar_args", "lzma_args"]}, 1) +
    "\n" +
    indent(f"[{log_identifier}.config.tar_args]\n", 2) +
    as_toml(p["tar_args"], 2) +
    "\n" +
    indent(f"[{log_identifier}.config.lzma_args]\n", 2) +
    as_toml({k: v for k, v in p["lzma_args"].items() if k != "filters"}, 2) +
    "".join(["\n" +
      indent(f"[[{log_identifier}.config.lzma_args.filters]]\n", 3) +
      as_toml(filter, 3) for filter in p["lzma_args"]["filters"]]))

  # Prepare pattern matcher
  pattern = ("([0-9]{4})-([0-9]{2})-([0-9]{2})-([0-9]{2})-([0-9]{2})-([0-9]{2}"
    f")-({'|'.join(p['sensors_physical_instance_names'])})."
    f"{p['file_extension']}")
  cp = re.compile(pattern)

  # Calculate threshold date
  date_startup = datetime.date.fromtimestamp(time.mktime(time_point_startup))
  min_age_date = date_startup - datetime.timedelta(days = p["min_age_days"])

  # Gather candidate files and filter by isfile, match, old enough
  # NOTE: I am tempted to sort the file list first and then use a bisection
  # algorithm to discard all the dates that are too new. However, as I will
  # usually be discarding 1-2 days worth of files and only keeping 1 day, that
  # would probably result in a lot more comparisons of dates than otherwise. By
  # the way, `os.listdir`/`os.scandir` in this particular case does not output
  # a sorted file list.
  get_date = lambda m: datetime.date.fromisoformat("-".join(m.group(1, 2, 3)))
  targets = sorted((date, item.name, item) for (date, item) in
    ((get_date(match), item) for (item, match) in
      ((item, cp.fullmatch(item.name))
        for item in os.scandir(data_shortly_host_path)
        if item.is_file())
      if not (match is None))
      if date <= min_age_date)

  # Log files found
  f.write("\n" +
    indent(f"[{log_identifier}.files]\n", 1) +
    as_toml({"min_age_date": min_age_date,
      "number_matches": len(targets)}, 1))

  # Group by date and archive
  for (date, group) in itertools.groupby(targets, lambda target: target[0]):
    group = list(group)
    basenames = [basename for (_, basename, _) in group]
    filepaths = [os.path.join(data_shortly_host_path, basename)
      for basename in basenames]
    total_size_in_bytes = sum(item.stat().st_size for (_, _, item) in group)
    # Log archiving of date group
    f.write("\n" +
      indent(f"[[{log_identifier}.files.groups]]\n", 2) +
      as_toml({"date": date,
        "number_matches": len(basenames),
        "total_size_in_bytes": total_size_in_bytes}, 2))
    if p["verbose"]:
      f.write(as_toml({"basenames": basenames}, 2))

    archive_basename = date.isoformat() + ".tar.xz"

    archive_basename_replacement = archive_basename
    while os.path.exists(os.path.join(data_daily_host_path,
        archive_basename_replacement)):
      archive_basename_replacement = (f"{archive_basename_replacement}-"
        f"{p['localhostname']}-{time_identifier}.tar.xz")

    if archive_basename_replacement != archive_basename:
      f.write("\n" +
        indent(f"# NOTE: Existence of this entry implies `{archive_basename}` "
          "already existed.\n", 2) +
        as_toml({"archive_basename": archive_basename_replacement}, 2))
      archive_basename = archive_basename_replacement

    archive_filepath = os.path.join(data_daily_host_path, archive_basename)

    if not p["dry_run"]:
      try:
        tic = time.time()
        with lzma.open(archive_filepath, "wb", **p["lzma_args"]) as xz:
          with tarfile.open(None, "w", xz, **p["tar_args"]) as tar:
            for (filepath, basename) in zip(filepaths, basenames):
              tar.add(filepath, arcname = basename)
        toc = time.time()
      except Exception as e:
        f.write("\n" +
          indent(f"# NOTE: Existence of this entry implies an error ocurred "
            "during writing of the archive.\n", 2) +
          as_toml({"error_archive_writing": e}, 2))
      else:
        f.write("\n" +
          as_toml({"duration_archive_writing_in_seconds": toc - tic}, 2))
        try:
          tic = time.time()
          with lzma.open(archive_filepath, "rb") as xz:
            with tarfile.open(None, "r", xz) as tar:
              # NOTE: `tar.getnames` returns a list of the paths of the
              # archived files (in the order they are stored in the archive)
              # relative to the archive root. I don't know if an entry
              # prefixed with a `./` can happen. Probably not, but let me
              # still call normpath to be sure.
              archived_filepaths = tar.getnames()
              archive_okay = all(basename == os.path.normpath(filepath)
                for (basename, filepath)
                in zip(basenames, archived_filepaths))
          archive_size_in_bytes = os.stat(archive_filepath).st_size
          toc = time.time()
        except Exception as e:
          f.write("\n" +
            indent(f"# NOTE: Existence of this entry implies an error ocurred "
              "during checking of the archive.\n", 2) +
            as_toml({"error_archive_checking": e}, 2))
        else:
          f.write(
            as_toml({"duration_archive_checking_in_seconds": toc - tic,
              "archive_okay": archive_okay,
              "archive_size_in_bytes": archive_size_in_bytes}, 2))
          if not archive_okay:
            if p["verbose"]:
              f.write(
                as_toml({"archived_filepaths": archived_filepaths}, 2))
          else:
            if not p["keep_files"]:
              try:
                tic = time.time()
                for filepath in filepaths:
                  # TODO
                  # print(f"os.remove(\"{filepath}\")")
                  # os.remove(filepath)
                  pass
                toc = time.time()
              except Exception as e:
                f.write("\n" +
                  indent(f"# NOTE: Existence of this entry implies an error "
                    "ocurred during deletion of the uncompressed data files."
                    "\n", 2) +
                  as_toml({"error_file_deletion": e}, 2))
              else:
                f.write(
                  as_toml({"duration_file_deletion": toc - tic}, 2))

  time_point_finish = time.gmtime()
  f.write("\n"
    f"[{log_identifier}] # Log finished with this section\n" +
    as_toml({"time_point_finish": time_point_finish}))

daily(config)
