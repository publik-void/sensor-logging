"""A scipt to be run daily, which scans the `minly` folder and gathers CSV files
into archives for each day, leaving the newest files alone."""

import itertools
import datetime
import time
import re
import os
import shutil
import sys
import tarfile
import lzma

# Parameters
inpath = os.path.join("/", "home", "pi", "sensor-logging", "data",
        "minly-copy-delme")
outpath = os.path.join("/", "home", "pi", "sensor-logging", "data", "daily")
devices = ["dht22", "sensorhub"]
min_age_days = 2


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
print(f"`inpath`: '{inpath}'")
print(f"`outpath`: '{outpath}'")
print(f"`devices`: {devices}")
print(f"`min_age_days`: {min_age_days}")
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
for (k, g) in itertools.groupby(sorted(matches, key=to_date), to_date):
    infiles = sorted([m.group() for m in g])
    outfile = f"{k.isoformat()}"
    print(f"Processing {len(infiles)} files for date {k.isoformat()}…")

    # Make a temporary directory and copy relevant files there
    # The reason for this is that tar behaves favorably when archiving a whole
    # directory as compared to a list of files

    # Note: Oh boy… It seems that a whole lot of the trial and error stuff I did
    # turned out as bullshit because my minly-copy-delme data was corrupt for
    # whatever reason. So I guess the whole thing might be easier than I
    # thought, and I may not even need to create an extra directory before
    # archiving…

    tic0 = time.time()
    os.mkdir(os.path.join(outpath, outfile))
    for infile in infiles:
        shutil.copy(os.path.join(inpath, infile), os.path.join(outpath, outfile))
    toc0 = time.time()
    print("Copying took {:.2f} seconds.".format(toc0 - tic0))

    tic1 = time.time()
    with tarfile.open(f"{os.path.join(outpath, outfile)}.tar", "w",
            format=tarfile.USTAR_FORMAT) as tar:
        tar.add(os.path.join(outpath, outfile), arcname=outfile)
    toc1 = time.time()
    print("Creating tar archive took {:.2f} seconds.".format(toc1 - tic1))


print(f"# Process \"{sys.argv[0]}\" completing at GMT " +
        time.strftime("%Y-%m-%d-%H-%M-%S.\n", time.gmtime()))

