#!/usr/bin/python
import time
import pypm
import liblo

moodpd_addr= 'osc.udp://localhost:4243/' 	#'osc.udp://trieste:4243/'


def FindControllerDevice():
    for loop in range(pypm.CountDevices()):
        interf,name,inp,outp,opened = pypm.GetDeviceInfo(loop)
        if 'nanoKONTROL' in name and inp==1: return loop
    return -1
    
if __name__ == '__main__':
    midi= pypm.Input(FindControllerDevice())
    controllerState= [0]*128
    while True:
        while not midi.Poll(): time.sleep(.01)   # no way to read blocking?!
        MidiData = midi.Read(32)
        for msg in MidiData[-1:]:   # only use the last message. the dali controllers won't react that fast anyway.
            if msg[0][0]>>4==0x0B:
                #print msg[0][0]," ",msg[0][1]," ",msg[0][2]
                controller= int(msg[0][1])
                value= int(msg[0][2]*2)
                controllerState[controller]= value
                if controller >= 32:                    # buttons
                        lampno= (controller%4)
                        path= '/dali/lamps/%02d/bright' % (lampno+1)
                        oscmsg= liblo.Message(path)
                        oscmsg.add(value)
                        liblo.send('osc.udp://172.22.83.5:4243/', oscmsg)
                elif controller<3 or controller>=16:    # moodlamp
                    if controller>=16:      # panning controllers for small lamps
                        lampno= ((controller-16)/3%4)
                        path= '/moodpd/lamps/%02d/rgb' % (lampno+1)
                        #~ print controller, lampno, path
                        oscmsg= liblo.Message(path)
                        oscmsg.add(controllerState[lampno*3+16])
                        oscmsg.add(controllerState[lampno*3+17])
                        oscmsg.add(controllerState[lampno*3+18])
                    else:                   # main moodlamp
                        path= '/moodpd/lamps/00/rgb'
                        oscmsg= liblo.Message(path)
                        oscmsg.add(controllerState[0])
                        oscmsg.add(controllerState[1])
                        oscmsg.add(controllerState[2])
                    liblo.send(moodpd_addr, oscmsg)
                else:                                   # dali
                    path= '/dali/lamps/%02d/bright' % ((controller%4)+1)
                    oscmsg= liblo.Message(path)
                    oscmsg.add(value)
                    liblo.send('osc.udp://172.22.83.5:4243/', oscmsg)
