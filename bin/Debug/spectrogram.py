########################################################################
# program: spectrogram.py
# author: Tom Irvine
# version: 1.0
# date: July 19, 2011
# description:  Spectrogram of a signal.
#               The file must have two columns: time(sec) & amplitude
########################################################################

from tompy import read_two_columns,signal_stats
from tompy import time_history_plot

import matplotlib.pyplot as plt

from matplotlib.mlab import window_hanning

from numpy import array,zeros,log
from sys import stdin

########################################################################

print " The input file must have two columns: time(sec) & amplitude"

a,b,num =read_two_columns()

sr,dt,mean,sd,rms,skew,kurtosis,dur=signal_stats(a,b,num)


print "\n samples = %d " % num

print "\n  start = %8.4g sec  end = %8.4g sec" % (a[0],a[num-1]) 
print "    dur = %8.4g sec \n" % dur 


dtmin=1e+50
dtmax=0

for i in range(1, num-1):
    if (a[i]-a[i-1])<dtmin:
        dtmin=a[i]-a[i-1];
    if (a[i]-a[i-1])>dtmax:
        dtmax=a[i]-a[i-1];

print "  dtmin = %8.4g sec" % dtmin
print "     dt = %8.4g sec" % dt
print "  dtmax = %8.4g sec \n" % dtmax

print "  srmax = %8.4g samples/sec" % float(1/dtmin)
print "     sr = %8.4g samples/sec" % sr
print "  srmin = %8.4g samples/sec" % float(1/dtmax)

a=array(a)
b=array(b)

b-=sum(b)/len(b)  # demean

################################################################################


n=len(b)

ss=zeros(n)
seg=zeros(n,'f')
i_seg=zeros(n)

NC=0;
for i in range (0,1000):
    nmp = 2**(i-1)
    if(nmp <= n ):
        ss[i] = 2**(i-1)
        seg[i] =float(n)/float(ss[i])
        i_seg[i] = int(seg[i])
        NC=NC+1
    else:
        break

print ' '
print ' Number of    df    '
print ' Segments    (Hz)   dof'
   
for i in range (1,NC+1):
    j=NC+1-i
    if j>0:
        if( i_seg[j]>0 ):
            tseg=dt*ss[j]
            ddf=1./tseg
            print '%8d  %6.3f  %d' %(i_seg[j],ddf,2*i_seg[j])
    if(i==12):
        break;

ijk=0
while ijk==0:
    print(' ')
    print(' Choose the Number of Segments:  ')
    s=stdin.readline()
    NW = int(s)
    for j in range (0,len(i_seg)):   
        if NW==i_seg[j]:
            ijk=1
            break

# check

mmm = 2**int(log(float(n)/float(NW))/log(2))

mmmd2=mmm/2

################################################################################

print " "

time_history_plot(a,b,1,'Time(sec)','Amplitude','Time History','time_history') 

plt.figure(2)
plt.specgram(b, NFFT=mmm, Fs=sr, Fc=0,
         window=window_hanning, noverlap=mmmd2,
         cmap=None, xextent=None, pad_to=None, sides='default',
         scale_by_freq=None)

plt.xlabel('Time(sec)')
plt.ylabel('Freq(Hz)')
plt.title('Spectrogram')

plt.show()
