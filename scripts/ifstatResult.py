import sys
import os

''' Parse a file to get goodput results in Mbps'''
def parse_file(file_name):
    results = []
    f = open(file_name)
    while True:
        line = f.readline().rstrip()
        if not line:
            break
        if line.find("Time") != -1 or line.find("Kbps") != -1:
            continue
        arr = line.split()
        #print 'arr[0] :', arr[0]
        '''kbps in, kbps out'''        
        if len(arr) == 2:            
            results.append(int(float(arr[0]))/1000)    #Kbps to Mbps
    f.close()
    return results
''' Save goodput in file with specified time interval'''
def save_results(file_name, inputarr, timeval):
    tv = max(1, int(timeval))
    results = []   
    group_count = 0
    average_kbps = 0
    for index in range(len(inputarr)):
        group_count += 1
        if group_count < tv:
            average_kbps += inputarr[index]            
        else: # group_count == tv
            average_kbps += inputarr[index]
            results.append(int(average_kbps/tv))
            average_kbps = 0
            group_count = 0

    f = open(file_name, "w")
    for kbps in results:
        f.write("%d\n" % kbps)
    f.close()

'''python ifstatResult.py <input_file_name> <output_file_name> <time_interval>'''
if __name__ == '__main__':
    final_results = []
    input_file = sys.argv[1]
    output_file = sys.argv[2]
    timeval = sys.argv[3]
    if os.path.isfile(input_file):
        final_results.extend(parse_file(input_file))
        #print 'final_result len:', len(final_results)
        save_results(output_file, final_results, timeval)
