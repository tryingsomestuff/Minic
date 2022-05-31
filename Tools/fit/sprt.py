from __future__ import division
import sys
import math

def LL(x):
    return 1/(1+10**(-x/400))

def LLR(W,D,L,elo0,elo1):
    if W==0 or D==0 or L==0:
        return 0.0
    N=W+D+L
    w,d,l=W/N,D/N,L/N
    s=w+d/2
    m2=w+d/4
    var=m2-s**2
    var_s=var/N
    s0=LL(elo0)
    s1=LL(elo1)
    return (s1-s0)*(2*s-s0-s1)/var_s/2.0

def SPRT(W,D,L,elo0,elo1,alpha,beta):
    LR=LLR(W,D,L,elo0,elo1)
    print("LR", LR)
    LA=math.log(beta/(1-alpha))
    print("LA", LA)
    LB=math.log((1-beta)/alpha)
    print("LB", LB)
    if LR>LB:
        return 'H1'
    elif LR<LA:
        return 'H0'
    else:
        return 'not sure'

if __name__ == "__main__":
    print (SPRT(int(sys.argv[1]),int(sys.argv[2]),int(sys.argv[3]),int(sys.argv[4]),int(sys.argv[5]),float(sys.argv[6]),float(sys.argv[7])))

