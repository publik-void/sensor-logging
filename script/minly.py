"""A script to be run each minute, which waits for the next minute to start and
then creates jobs at regular time intervals that call the sensor readout
functions/executables and write their results into CSV files that were provided
by this script beforehand."""

import os
import time
import sched

import dht22
import sensorhub


# Readout functions to run in regular intervals. Each function handles one
# device. It reads the device's sensors and writes the results, together with
# the provided unix time string, into the file with the provided name, in CSV
# format. These functions may just call another similar python function, but
# they are meant as general interface for calling anything, e.g. a C program.
def readout_dht22(ut_str: str, filename: str):
    """The DHT22 sensor."""
    # print(f"`readout_dht22` called with "
    #       f"`ut_str` = {ut_str}, "
    #       f"`filename` = {filename}")
    dht22.readout_dht22(ut_str, filename)

def readout_sensorhub(ut_str: str, filename: str):
    """ The GeeekPi Docker Pi Sensor Hub Development Board."""
    # print(f"`readout_sensorhub` called with "
    #       f"`ut_str` = {ut_str}, "
    #       f"`filename` = {filename}")
    sensorhub.readout_sensorhub(ut_str, filename)


# Parameters for the script
path = "/home/pi/sensor-logging/data/minly"

readout_functions = [readout_dht22, readout_sensorhub]
readout_column_titles = [["temperature", "humidity"],
    ["ntc_temperature", "ntc_overrange", "ntc_missing", "dht11_temperature",
        "dht11_humidity", "dht11_error", "bmp280_temperature",
        "bmp280_pressure", "bmp280_error", "brightness", "brightness_overrange",
        "brightness_error", "motion"]]
readout_names = ["dht22", "sensorhub"]

interval = 1 # Interval in seconds (integer)


readout_column_titles = [[f"{name}_{column_title}"
    for column_title in column_titles]
    for (column_titles, name) in zip(readout_column_titles, readout_names)]

headers = [f"unix_time, {', '.join(column_titles)}\n" if column_titles else None
           for column_titles in readout_column_titles]

def run():
    """The program which is run at the start of the next minute"""
    # I work with GMT to avoid issues with daylight savings time
    gmt = time.gmtime()
    # gmt_str = (f"{gmt.tm_year}-{gmt.tm_mon}-{gmt.tm_mday}-"
    #            f"{gmt.tm_hour}-{gmt.tm_min}-{gmt.tm_sec}")
    gmt_str = "-".join([
        f"{gmt.tm_year}".zfill(4),
        f"{gmt.tm_mon}".zfill(2),
        f"{gmt.tm_mday}".zfill(2),
        f"{gmt.tm_hour}".zfill(2),
        f"{gmt.tm_min}".zfill(2),
        f"{gmt.tm_sec}".zfill(2)])

    filenames = [os.path.join(path, f"{gmt_str}-{name}.csv")
                 for name in readout_names]

    # Create files for the readout functions to write into
    for (filename, header) in zip(filenames, headers):
        out = open(filename, "w")
        if not header is None:
            out.write(header)
        out.close()

    def job():
        """The master job which is supposed to be run at regular intervals and
        calls the readout functions."""
        ut_str = f"{time.time()}"
        for (f, filename) in zip(readout_functions, filenames):
            f(ut_str, filename)

    # Initilaize scheduler
    s = sched.scheduler()
    for t in range(0, 60, interval):
        s.enter(t, 1, job)

    s.run()

    # It seems like the scheduler slowly drifts a tiny bit in its timing, but
    # this shouldn't be an issue given that this script is called every minute.

# Schedule the actual whole process to run at the start of the next minute
s = sched.scheduler(timefunc=time.time)
current_minute, _ = divmod(time.time(), 60)
s.enterabs((current_minute + 1) * 60, 1, run)
s.run()

