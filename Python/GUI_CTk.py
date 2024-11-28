#import multitasking
import matplotlib.pyplot as plt
import numpy as np

import matplotlib
matplotlib.use("TkAgg")
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk
import matplotlib.animation as animation
from matplotlib import style
from matplotlib.figure import Figure
style.use("ggplot")

import tkinter as tk
from tkinter import messagebox
import customtkinter as ctk
ctk.set_appearance_mode("dark")  # Modes: system (default), light, dark
ctk.set_default_color_theme("blue")  # Themes: blue (default), dark-blue, green

from LockInAPI import lockIn



class myGUI ():
    li = lockIn()
    fontSize = 12
    #init plot---
    f = Figure(figsize=(8, 5), dpi=100)
    sbplt = f.add_subplot(111)
    Nplot = 100 #DataPoints in plot
    plotTime = 5 #Time of plot = samplingTime * DataPoints = Nplot/ samplesPerSec
    ani_is_paused = True
    samplesPerSec = 200
    samplesPerSecOut = 200
    xList = np.zeros(Nplot)
    yList = np.zeros(Nplot)

    divider: int=1
    refFreq: float = 1
    tau: float = 1

    def __init__(self):
        self.loadParam()

        self.root = ctk.CTk()
        self.root.geometry("1000x850")
        self.root.title("Hipp USB-Lock-In Interface")
        self.root.protocol("WM_DELETE_WINDOW", self.onClose)
        #first row: connection menu-------------------------------------------
        self.connectFrame = ctk.CTkFrame(self.root)
        self.connectFrame.columnconfigure(0, weight=1)
        self.connectFrame.columnconfigure(1, weight=3)
        self.connectBtn = ctk.CTkButton(self.connectFrame, text="connect", font=('Arial', self.fontSize), command=self.connectEvent)
        self.connectBtn.grid(row=0, column=0, padx=40, pady=10, sticky=tk.W)
        self.connectionLabel = ctk.CTkLabel(self.connectFrame, text=str(self.li.port)+" V"+str(self.li.firmware), font=('Arial',self.fontSize), fg_color='darkred', padx=20, pady=5, corner_radius=10)
        self.connectionLabel.grid(row=0, column=1, padx=10, pady=10, sticky=tk.W)
        self.connectFrame.pack(fill='x')
        #Lock In Parameter Menu, Textboxes etc.---------------------------------------------------
        self.paramFrame = ctk.CTkFrame(self.root)
        self.paramFrame.columnconfigure(0, weight=3)
        self.paramFrame.columnconfigure(1, weight=3)
        self.paramFrame.columnconfigure(2, weight=3)
        self.paramFrame.columnconfigure(3, weight=1)
        self.paramFrame.columnconfigure(4, weight=3)
        self.paramFrame.columnconfigure(5, weight=1)
        #Radio Buttons---------------
        self.radioValue = tk.StringVar()  # 'ext', 'int' or 'none'
        self.radioValue.set('ext')
        self.intRadio = ctk.CTkRadioButton(self.paramFrame, text='Int. Ref.', command=self.intRef, variable=self.radioValue, value='int',font=('Arial', self.fontSize))
        self.intRadio.grid(row=0, column=0, padx=10, pady=10, sticky=tk.W)
        self.extRadio = ctk.CTkRadioButton(self.paramFrame, text='Ext. Ref.', command=self.extRef, variable=self.radioValue, value='ext',font=('Arial', self.fontSize))
        self.extRadio.grid(row=1, column=0, padx=10, pady=10, sticky=tk.W)
        #-----------------
        self.refFreqDivLabel = ctk.CTkLabel(self.paramFrame, text="Divider", font=('Arial', self.fontSize)) #ext is standard, therfore divider is standard
        self.refFreqDivLabel.grid(row=0, column=1, padx=10, pady=10, sticky=tk.W)
        self.tauLabel = ctk.CTkLabel(self.paramFrame, text="Timeconstant", font=('Arial', self.fontSize))
        self.tauLabel.grid(row=1, column=1, padx=10, pady=10, sticky=tk.W)
        self.refFreqDivEntry = ctk.CTkEntry(self.paramFrame, )
        self.refFreqDivEntry.grid(row=0, column=2, padx=10, pady=10, sticky=tk.W)
        self.tauEntry = ctk.CTkEntry(self.paramFrame, )
        self.tauEntry.grid(row=1, column=2, padx=10, pady=10, sticky=tk.W)
        self.tauEntry.bind('<FocusOut>', self.update_tau)
        self.tauEntry.bind('<Return>', self.update_tau)
        self.refFreqDivDropValue = tk.StringVar()
        self.refFreqDivDropValue.set("X")
        self.refFreqDivDrop = ctk.CTkOptionMenu(self.paramFrame,variable=self.refFreqDivDropValue, values=["mHz", "Hz", "kHz"])
        self.refFreqDivDrop.configure(state=tk.DISABLED)
        self.refFreqDivDrop.grid(row=0, column=3, padx=10, pady=10, sticky=tk.W)
        self.tauDropValue = tk.StringVar(value="ms")
        self.tauDrop = ctk.CTkOptionMenu(self.paramFrame, variable=self.tauDropValue, values=["ms", "s"])
        self.tauDrop.grid(row=1, column=3, padx=10, pady=10, sticky=tk.W)
        #Filter order and type menu
        self.filterOdLabel = ctk.CTkLabel(self.paramFrame, text="Filter Order", font=('Arial', self.fontSize))
        self.filterOdLabel.grid(row=0, column=4, padx=10, pady=10, sticky=tk.W)
        self.filterTypeLabel = ctk.CTkLabel(self.paramFrame, text="Filter Type", font=('Arial', self.fontSize))
        self.filterTypeLabel.grid(row=1, column=4, padx=10, pady=10, sticky=tk.W)

        self.filterOdDropValue = tk.StringVar()
        self.filterOdDropValue.set(1)
        self.filterOdDrop = ctk.CTkOptionMenu(self.paramFrame, variable=self.filterOdDropValue, values=["1", "2", "3", "4", "6", "8"])
        self.filterOdDrop.grid(row=0, column=5, padx=10, pady=10, sticky=tk.W)
        self.filterTypeValue = tk.StringVar()
        self.filterTypeValue.set("Bessel")
        self.filterTypeDrop = ctk.CTkOptionMenu(self.paramFrame, variable=self.filterTypeValue, values=["Bessel", "Butterworth", "Chebyshev"])
        self.filterTypeDrop.grid(row=1, column=5, padx=10, pady=10, sticky=tk.W)
        self.paramFrame.pack(fill='x')
        self.samplesLabel = ctk.CTkLabel(self.paramFrame, text="received samples / s", font=('Arial', self.fontSize))
        self.samplesLabel.grid(row=2, column=1, padx=10, pady=10, sticky=tk.W)
        self.samplesEntry = ctk.CTkEntry(self.paramFrame, )
        self.samplesEntry.grid(row=2, column=2, padx=10, pady=10, sticky=tk.W)
        self.samplesEntry.bind('<FocusOut>', self.update_samples)
        self.samplesEntry.bind('<Return>', self.update_samples)

        #Run menu
        self.runBtn = ctk.CTkButton(self.paramFrame, text='RUN', command=self.startLockIn)
        self.runBtn.grid(row = 3, column = 0, padx = 10, pady = 10, sticky = tk.W)
        self.plotTimeLabel = ctk.CTkLabel(self.paramFrame, text="X-Axis: Time", font=('Arial', self.fontSize))
        self.plotTimeLabel.grid(row=3, column=1, padx=10, pady=10, sticky=tk.W)
        self.plotTimeEntry = ctk.CTkEntry(self.paramFrame, )
        self.plotTimeEntry.grid(row=3, column=2, padx=10, pady=10, sticky=tk.W)
        self.plotTimeEntry.bind('<FocusOut>', self.update_plotTime)
        self.plotTimeEntry.bind('<Return>', self.update_plotTime)
        self.saveBtn = ctk.CTkButton(self.paramFrame, text='save to', command=self.saveData)
        self.saveBtn.grid(row=3, column=3, padx=10, pady=10, sticky=tk.W)
        self.saveEntry = ctk.CTkEntry(self.paramFrame, )
        self.saveEntry.grid(row=3, column=4, padx=10, pady=10, sticky=tk.W)
        self.saveLabel = ctk.CTkLabel(self.paramFrame, text=".csv", font=('Arial', self.fontSize))
        self.saveLabel.grid(row=3, column=5, padx=10, pady=10, sticky=tk.W)

        #init the figure and its Frame
        self.plotFrame = ctk.CTkFrame(self.root)
        self.label2 = ctk.CTkLabel(self.plotFrame, text="Graph Page!")
        self.label2.pack(pady=10, padx=10)
        self.canvas = FigureCanvasTkAgg(self.f, self.plotFrame)
        self.canvas.draw()
        self.canvas.get_tk_widget().pack(side=tk.BOTTOM, fill=tk.BOTH, expand=True)
        self.toolbar = NavigationToolbar2Tk(self.canvas, self.plotFrame)
        self.toolbar.update()
        self.canvas._tkcanvas.pack(side=tk.TOP, fill=tk.BOTH, expand=True)
        self.ani = animation.FuncAnimation(self.f, self.animate, interval=20)
        self.tList = np.linspace(-5, 0, self.Nplot)
        self.xLine, = self.sbplt.plot(self.tList, self.xList, color='k')
        self.yLine, = self.sbplt.plot(self.tList, self.yList, color='r')
        self.plotFrame.pack()

        self.root.mainloop()
    def animate(self,i):
        if self.ani_is_paused == False:
            #tList = np.linspace(-5,0,self.Nplot)
            #print('pollingData')
            newDataX, newDataY = self.li.pollData()
            if len(newDataX) > 0 and len(newDataY) > 0:
                if len(newDataX > self.Nplot):
                    newDataX = newDataX[-self.Nplot:]
                    newDataY = newDataY[-self.Nplot:]
                self.xList = np.roll(self.xList, -len(newDataX))
                self.xList[-len(newDataX):] = newDataX
                self.xLine.set_ydata(self.xList)
                self.yList = np.roll(self.yList, -len(newDataY))
                self.yList[-len(newDataY):] = newDataY
                self.yLine.set_ydata(self.yList)
                self.sbplt.set_ylim((np.min([self.xList,self.yList])-1,np.max([self.xList,self.yList])+1))
            #self.sbplt.clear()
            #self.sbplt.plot(xList, self.yList)
            #self.sbplt.set_ylim((0,1))
            #plt.patuse(100)
    def replot(self):
        print('plotTime', self.plotTime)
        print('Nplot: ', self.Nplot)
        self.tList = np.linspace(-self.plotTime, 0, self.Nplot) # +time.time()?
        tempxList = self.xList
        tempyList = self.yList
        self.xList = np.zeros(self.Nplot)
        self.yList = np.zeros(self.Nplot)
        if self.Nplot > len(tempxList):
            self.xList[-len(tempxList):] += tempxList
            self.yList[-len(tempyList):] += tempyList
        else:
            self.xList += tempxList[-self.Nplot:]
            self.yList += tempyList[-self.Nplot:]
        self.sbplt.clear()
        self.xLine, = self.sbplt.plot(self.tList, self.xList, color='k')
        self.yLine, = self.sbplt.plot(self.tList, self.yList, color='r')

    def connectEvent(self):
        status, portsList, connectMsg = self.li.connect()
        #connectMsg untested!!!!!
        connectMsg += "\n"
        for i in range(len(portsList)):
            portInfo = portsList[i]
            connectMsg += str(portInfo[0]) + " firmware V" + str(portInfo[1]) +"\n"
        if status:
            messagebox.showinfo(title="Available COM ports", message="Available COM ports:\n"+connectMsg)
            self.connectionLabel.configure(fg_color='darkgreen')
        else:
            messagebox.showinfo(title="No working device found", message="Connecting unsucsessfull. "+ connectMsg)
            self.connectionLabel.configure(fg_color='darkred')
        # update label
        self.connectionLabel.configure(text=str(self.li.port)+" firmware V"+str(self.li.firmware))

    def startLockIn(self):
        """function to send start lock in command to uC, using the parameters from the radio button, entrys and dropdownmenu"""
        #adjust reference frequency to SI
        if self.refFreqDivDropValue == 'mHz':
            refFreq_s = self.refFreq/1000
        if self.refFreqDivDropValue == 'kHz':
            refFreq_s = self.refFreq*1000
        else:
            refFreq_s = self.refFreq
        print(str(refFreq_s)+'Hz')
        #adjust timeconstant tau to SI
        if self.tauDropValue == 'ms':
            tau_s = self.tau/1000
        else:
            tau_s = self.tau
        #send parameters to USB LockIn and start measuring
        if self.li.is_connected():
            if self.radioValue.get() == 'int':
                print('starting LockIn with Int ref.')
                self.li.startIntRef()
            elif self.radioValue.get() == 'ext':
                print('starting LockIn with EXT ref.')
                self.li.startExtRef(tau_s, self.filterTypeValue.get(), int(self.filterOdDropValue.get()),  self.samplesPerSecOut)
            print('preDS', self.li.preDS)
            print('postDS: ', self.li.postDS)
            self.samplesPerSec = self.li.ADCfreq/(self.li.preDS*self.li.postDS) #not ideal yet, becaus ADCfreq is not actual. it is 1/(some int * 1/133Meg.) and therefor has roundoff error. around 50kHz error is max 10Hz or 0.02%
            print('actual samples per sec.: ', self.samplesPerSec)
            self.Nplot = int(self.plotTime * self.samplesPerSec)
            self.replot()
            self.ani_is_paused = False
            self.runBtn.configure(text='STOP', command=self.stopLockIn)
        else:
            messagebox.showinfo(title="Error", message="Unable to start Lock-In: No device connected")
    def stopLockIn(self):
        '''stops the lockIn command on the uC, pauses animation and resets the button to RUN'''
        self.li.stopStream()
        self.ani_is_paused = True
        self.runBtn.configure(text='RUN', command=self.startLockIn)

    def intRef(self):
        """changes the divider entry and dropdownmenu to a reference freq entry and drop down menu"""
        self.refFreqDivLabel.configure(text="Reference freq.")
        self.refFreqDivDropValue.set("Hz")
        self.refFreqDivDrop.configure(state=tk.NORMAL)
    def extRef(self):
        """changes the ref freq. entry and drop down menu to a divider entry and blocks the drop down menu"""
        self.refFreqDivLabel.configure(text="Divider")
        self.refFreqDivDropValue.set("X")
        self.refFreqDivDrop.configure(state=tk.DISABLED)

    def update_samples(self, event):
        """function used to update and limit samplesPerSec when samples entry is commited with return or out of focus"""
        tempSamples = 0
        try:
            tempSamples = int(self.samplesEntry.get())
        except:
            messagebox.showinfo(title="Input Error", message="Samples not numeric")
        if tempSamples < 1:
            tempSamples = 1
        if tempSamples > 1000:
            tempSamples = 1000
        self.samplesEntry.delete(0, tk.END)  # Clear any existing value
        self.samplesEntry.insert(0, str(tempSamples))
        self.samplesPerSecOut = tempSamples

    def update_tau(self, event):
        """function used to update and limit tau when tau entry is commited with return or out of focus"""
        tempTau = 0
        try:
            tempTau = float(self.tauEntry.get())
        except:
            messagebox.showinfo(title="Input Error", message="Tau not numeric")
        if tempTau < 1:
            tempTau = 1
        self.tauEntry.delete(0, tk.END)  # Clear any existing value
        self.tauEntry.insert(0, str(tempTau))
        self.tau = tempTau

    def update_plotTime(self, event):
        """function used to update and limit plotTime when entry 'X-Axis-Time' is commited with return or out of focus"""
        tempTime = 0
        try:
            tempTime = float(self.plotTimeEntry.get())
        except:
            messagebox.showinfo(title="Input Error", message="X-Axis Time not numeric")
        if tempTime < 0.1:
            tempTime = 0.1
        if tempTime > 500:
            tempTime = 500
        self.plotTimeEntry.delete(0, tk.END)  # Clear any existing value
        self.plotTimeEntry.insert(0, str(tempTime))
        self.plotTime = tempTime+0
        self.Nplot = int(self.plotTime*self.samplesPerSec)
        self.replot()

    def saveData(self):
        """Saves data to a file in the same directory called "whats in the entry box"+.csv.
        Everything that is in the plot is saved, including a time vector. First line is heading and units"""
        dataToSave = np.vstack((self.tList, self.xList, self.yList))
        dataToSave = dataToSave.transpose()
        print(dataToSave)
        print(np.shape(dataToSave))
        np.savetxt(self.saveEntry.get()+'.csv', dataToSave, delimiter=',', header='time / s, X / digit, Y / digit')
        #messagebox.showinfo(title="Input Error", message="You better code this shit!")

    def onClose(self):
        try:
            self.li.stopStream()
            self.saveParam()
        except:
            pass
        messagebox.showinfo(title="Exit LockIn GUI?", message="You may have unstored data!")
        self.root.destroy()

    def saveParam(self):
        """NOT READY: function to store the parameters in the dropdownmenus, entrys etc."""
        pass
        #dave int and ext parameters to file for next startup
    def loadParam(self):
        """NOT READY: function to save all he parameters in the dropdownmenus, entrys etc. """
        try:
            storedParam = np.loadtxt('parameters.txt')
        except:
            print('No stored parameters found')
thisGUI = myGUI()

#samplig rate fixed?
#sample rate to PC changeable


#start plotter and logger in seperate (background ) threads
#https://pypi.org/project/multitasking/