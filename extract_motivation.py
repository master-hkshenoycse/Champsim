
import pandas as pd
import os
log_location="/mnt/c/MTP_ORGINAL/logs"

no_of_access_per_block=[]
no_of_blocks=[]
workload_array=[]

#epoch_array=[]


folder_loc=['l2c_motivation']


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
            print(line_array)

            if len(line_array)>0 and line_array==['Frequency', 'of', 'blocks', 'accesed']:
                
                print(i)
                """
                for j in range(i,i+64):
                    print(j,Lines[j])
                """
                
                for j in range(i+1,i+22):
                    workload_array.append(workload_name)
                    no_of_access_per_block.append(Lines[j].split(' ')[0])
                    no_of_blocks.append(Lines[j].split(' ')[1].replace('\n',''))   
                    
                break

                
                    




print(len(workload_array))

df=pd.DataFrame({'Workload':workload_array,
                'No of Access Per block':no_of_access_per_block,
                'No of blocks':no_of_blocks                           
                 })


print(df)

df.to_csv(log_location+'_l2c_victims_motivation.csv')



