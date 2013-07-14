################################################################################
# program: tom_signal_tools.py
# author: Tom Irvine
# version: 1.0
# date: April 27, 2012
#              
################################################################################

from tompy import read_two_columns
from numpy import sqrt,pi,log
from numpy import array,zeros

################################################################################

def EnterPSD():
    """
    a = frequency column
    b = PSD column
    num = number of coordinates
    slope = slope between coordinate pairs    
    """
    print " "
    print " The input file must have two columns: freq(Hz) & accel(G^2/Hz)"

    a,b,num =read_two_columns()

    print "\n samples = %d " % num

    a=array(a)
    b=array(b)
    

    nm1=num-1

    slope =zeros(nm1,'f')


    ra=0

    for i in range (0,nm1):
#
        s=log(b[i+1]/b[i])/log(a[i+1]/a[i])
        
        slope[i]=s
#
        if s < -1.0001 or s > -0.9999:
            ra+= ( b[i+1] * a[i+1]- b[i]*a[i])/( s+1.)
        else:
            ra+= b[i]*a[i]*log( a[i+1]/a[i])

    omega=2*pi*a

    bv=zeros(num,'f') 
    bd=zeros(num,'f') 
        
    for i in range (0,num):         
        bv[i]=b[i]/omega[i]**2
     
    bv=bv*386**2
    rv=0

    for i in range (0,nm1):
#
        s=log(bv[i+1]/bv[i])/log(a[i+1]/a[i])
#
        if s < -1.0001 or s > -0.9999:
            rv+= ( bv[i+1] * a[i+1]- bv[i]*a[i])/( s+1.)
        else:
            rv+= bv[i]*a[i]*log( a[i+1]/a[i])         
         
        
    for i in range (0,num):         
        bd[i]=bv[i]/omega[i]**2
     
    rd=0

    for i in range (0,nm1):
#
        s=log(bd[i+1]/bd[i])/log(a[i+1]/a[i])
#
        if s < -1.0001 or s > -0.9999:
            rd+= ( bd[i+1] * a[i+1]- bd[i]*a[i])/( s+1.)
        else:
            rd+= bd[i]*a[i]*log( a[i+1]/a[i])         


    rms=sqrt(ra)
    three_rms=3*rms
    
    print " "
    print " *** Input PSD *** "
    print " "
 
    print " Acceleration "
    print "   Overall = %10.3g GRMS" % rms
    print "           = %10.3g 3-sigma" % three_rms

    grms=rms

    vrms=sqrt(rv)
    vthree_rms=3*vrms

    print " "
    print " Velocity " 
    print "   Overall = %10.3g in/sec rms" % vrms
    print "           = %10.3g in/sec 3-sigma" % vthree_rms

    drms=sqrt(rd)
    dthree_rms=3*drms

    print " "
    print " Displacement " 
    print "   Overall = %10.3g in rms" % drms
    print "           = %10.3g in 3-sigma" % dthree_rms

    return a,b,grms,num,slope