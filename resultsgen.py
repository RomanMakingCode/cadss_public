import subprocess
import re
import numpy as np
#import pandas as pd




total = np.zeros((4,5))









for i1 in range(4):
    file1 = open("cohotest.config", "w+")

    L1 = "__processor  -f 2 -d 1 -m 2 -j 2 -k 1 -c 2\n"
    L2 = "__cache -E 1 -b 4 -s 8 \n"
    L3 = "__branch -s 7 -b 2 -g 1\n"
    L4 = "__coherence -s " + str(i1) + "\n"
    L5 = "__interconnect \n"
    L6 = "__memory \n"


    file1.writelines(L1)
    file1.writelines(L2)
    file1.writelines(L3)
    file1.writelines(L4)
    file1.writelines(L5)
    file1.writelines(L6)
    file1.close()

    print("\nproc_mig ref: \n")
    output = subprocess.check_output("/usr/bin/time ./cadss-engine -s ex_coher.config -n 4 -p refProcessor/ -c simpleCache/ -o refCoherence/ -t /afs/cs.cmu.edu/academic/class/15346-f23/public/traces/coher/4proc_migratory/", shell = True)
    output = re.findall(r'\d+', str(output))
    print(output)
    total[i1, 0] = output[0]

    print("\nproc_prod ref: \n")
    output = subprocess.check_output("/usr/bin/time ./cadss-engine -s ex_coher.config -n 4 -p refProcessor/ -c simpleCache/ -o refCoherence/ -t /afs/cs.cmu.edu/academic/class/15346-f23/public/traces/coher/4proc_prodcons/", shell = True)
    output = re.findall(r'\d+', str(output))
    print(output)
    total[i1, 1] = output[0]


    print("\nblack ref: \n")
    output = subprocess.check_output("/usr/bin/time ./cadss-engine -s ex_coher.config -n 4 -p refProcessor/ -c simpleCache/ -o refCoherence/ -t /afs/cs.cmu.edu/academic/class/15346-f23/public/traces/coher/blackscholes_4_simsmall.taskgraph", shell = True)
    output = re.findall(r'\d+', str(output))
    print(output)
    total[i1, 2] = output[0]

    print("\n dedup_4 ref: \n")
    output = subprocess.check_output("/usr/bin/time ./cadss-engine -s ex_coher.config -n 4 -p refProcessor/ -c simpleCache/ -o refCoherence/ -t /afs/cs.cmu.edu/academic/class/15346-f23/public/traces/coher/dedup_4_simsmall.taskgraph", shell = True)
    output = re.findall(r'\d+', str(output))
    print(output)
    total[i1, 3] = output[0]

    print("\n dedup_4 ref: \n")
    output = subprocess.check_output("/usr/bin/time ./cadss-engine -s ex_coher.config -n 4 -p refProcessor/ -c simpleCache/ -o refCoherence/ -t /afs/cs.cmu.edu/academic/class/15346-f23/public/traces/coher/dedup_8_simsmall.taskgraph", shell = True)
    output = re.findall(r'\d+', str(output))
    print(output)
    total[i1, 4] = output[0]



print(total)



#        arr[i,j] = output[0]
#        print("out + " + str(arr[i,j]))

#print(arr)

#write to csv file
np.savetxt('totaldoc.csv', total, fmt='%1.3f', delimiter=',')

#prints out data in format that I can copy and paste into mathematica to create graphs
#for i in range (rows):
#    for j in range(cols):
#        if(arr[i,j] != 0):
#            print("{" + str(i) + "," + str(j) + "," + str(arr[i,j]) + "},")
