"""A scipt to be run daily, which scans the `minly` folder and gathers CSV files
into archives for each day, leaving the newest files alone."""

import itertools
import datetime
import re
import os
import subprocess

path = "."
devices = ["dht22", "sensorhub"]
min_age_days = 2

min_age_date = datetime.date.today() - datetime.timedelta(days=min_age_days)
# files = os.listdir(path)
files = ["2021-08-25-13-45-00-dht22.csv",
         "2021-08-21-13-45-00-dht22.csv",
         "2021-08-21-13-45-00-sensorhub.csv",
         "2021-08-21-13-46-00-dht22.csv",
         "2021-08-18-12-00-00-sensorhub.csv",
         "bull"]

pattern = ("([0-9]{4})-([0-9]{2})-([0-9]{2})-([0-9]{2})-([0-9]{2})-([0-9]{2})-"
    + f"({'|'.join(devices)}).csv")
cp = re.compile(pattern)

# filter list files by isfile, match, old enough
to_date = lambda m: datetime.date.fromisoformat("-".join(m.group(1, 2, 3)))
matches = [f for f in files if os.path.isfile(os.path.join(path, f))]
matches = [cp.fullmatch(f) for f in files]
matches = [m for m in matches if not (m is None)]
matches = [m for m in matches if to_date(m) <= min_age_date]

# then partition into days and archive
for (k, g) in itertools.groupby(sorted(matches, key=to_date), to_date):
    command = (f"tar -C {path} -c "
            f"-f {' '.join([os.path.join(path, m.group()) for m in g])} | "
            f"xz --lzma2=dict=16MiB,lc=4,lp=0,"
            f"pb=0,mf=hc4,mode=normal,nice=16,depth=0 > "
            f"{k.isoformat()}.tar.xz")
    print(command)
    # subprocess.run(["sh", "-c", command])

