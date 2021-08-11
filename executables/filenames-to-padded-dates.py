import os

path = os.path.join("/", "home", "pi", "sensor-logging", "data",
        "minly-copy-delme")
filenames = os.listdir(path)

def pad_dates_in_filename(filename: str):
    """This function assumes a filename format like this:
    `*y-*m-*d-*h-*m-*s-*`"""
    ss = filename.split("-")
    return "-".join([ss[0].zfill(4), ss[1].zfill(2), ss[2].zfill(2),
        ss[3].zfill(2), ss[4].zfill(2), ss[5].zfill(2)] + ss[6:])

for filename in filenames:
    if len(filename) >= 4 and filename[-4:] == ".csv":
        os.rename(os.path.join(path, filename),
                os.path.join(path, pad_dates_in_filename(filename)))

