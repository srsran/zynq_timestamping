import sys

import numpy as np
from matplotlib import pyplot as plt

FILENAME = sys.argv[1]

samples = np.fromfile(FILENAME, dtype=np.csingle)


plt.plot(samples)
plt.show()

print(0)
