import sys
import select
from matplotlib import pyplot as plt
import numpy
import scipy

Cycle = [int(x) for x in sys.stdin.readline().strip().split(' ')]
ft = scipy.fft(Cycle)
ft = numpy.abs(ft)
x = list(range(48000,208000,16))
plt.plot(x,ft[3000:13000])
plt.show()


 
  
