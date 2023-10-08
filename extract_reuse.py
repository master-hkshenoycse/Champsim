
import pandas as pd
import os
log_location="/mnt/c/Users/hkshe/MTP/ChampSim_v3/logs"


segment_array=[]
avg_reuse_block_array=[]
no_of_access_array=[]
no_of_access_block=[]
workload_array=[]

#epoch_array=[]








folder_loc=['check']
exp_name=['NO_EPOCH']

curr=0

def get_epoch(s):
    l=s.split(' ')
    print(l)
    if(l[2]=="Epoch"):
        l[4]=l[4].replace('\n','')
        return l[4]

    return "NO_EPOCH"




for folders in folder_loc:
    
    folder_path=log_location+'/'+folders+'/'

   

    for files in os.listdir(folder_path):
        
        
        file1 = open(folder_path+files, 'r')
        Lines = file1.readlines()
        #print(Lines)
        l=len(Lines)
        workload_name=files.split("_")[2]
        
        for i in range(0,l):
            
            line=Lines[i]
            line_array=line.replace(':',' ')
            line_array=line.strip()
            line_array=line_array.split()
            #print(line_array)

            if len(line_array)>0 and line_array[0]=="Reuse_distance_Access":
                
                for j in range(i,i+64):
                    print(j,Lines[j])
                
                for j in range(i+1,i+21):
                    workload_array.append(workload_name)
                    segment_array.append(Lines[j].split(' ')[0])
                    no_of_access_array.append(Lines[j].split(' ')[1].replace('\n',''))   
                    
                for j in range(i+22,i+42):
                    print(Lines[j].split(' '))
                    avg_reuse_block_array.append(Lines[j].split(' ')[1].replace('\n',''))                    
                    

                for j in range(i+43,i+63): 
                    print(Lines[j].split(' '))
                    no_of_access_block.append(Lines[j].split(' ')[1].replace('\n',''))
                    




print(len(workload_array))
print(len(segment_array))

df=pd.DataFrame({'Workload':workload_array,
                'Reuse Segment':segment_array,
                'No of Access with Reuse segment':no_of_access_array,
                'No of blocks in Avg Reuse Segment':avg_reuse_block_array,
                'No of access with access':no_of_access_block                            
                 })


print(df)

df.to_csv(log_location+'_collated_results_for_reuse.csv')



