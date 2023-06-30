"""Module providing readout functions intended to be called by minly.py or
another scheduling master program."""

import board
import adafruit_dht


dht = adafruit_dht.DHT22(board.D4, use_pulseio=False)

def readout_dht22(ut_str: str, filename: str):
    """Readout function writing the unix time string as well as the readout data
    into the file with the provided name in CSV format."""
    out = open(filename, "a")
    try:
        temperature = f"{dht.temperature}"
    except RuntimeError as error:
        temperature = ""
    try:
        humidity = f"{dht.humidity}"
    except RuntimeError as error:
        humidity = ""
    if temperature == "None":
        temperature = ""
    if humidity == "None":
        humidity = ""
    out.write(f"{ut_str}, {temperature}, {humidity}\n")
    out.close()

