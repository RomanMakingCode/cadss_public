import subprocess
import numpy as np
#import pandas as pd
import re

mode = 1

rows = 1
cols = 4

arr = np.zeros((rows,cols))
arr2 = np.zeros((rows,cols))

#for i in range(rows):
#    for j in range(cols):
#        #writing to a file
#        print("s (row) = " + str(i) + " b (col) = " + str(j))
file1 = open("branchtest.config", "w+")

L1 = "__processor  -f 2 -d 2 -m 2 -j 2 -k 2 -c 2\n"
L2 = "__cache -E 1 -b 4 -s 8 \n"
L3 = "__branch -s 7 -b 2 -g 1\n"
L4 = "__coherence \n"
L5 = "__interconnect \n"
L6 = "__memory \n"


file1.writelines(L1)
file1.writelines(L2)
file1.writelines(L3)
file1.writelines(L4)
file1.writelines(L5)
file1.writelines(L6)
file1.close()

print("\ncadss.trace ref: \n")
output = subprocess.check_output("/usr/bin/time ./cadss-engine -s branchtest.config -p refProcessor/ -t /afs/cs.cmu.edu/academic/class/15346-f23/public/traces/processor/cadss.trace", shell = True)
output = re.findall(r'\d+', str(output))
print(output)
arr[0,0] = output[0]

print("\nfluid.trace ref: \n")
output = subprocess.check_output("/usr/bin/time ./cadss-engine -s branchtest.config -p refProcessor/ -t /afs/cs.cmu.edu/academic/class/15346-f23/public/traces/processor/fluid.trace", shell = True)
output = re.findall(r'\d+', str(output))
print(output)
arr[0,1] = output[0]

print("\nblack.trace ref: \n")
output = subprocess.check_output("/usr/bin/time ./cadss-engine -s branchtest.config -p refProcessor/ -t /afs/cs.cmu.edu/academic/class/15346-f23/public/traces/processor/black.trace", shell = True)
output = re.findall(r'\d+', str(output))
print(output)
arr[0,2] = output[0]

print("\nls.trace ref: \n")
output = subprocess.check_output("/usr/bin/time ./cadss-engine -s branchtest.config -p refProcessor/ -t /afs/cs.cmu.edu/academic/class/15346-f23/public/traces/processor/ls.trace", shell = True)
output = re.findall(r'\d+', str(output))
print(output)
arr[0,3] = output[0]

print("\ncadss.trace processor: \n")
output = subprocess.check_output("/usr/bin/time ./cadss-engine -s branchtest.config -p processor/ -t /afs/cs.cmu.edu/academic/class/15346-f23/public/traces/processor/cadss.trace", shell = True)
output = re.findall(r'\d+', str(output))
print(output)
arr2[0,0] = output[0]

print("\nfluid.trace processor: \n")
output = subprocess.check_output("/usr/bin/time ./cadss-engine -s branchtest.config -p processor/ -t /afs/cs.cmu.edu/academic/class/15346-f23/public/traces/processor/fluid.trace", shell = True)
output = re.findall(r'\d+', str(output))
print(output)
arr2[0,1] = output[0]

print("\nblack.trace processor: \n")
output = subprocess.check_output("/usr/bin/time ./cadss-engine -s branchtest.config -p processor/ -t /afs/cs.cmu.edu/academic/class/15346-f23/public/traces/processor/black.trace", shell = True)
output = re.findall(r'\d+', str(output))
print(output)
arr2[0,2] = output[0]

print("\nls.trace processor: \n")
output = subprocess.check_output("/usr/bin/time ./cadss-engine -s branchtest.config -p processor/ -t /afs/cs.cmu.edu/academic/class/15346-f23/public/traces/processor/ls.trace", shell = True)
output = re.findall(r'\d+', str(output))
print(output)
arr2[0,3] = output[0]
    
print("\nRef\n")
print(arr)
print("\nproc\n")
print(arr2)


#        arr[i,j] = output[0]
#        print("out + " + str(arr[i,j]))

#print(arr)

#write to csv file
#np.savetxt('myoutcaddsref.csv', arr, fmt="%d", delimiter=',')

#prints out data in format that I can copy and paste into mathematica to create graphs
#for i in range (rows):
#    for j in range(cols):
#        if(arr[i,j] != 0):
#            print("{" + str(i) + "," + str(j) + "," + str(arr[i,j]) + "},")
