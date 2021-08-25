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
inpath = os.path.join("/", "home", "schloer", "sensor-logging", "data",
        "minly-copy-delme")
outpath = os.path.join("/", "home", "schloer", "sensor-logging", "data", "daily")
devices = ["dht22", "sensorhub"]
min_age_days = 2
tar_format = tarfile.GNU_FORMAT
lzma_format = lzma.FORMAT_XZ
lzma_check = lzma.CHECK_CRC64
lzma_filters = [{"id": lzma.FILTER_LZMA2, "dict_size": 16777216,
                    "lc": 4, "lp": 0, "pb": 0, "mf": lzma.MF_HC4, "mode":
                    lzma.MODE_NORMAL, "nice_len": 100, "depth": 10}]


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
    infiles = sorted([m.group() for m in g])
    outfile = f"{k.isoformat()}.tar.xz"
    print(f"Part {i}:")
    print("  Processing {len(infiles)} files for date {k.isoformat()}…")

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

