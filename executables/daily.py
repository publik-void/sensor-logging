"""A scipt to be run daily, which scans the `minly` folder and gathers CSV files
into archives for each day, leaving the newest files alone."""

import itertools
import datetime
import time
import re
import os
import sys
import subprocess

# Parameters
inpath = "/home/pi/sensor-logging/data/minly-copy-delme"
outpath = "/home/pi/sensor-logging/data/daily"
devices = ["dht22", "sensorhub"]
min_age_days = 2
compression_pipe = ("xz "
    "--lzma2=dict=16MiB,lc=4,lp=0,pb=0,mf=hc4,mode=normal,nice=100,depth=10")
archive_extension = ".tar.xz"


# Calculate threshold date and gather candidate files
min_age_date = datetime.date.today() - datetime.timedelta(days=min_age_days)
files = os.listdir(inpath)

# Prepare pattern matcher
pattern = ("([0-9]{4})-([0-9]{2})-([0-9]{2})-([0-9]{2})-([0-9]{2})-([0-9]{2})-"
    + f"({'|'.join(devices)}).csv")
cp = re.compile(pattern)

# Logging output
print(f"# Process \"{sys.argv[0]}\" preparing at GMT " +
        time.strftime("%Y-%m-%d-%H-%M-%S.", time.gmtime()))
print("#")
print("# Parameters:")
print(f"# `inpath`: '{inpath}'")
print(f"# `outpath`: '{outpath}'")
print(f"# `devices`: {devices}")
print(f"# `min_age_days`: {min_age_days}")
print(f"# `compression_pipe`: '{compression_pipe}'")
print(f"# `archive_extension`: '{archive_extension}'")
print("#")
print(f"# `min_age_date`: {min_age_date.isoformat()}")
print(f"# Found {len(files)} candidate files in inpath to potentially archive.")

# Filter files by isfile, match, old enough
to_date = lambda m: datetime.date.fromisoformat("-".join(m.group(1, 2, 3)))
matches = [f for f in files if os.path.isfile(os.path.join(inpath, f))]
matches = [cp.fullmatch(f) for f in files]
matches = [m for m in matches if not (m is None)]
matches = [m for m in matches if to_date(m) <= min_age_date]

# Partition into days and archive
for (k, g) in itertools.groupby(sorted(matches, key=to_date), to_date):
    infiles = [os.path.join(inpath, m.group()) for m in g]
    outfile = f"{os.path.join(outpath, k.isoformat())}{archive_extension}"

    # # This creates an archiving command that lists all files verbatim
    # # …which creates problems because the argument list is too long.
    # infiles_log = f"<{len(infiles)} files for date {k.isoformat()}…>"
    # command, command_log = [f"tar -C {inpath} -cf {sub} | {compression_pipe}"
    #         f" > {outfile}" for sub in [" ".join(infiles), infiles_log]]

    # This creates an archiving command which uses shell globbing instead
    # …which is less accurate, but should be sufficient to get the job done.
    command = (
        f"tar -C {inpath} -cf {os.path.join(inpath, k.isoformat())}-*-*-*-*.csv"
        f" | {compression_pipe} > {outfile}")
    command_log = command

    print(command_log)

    tic = time.time()
    csp = subprocess.run(["sh", "-c", command])
    toc = time.time()
    print("# Archiving command took {:.2f} seconds ".format(toc-tic) +
            f"and returned {csp.returncode}.")

    if csp.returncode == 0:
        # Delete uncompressed files
        print("# Proceeding to delete uncompressed files.")
    #     for infile in infiles:
    #         os.remove(infile)

print(f"# Process \"{sys.argv[0]}\" completing at GMT " +
        time.strftime("%Y-%m-%d-%H-%M-%S.\n", time.gmtime()))

