#!/usr/bin/env python
import serial

ser = serial.Serial()   # open first serial port
ser.timeout = 1
ser.writeTimeout = 1
ser.interCharTimeout = 0
ser.baudrate = 57600
ser.port = '/dev/tty.usbserial'
ser.parity = serial.PARITY_ODD
ser.xonxoff = True

print 
print ser
print


#command = ":SYST:BEEP500\n"
#command = "*RST\n"
command = ":SYST:COMM:SER:BAUD?\n"
#command = "\r ----- \n"
print "-----------"
print command.encode('ascii')
print "-----------"
ser.open()
#print ser.isOpen()
#print ser.read(10)
#print ser.readline()
test = ser.write(command.encode('ascii'))
print test
print "-----------"
for x in range(10):
    ser.write(command.encode('ascii'))
    print ser.readline()
print "-----------"
#ser.write(":SYST:BEEP 500")      # write a string
ser.close()             # close port
#print ser.isOpen()
