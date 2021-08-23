# This is mostly bullshit. I can't practically compare all possible dates, and
# taking a random sample gives me just a heuristic and doesn't work the way it's
# coded right now. But I'm keeping the file anyway for now.

import itertools
import random

digits = range(10)
print(f"Digits: {[{i} for i in digits]}")

okay = True

sample_size = 1000000
progress_step = 10000

for (i, (dy00, dy01, dy02, dy03, dm00, dm01, dd00, dd01, dy10, dy11, dy12, dy13,
dm10, dm11, dd10, dd11)) in enumerate(random.sample(itertools.product(digits,
    repeat=16), sample_size)):
    if (i % progress_step == 0):
        print(f"{100 * i / sample_size}%")
    str0 = f"{dy00}{dy01}{dy02}{dy03}-{dm00}{dm01}-{dd00}{dd01}"
    str1 = f"{dy10}{dy11}{dy12}{dy13}-{dm10}{dm11}-{dd10}{dd11}"
    int0 = ((dy00 * 1000 + dy01 * 100 + dy02 * 10 + dy03) * 10000 +
            (dm00 * 10 + dd01) * 100 * dd00 * 10 + dd01)
    int1 = ((dy10 * 1000 + dy11 * 100 + dy12 * 10 + dy13) * 10000 +
            (dm10 * 10 + dd11) * 100 * dd10 * 10 + dd11)
    if (str0 > str1) != (int0 > int1):
        print(f"Problem:")
        print(f"{str0} > {str1}: {str0 > str1}, {int0} > {int1}: {int0 > int1}")
        okay = False

print(f"Okay? {okay}")

