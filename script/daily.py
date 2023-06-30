"""A scipt to be run daily, which scans the `minly` folder and gathers CSV files
into archives for each day, leaving the newest files alone."""

import itertools
import datetime
import time
import re
import os
import sys
import tarfile
import lzma

# Parameters
inpath = os.path.join("/", "home", "pi", "sensor-logging", "data", "minly")
outpath = os.path.join("/", "home", "pi", "sensor-logging", "data", "daily")
devices = ["dht22", "sensorhub"]
min_age_days = 2 # 2 for security, should there ever be some dating issues
tar_format = tarfile.GNU_FORMAT
lzma_format = lzma.FORMAT_XZ
lzma_check = lzma.CHECK_CRC64
# Note: The maximum nice value of 273 seems to produce the best results.
# Increasing depth increases the runtime approximately linearly, but squeezes a
# few KiB more out of each archive. Since the Raspberry Pi hangs around with
# idle CPUs anyway, I don't think it's a problem to run an expensive compression
# once daily for a few minutes for this admittedly minor improvement in file
# size. I only see very minor improvements when increasing the depth from 200 to
# 300, so I guess I won't bother to go even higher.
lzma_filters = [{"id": lzma.FILTER_LZMA2, "dict_size": 16777216,
                    "lc": 4, "lp": 0, "pb": 0, "mf": lzma.MF_HC4, "mode":
                    lzma.MODE_NORMAL, "nice_len": 273, "depth": 300}]


# Calculate threshold date and gather candidate files
min_age_date = datetime.date.today() - datetime.timedelta(days=min_age_days)
files = os.listdir(inpath)

# Prepare pattern matcher
pattern = ("([0-9]{4})-([0-9]{2})-([0-9]{2})-([0-9]{2})-([0-9]{2})-([0-9]{2})-"
    + f"({'|'.join(devices)}).csv")
cp = re.compile(pattern)

# Logging output
sep = "----"
print(f"\"{sys.argv[0]}\" preparing at GMT " +
        time.strftime("%Y-%m-%d-%H-%M-%S.", time.gmtime()))
print(sep)
print("Parameters:")
print(f"  `inpath`: '{inpath}'")
print(f"  `outpath`: '{outpath}'")
print(f"  `devices`: {devices}")
print(f"  `min_age_days`: {min_age_days}")
print(f"  `tar_format`: {tar_format}")
print(f"  `lzma_format`: {lzma_format}")
print(f"  `lzma_check`: {lzma_check}")
print(f"  `lzma_filters`: {lzma_filters}")
print(sep)
print(f"`min_age_date`: {min_age_date.isoformat()}")
print(f"Found {len(files)} candidate files in inpath to potentially archive.")

# Filter files by isfile, match, old enough
to_date = lambda m: datetime.date.fromisoformat("-".join(m.group(1, 2, 3)))
matches = [f for f in files if os.path.isfile(os.path.join(inpath, f))]
matches = [cp.fullmatch(f) for f in files]
matches = [m for m in matches if not (m is None)]
matches = [m for m in matches if to_date(m) <= min_age_date]

# Partition into days and archive
parts = itertools.groupby(sorted(matches, key=to_date), to_date)
for (i, (k, g)) in enumerate(parts):
    # Note: I could probably squeeze a lot higher compression ratios out of this
    # data if I stored it in a custom binary format and then used some FLAC-like
    # compression or at least a delta filter. Since this would take quite some
    # coding effort and perhaps even create maintenance and compatibility
    # issues, I won't go overboard with that and just use LZMA on the CSVs. I'll
    # end up with some 300 MiB of compressed data per year, which I think is
    # acceptable.
    # Note: I tried using the xz delta filter, but it seems to produce worse
    # results, even if I limit the input to one device. One issue is that the
    # CSV entries don't always have the same length – maybe delta compression
    # would be better if they did.
    # Note: Sorting infiles improves compression. Sorting by timestamp first
    # seems to produce favorable results as compared to sorting by device first.
    infiles = sorted([m.group() for m in g])
    outfile = f"{k.isoformat()}.tar.xz"
    print(f"Part {i}:")
    print(f"  Processing {len(infiles)} files for date {k.isoformat()}…")

    try:
        tic = time.time()
        with lzma.open(f"{os.path.join(outpath, outfile)}", "wb",
                format=lzma_format, check=lzma_check,
                filters=lzma_filters) as xz:
            with tarfile.open(None, "w", xz, format=tar_format) as tar:
                for infile in infiles:
                    tar.add(os.path.join(inpath, infile), arcname=infile)
        toc = time.time()
    except Exception as e:
        print(f"  Creating archive failed: {e}")
    else:
        print("  Creating archive took {:.2f} seconds.".format(toc - tic))
        print("  Deleting uncompressed files…")
        try:
            tic = time.time()
            for infile in infiles:
                os.remove(os.path.join(inpath, infile))
            toc = time.time()
        except Exception as e:
            print(f"  Deleting uncompressed files failed: {e}")
        else:
            print("  Deleting took {:.2f} seconds.".format(toc - tic))

print(f"\"{sys.argv[0]}\" completing at GMT " +
        time.strftime("%Y-%m-%d-%H-%M-%S.\n", time.gmtime()))

