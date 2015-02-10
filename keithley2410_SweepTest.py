#!/usr/bin/env python
import serial
import sys
#import matplotlib.pyplot as plt

script = """
# hashes make comments
# @ on a read specifies the parser for the read data

*RST
:SYST:BEEP1000
#:SENS:FUNC:CONC OFF    #Turn off concurrent functions
#:SOUR:FUNC VOLT        #Set supply to voltage
#:SENS:FUNC 'CURR:DC'   #Set measurement to DC current
#:SENS:CURR:PROT 20E-3  #Set compliance current
#:SOUR:VOLT:START 0     #Set Start Voltage
#:SOUR:VOLT:STOP 10     #Set Stop Voltage
#:SOUR:VOLT:STEP 1      #Set Voltage Step
#:SOUR:VOLT:MODE SWE    #Enable sweep mode
#:SOUR:SWE:RANG AUTO    #Set range to auto
#:SOUR:SWE:SPAC LIN     #Use linear sweep
#:TRIG:COUN 10          #Number of measurements(should be compatable with START|STOP|STEP)
#:SOUR:DEL 0.1          #Delay between Voltage set and current measure
#:FORM:ELEM VOLT,CURR   #Format of output
#:OUTP ON               #Enable HV
#:READ? @IV             #Read back measurements
#:OUTP OFF              #Turn off output
         """

def process_script():
    print "processing script"
    global script
    script = script.splitlines()
    script = [line.split('#')[0].strip() for line in script]
    script = [line for line in script if len(line) > 0]
    for i, line in enumerate(script):
        line = line.split('@')
        if len(line) == 1:
            script[i] = (line[0]+'\n',None)
        else:
            script[i] = (line[0].strip()+'\n',line[1])
    print "processed script"

def read(ser):
    data = bytes()
    endl = '\n'.encode('ascii')[0]
    while True:
        data += ser.read(1)
        if data[-1] == endl:
            break
    return data.decode('ascii')

def parse(parser, result):
    if parser == "IV":
        result = result.split(',')
        current = []
        voltage = []
        for i, item in enumerate(result):
            if not i % 2:
                voltage.append(float(item))
            else:
                current.append(float(item))
        for v, c in zip(voltage, current):
            print("{0:+10.5e}  {1:+10.5e}".format(v,c))
#        plt.xlabel('Voltage(Volts)')
#        plt.ylabel('Current(Amps)')
#        plt.plot(voltage,current,'.')
    else:
        raise NotImplementedError("This parser is not implemented: {0}".format(parser))

def main():
    print "starting"
    global script
    process_script()
    with serial.Serial('/dev/tty.usbserial', timeout = 0, writeTimeout = 0, baudrate = 57600, parity = serial.PARITY_ODD, xonxoff = True) as ser:
        print "port open"
        print ser
        for command, parser in script:
#            print('==>'+command,end='')
            print '==>'+command
            ser.write(command.encode('ascii'))
            if parser != None:
                parse(parser, read(ser))
                ser.flushOutput()
            parse(parser, read(ser))
            ser.flushOutput()
            sys.exit()

#    plt.show()

if __name__ == '__main__':
    main();
