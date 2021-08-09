import os
from random import random

out = open(os.path.join("/", "home", "pi", f"data.txt"), "w")
for i in range(12 * 60 * 24):
    str = ""
    str += "{:.1f} ".format(random() * 100) # DHT22 Temperature
    str += "{:.1f} ".format(random() * 100) # DHT22 Humidity
    str += "{:.0f} ".format(random() * 2**16) # Brightness
    str += "{:.0f} ".format(random() * 100) # Temperature
    str += "{:.0f} ".format(random() * 100) # Humidity
    str += "{:.0f} ".format(random() * 2**24) # Pressure
    str += "yes" if random() < .5 else "no" # Human
    out.write(str + "\n")
out.close()
