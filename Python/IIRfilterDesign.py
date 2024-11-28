import numpy as np
import matplotlib.pyplot as plt
from scipy import signal


f3dB = 100
sim = True

fig, axs = plt.subplots(3,1)

if sim == False:
    dt = 0.001
    fsample = 1 / dt
    fny = fsample / 2
    data = np.loadtxt('ArduinoInput.txt')
    t = np.arange(len(data)) * dt
else:
    dt = 0.0001 #10kHz sampling rate
    fsample = 1 / dt
    fny = fsample / 2
    t = np.arange(1000)*dt
    #data = np.append(np.zeros(500),np.ones(500))
    #data = np.sin(100*2*np.pi*t) +  np.sin(530*2*np.pi*t)
    data = np.append(np.zeros(500), np.ones(500))+np.random.normal(0, 0.02, 1000)


#b,a = signal.butter(2,f3dB/fny)
#b,a = signal.bessel(2,0.001,f3dB/fny)
b,a = signal.bessel(2,f3dB/fny)

x = np.append(np.zeros(2), data)
y = np.zeros_like(x)
for i in np.arange(len(y)):
    if(i < 3):
        z = 1
    else:
        y[i] = (x[i] * b[0] + x[i-1] * b[1] + x[i-2] * b[2] - y[i-1] * a[1] - y[i-2] * a[2])/a[0]

x = x[2:]
y = y[2:]

axs[0].plot(t, data, label='data')
axs[0].plot(t, y, label='filter')
#plt.plot(t,signal.lfilter(b,a,x), label ='lfilter')
axs[0].legend()
fig.tight_layout()

w, h = signal.freqz(b,a)
w = w/(2*np.pi)*fsample

axs[1].loglog(w, abs(h),'k')
#axs[2].semilogx(w,180/np.pi*np.arctan2(np.real(h),np.imag(h)))
axs[2].semilogx(w,180/np.pi*np.unwrap(np.angle(h))) #should be the same as the line before
plt.show()

print(b)
print(a)