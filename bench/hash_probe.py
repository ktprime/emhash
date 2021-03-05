import matplotlib.pyplot as plt 
import numpy as np
import math

#fp = np.arange(10)/10
#(e^(-L) * L^k / k!.
#0 (e^-L)
#1 (e^-L * L)
#2 (e^-L * L^2 / 2)
#3 (e^-L * L^3 / 6)
# miss = f(k) * k
#      = f(0) + f(1) * 2 + f(2) * 3 ....

# https://cs.stackexchange.com/questions/10273/hash-table-collision-probability
# From
# NUMBER OF PROBES / LOOKUP       Successful            Unsuccessful
# Quadratic collision resolution   1 - ln(1-L) - L/2    1/(1-L) - L - ln(1-L)
# Linear collision resolution     [1+1/(1-L)]/2         [1+1/(1-L)2]/2
# separator chain resolution      1 + L / 2             e^-L + L
# -- enlarge_factor --           0.10  0.50  0.60  0.75  0.80  0.90  0.99
# QUADRATIC COLLISION RES.
#    probes/successful lookup    1.05  1.44  1.62  2.01  2.21  2.85  5.11
#    probes/unsuccessful lookup  1.11  2.19  2.82  4.64  5.81  11.4  103.6
# LINEAR COLLISION RES.
#    probes/successful lookup    1.06  1.5   1.75  2.5   3.0   5.5   50.5
#    probes/unsuccessful lookup  1.12  2.5   3.6   8.5   13.0  50.0
# SEPARATE CHAN RES.
#    probes/successful lookup    1.05  1.25  1.3   1.25  1.4   1.45  1.50
#    probes/unsuccessful lookup  1.00  1.11  1.15  1.22  1.25  1.31  1.37


# coll = f(k) * k / (k + 1)
# main = f(k) * 1 / (k + 1)
# miss = f(k) * (k + 1)
def poisson(lf):
	miss = powk = np.exp(-lf)
	main, coll = powk, 0 
	for k in range(1,100):
#		if lf == 0.75:
#			print(k, powk)
		powk *= lf / k
		miss += powk * (k + 1)
		coll += powk * k / (k + 1)
		main += powk * 1 / (k + 1)
		if powk < 1e-10:
			break
	miss = miss * lf + (1 - lf)
	return miss, coll, main * lf, miss + 1 / (1 - lf)

# 准备数据
x = np.array([0.1,0.5,0.6,0.75,0.8,0.91,0.998])
x = np.arange(0,99,2) / 100.0
#print(x)
all = [poisson(lf) for lf in x]
#print('all = ', all)
find_miss = [e[0] for e in all]
coll = [e[1] for e in all]
main = [e[2] for e in all]
insert = [e[3] for e in all]
insert = [(1 + 1/math.pow(10,1 - e)) / 2 for e in x]
#ff = 0.80
#print(ff, collsion(ff), poisson(ff))

#find_miss = [poisson(lf)[0] for lf in x]
print('find_miss = ', find_miss)
#冲突概率
print('coll = ', coll)
#main bucket
print('main = ', main)

find_hit = 1 + x / 2


# 绘制数据
plt.subplot(1, 2, 1)
plt.plot(x, find_miss, label='miss')


plt.plot(x, find_hit, label='hit')
#plt.plot(x, insert, label='search')
z = x + np.exp(1-x)
#plt.plot(x, z, label='poisson')

plt.legend()
plt.ylabel('find probes')
plt.xlabel('load factor')
plt.title('chaining probes')

##冲突&main bucket
plt.subplot(1, 2, 2)
plt.plot(x, coll, label='coll')
plt.plot(x, main, label='bucket')


plt.xlabel('load factor')
plt.ylabel('ration')
plt.title('main bucket & collsion')
plt.legend()
plt.show()

plt.savefig('hash_map.png')


# NUMBER OF PROBES / LOOKUP       Successful            Unsuccessful
# Quadratic collision resolution  1 - ln(1-L) - L/2     1/(1-L) - L - ln(1-L)
# Linear collision resolution     [1+1/(1-L)]/2         [1+1/(1-L)2]/2
# separator chain resolution      1 + L / 2             exp(-L) + L


# gcc 9.2.0 __cplusplus = 201703 x86-64 OS = Win, cpu = Intel(R) Core(TM) i7-7500U CPU @ 2.70GHz
