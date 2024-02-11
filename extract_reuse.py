
import pandas as pd
import os
log_location="/mnt/c/MTP_ORGINAL/logs"


segment_array=[]
avg_reuse_block_array=[]
hierarchy_array=[]
no_of_access_array=[]
no_of_access_block=[]
l2c_avg_reuse=[]
l2c_access_reuse=[]
workload_array=[]

#epoch_array=[]


folder_loc=['check']


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
                
                print(i)
                """
                for j in range(i,i+64):
                    print(j,Lines[j])
                """
                
                for j in range(i+1,i+21):
                    hierarchy_array.append(folders)
                    workload_array.append(workload_name)
                    segment_array.append(Lines[j].split(' ')[0])
                    no_of_access_array.append(Lines[j].split(' ')[1].replace('\n',''))   
                    
                for j in range(i+22,i+42):
                    print(Lines[j].split(' '))
                    avg_reuse_block_array.append(Lines[j].split(' ')[1].replace('\n',''))                    


                for j in range(i+43,i+63): 
                    print(Lines[j].split(' '))
                    l2c_avg_reuse.append(Lines[j].split(' ')[1].replace('\n',''))

                for j in range(i+65,i+85): 
                    print(Lines[j].split(' '))
                    l2c_access_reuse.append(Lines[j].split(' ')[1].replace('\n',''))
                
                break

                
                    




print(len(workload_array))
print(len(segment_array))

df=pd.DataFrame({'Workload':workload_array,
                'Reuse Segment':segment_array,
                'Hierarchy':hierarchy_array,
                'No of Access with Reuse segment':no_of_access_array,
                'No of blocks in Avg Reuse Segment':avg_reuse_block_array,
                'No of blocks in Avg Reuse Segment L2C':l2c_avg_reuse,
                'No of Access in Reuse Segment L2C':l2c_access_reuse                           
                 })


print(df)

df.to_csv(log_location+'_llc_victims_for_reuse.csv')



