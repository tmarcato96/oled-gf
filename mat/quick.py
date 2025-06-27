import numpy as np 

wvl =  [200, 250, 300, 350, 400, 450, 500, 550, 600, 650, 700, 750, 800, 850, 900]
real = [2.6000, 4.3500, 5.6700, 4.6176, 3.9996, 3.6099, 3.4224, 3.2399, 3.1683, 3.0624, 2.9928, 2.8899, 2.8222, 2.7554, 2.6567]
imag = [2.8800, 3.0800, 1.4400, 0.3010, 0.0800, 0.0380, 0.0370, 0.0360, 0.0356, 0.0350, 0.0346, 0.0340, 0.0504, 0.0498, 0.0489]

wvl = np.asarray(wvl).reshape(-1, 1)
real = np.asarray(real).reshape(-1, 1)
imag = np.asarray(imag).reshape(-1, 1)

absol = np.sqrt(real**2 + imag**2)
n = np.sqrt((absol+real)/2)
k = np.sqrt((absol-real)/2)

res = np.concat((wvl, n, k), axis=1)
np.savetxt("test_ito.csv", res, '%.5f', delimiter=',')

