#This file should contain functions to interact with the commands on the piPico-Lockin
#Or even better: a class. That would solve the problems with globals by using self.variable??
#code to find com port will only work on WINDOWS
import copy
import time

import serial
import serial.tools.list_ports
from scipy import signal
import numpy as np

newestFirmware = 0.1

class lockIn():
    """
    class that handles all communication with a RP2040 / piPico based Lock-In Amplifier
    """
    port = 'none'
    firmware = 0
    ser = serial.Serial(timeout=1)
    preDS = 1
    postDS = 1
    ADCfreq = 50000 #Hz
    def connect(self): #returns three values: first: true:connected / False:no connection second: list of tuples. each tuple contains the port at index 0 and the firmware version as float on index 1 third return value: string message
        #Find port
        '''
        This function will fail to identify a LockIn that is currently executing a command and does not respond to a 'h' command.
        '''
        NrOfRP2040 = 0
        self.port = 'none'
        self.firmware = 0
        ports = list(serial.tools.list_ports.comports())
        goodPorts = [] #list to store all potential candidates. Used for selecting in a multi LockIn scenario. Only piPico based LockIns are listed
        print('available COM ports:')
        for p in ports:
            print(p.device +" "+ p.description)
            '''print(p.hwid)
            print(p.vid)
            print(p.pid)
            print(p.serial_number)'''
            if 11914 == p.vid: #usb vid of rasperry pi foundation is 11914. This does however not check that an actual Lock In is attached, for that the version identification would be helpfull
                print("This is an RP2040!")
                NrOfRP2040 += 1
                # check firmware version
                # read from first line of help (h command). this also makes sure that a LockIn is attached by reding the description in the first help line
                self.ser.port = p.device
                self.ser.open()
                self.ser.write(b'h')
                self.ser.flushOutput() #Necessary???
                reply = self.ser.readline() #first line is just platform information, disregard for now
                reply = self.ser.readline()
                reply = self.ser.readline()
                self.ser.close()
                print('reply:')
                print(reply)
                if b'Digital LockIn Firmware Version ' in reply:
                    #reply.lstrip("Digital LockIn Firmware Version ")
                    # Example string containing non-numeric characters and numeric characters of the float
                    string_with_mixed_chars = reply.decode('utf-8')
                    # Extract numeric characters from the string
                    numeric_chars = [char for char in string_with_mixed_chars if char.isdigit() or char == '.']
                    # Join the numeric characters into a string
                    numeric_string = ''.join(numeric_chars)
                    # Parse the float from the numeric string
                    thisFirmware = float(numeric_string)
                    goodPorts.append((str(p.device),thisFirmware))
                    if thisFirmware >= newestFirmware: #only save working devices
                        # save port
                        self.firmware = thisFirmware
                        self.port = p.device  # SOLVED: By making a class. TODOthis does not work? python does not allow maipulation of globals?
                    #else:
                    #    self.firmware = thisFirmware

        #if no port or multiple ports are found or firmware is too old exit with warning
        status = True
        msg = ''
        self.ser.setPort(self.port)
        if NrOfRP2040 > 1:
            msg = ('multiple RP2040 found! RP2040 on port ' + self.port + ' is chosen.')
        if self.port == 'none':
            self.port = 'none'
            msg = ('No Device found. Try plugging out (at least 2 sec.) and in to reinitialize handshake')
            status = False
        elif self.firmware == 0:
            msg = ('Firmware too old. Update to V...?')
            status = False
        print(msg)
        return(status, goodPorts, msg)

    def pollData(self): #returns two arrays: X & Y containing all X and Y tuples from the buffer. Only complete tuples are read, len(X) is always len(Y)
        if self.ser.port is not None:  # Check if there is a port found at all
            if self.ser.is_open == False:
                self.ser.open()
            bytesIn = self.ser.in_waiting
            #print(bytesIn)
            newMsg = b""
            if bytesIn >= 3:
                #print('waiting:' + str(bytesIn))
                newMsg += self.ser.read(bytesIn)
            else:
                #print('nothing to read')
                pass
            Xs, Ys = self.parse_integers_from_string(newMsg)
            #if parsed == None:
            #return(np.empty((0,0)), np.empty((0,0)))
            #else:
            if Xs is not None:
                Xs = Xs/(self.preDS)#*postDS #correct for summing in preDS on uC. postDC doesnt average (yet)!
                Ys = Ys / (self.preDS)
            return(Xs, Ys)
        else:
            return (np.empty((0)), np.empty((0)))

    def stopStream(self): #abort command, reset serial buffer and close port. This is needed in order for the next handshake (connecting) to work. Plugging out and in is an alternative
        if self.ser.port is not None: #Check if there is a port found at all
            if self.ser.is_open == False:
                self.ser.open()
            self.ser.write(b'e') #does this work?
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            self.ser.close()
            #serialsend 'e'

    def startExtRef(self, tau, filterType, filterOd, samplsPerSec):  # tau in s, samplsPerSec is the actual samples sent to PC, got nothing to do with sampling or FIFO rate
        """
        start LockIn Measurement with external (TTL) reference Freq.
        :param tau: lowpass filter timeconstant in seconds
        :param filterType: 'Bessel', 'Chebyshev' or 'Butterworth' IIR filter
        :param filterOd: Order of IIR filter
        :param samplsPerSec: samples per second that shall be returned via COM port. this variable will be slightly altered due to int division restrictions
        :return: self.ADCfreq,self.preDS,self.postDS,b,a......most of this variables are also accessible as variable of the class
        """
        if False:#tau > 1:
            self.ADCfreq = 1000*1/(tau) #something like this to reduce sampling rate for super low data Rates? Maybe only with internal Reference.
        else:
            self.ADCfreq = 1e5
        fny = self.ADCfreq/2
        f3dB = 1/(2*np.pi*tau)
        #compute ideal preDS factor, compute filter coeff,
        if filterType=='Bessel' or filterType=='bessel':
            b,a = signal.bessel(filterOd,f3dB/fny)
        elif filterType == 'Cheby' or filterType == 'cheby' or filterType == 'Chebyshev' or filterType == 'chebyshev':
            b, a = signal.cheby1(filterOd, 0.001, f3dB / fny)
        elif filterType == 'Butter' or filterType == 'butter' or filterType == 'Butterworth' or filterType == 'butterworth':
            b,a = signal.butter(filterOd,f3dB/fny)
        else:
            print('Unknown filter Type!')
            return(None,None,None,None,None)
        self.preDS = int(np.round(self.ADCfreq/10000 + 0.5)) #roundup, limits fifo rate to smaller or equal 10000. Hausnummer!
        if self.preDS > 255:
            self.preDS = 255 #limit to byte range
        self.postDS = int(np.round(self.ADCfreq/self.preDS / samplsPerSec))
        #serial send e to exit
        #serial send command letter
        #serial send all the parameters in the right order
        if self.ser.port is not None: #Check if there is a port found at all
            if self.ser.is_open == False:
                self.ser.open()
            self.ser.reset_input_buffer()
            self.ser.reset_output_buffer()
            print_t = 0.15
            self.ser.write(b'e') #(hopefully) exit the last program
            time.sleep(0.1)
            self.ser.write(b'r') #start with ext reference
            time.sleep(print_t)
            self.ser.write(b'n') #say no to old parameters
            time.sleep(print_t)
            self.ser.write(str(int(self.ADCfreq/1000)).encode('utf-8')) #send in kHz int
            time.sleep(print_t)
            self.ser.write(str(self.preDS).encode('utf-8'))
            time.sleep(print_t)
            self.ser.write(str(self.postDS).encode('utf-8'))
            time.sleep(print_t)
            print('filterorder to send: ', str(int(filterOd)).encode('utf-8'))
            self.ser.write(str(int(filterOd)).encode('utf-8'))
            time.sleep(print_t)
            #self.ser.write(str(int(tau*1000)).encode('utf-8')) #Tau in Nr of downsampled samples of lowpass, just for now until IIR is implemented
            #time.sleep(print_t)
            for i in range(filterOd):
                self.ser.write(str(int(a[i]*32768)).encode('utf-8'))
                time.sleep(print_t)
                self.ser.write(str(int(b[i]*32768)).encode('utf-8'))
                time.sleep(print_t)
            print(self.readAllLines())
            self.ser.write(b'y') #press enter to start doesnt actually require enter, y is fine
            time.sleep(print_t)
            #print(self.readAllLines())
        return(self.ADCfreq,self.preDS,self.postDS,b,a)

    def startIntRef(self, tau, filterType, filterOd, samplsPerSec, refFreq): #this function starts a Lock In Measurement with a specified internal reference frequency. To receive Data, use pollData
        if refFreq < 100:
            self.ADCfreq = 1000*refFreq #something like this to reduce sampling rate for super low data Rates? Maybe only with internal Reference.
        else:
            self.ADCfreq = 1e5
    def is_connected(self): #this closes and reopens the port, use with care! Returns True if port is working, False if not
        if self.ser.port == None:
            print('No port defined')
        try:
            if self.ser.is_open:
                print('closing port')
                self.ser.close()
            print('trying to open ', self.ser.port)
            self.ser.open()
        except:
            print('failed to open')
            return False
        print('success to open')
        return True
    def readAllLines(self): # read all lines for a maximum of 2 seconds
        self.ser.timeout = 0.05
        Msg = b''
        endtime = time.time() + 2
        while(time.time() < endtime):
            newMsg = self.ser.readline()
            if(newMsg==b''):
                self.ser.timeout = 1
                return (Msg.decode('utf-8'))
                break
            Msg += newMsg
    last_line = ""
    def parse_integers_from_string(self,input_string):  # maybe slow, try numpy 2d array of shape (N,2)
        if isinstance(input_string, bytes):
            input_string = input_string.decode('utf-8') #used for byte literals, but somehow isnt(or is it?) needed becaus read return string??? or newmessage is initialized as string???
        if len(input_string) > 0:
            incomplete = 0 if input_string[-1] == '\n' else 1
            input_string = self.last_line + input_string  # Add the last incomplete line to the beginning of the input string
            lines = input_string.split('\n')
            x_list = np.zeros(len(lines)-1)
            y_list = np.zeros_like(x_list)
            for i, line in enumerate(lines[0:-1]):  # Skip the last line if it is incomplete or skip the last empty line if the string is complete
                # Split the line by comma
                parts = line.split(',')
                #print(line)
                try:
                    # Parse X and Y as integers and append them to the list
                    x, y = map(int, parts)
                    x_list[i] = x
                    y_list[i] = y
                except ValueError:
                    print('Theres garbage that cannot be parsed :.(')  # Ignore lines that cannot be parsed as integers

            # Save the last incomplete line
            if incomplete:
                self.last_line = lines[-1]
            else:
                self.last_line = ""
            return x_list, y_list
        else:
            return(np.empty((0)), np.empty((0)))
