import time
import os
import sched

lt = time.localtime()
s = sched.scheduler()

def f():
    lt0 = time.localtime()
    name0 = f"{lt0.tm_year}-{lt0.tm_mon}-{lt0.tm_mday}-{lt0.tm_hour}-{lt0.tm_min}-{lt0.tm_sec}"
    out = open(os.path.join("/", "home", "pi", f"{name}.txt"), "a")
    out.write(f"{name0}\n")
    out.close()


name = f"{lt.tm_year}-{lt.tm_mon}-{lt.tm_mday}-{lt.tm_hour}-{lt.tm_min}-{lt.tm_sec}"
out = open(os.path.join("/", "home", "pi", f"{name}.txt"), "w")
out.write(f"{name}\n")
out.close()

s.enter(15, 1, f)
s.enter(30, 1, f)
s.enter(45, 1, f)
s.run()
