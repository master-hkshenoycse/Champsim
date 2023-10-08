
import pandas as pd
import os
log_location="/mnt/c/Users/hkshe/MTP/ChampSim_v3/logs"
no_of_inst=50000000


replacement_array=[]

workload_array=[]
ipc_array=[]

llc_load_miss=[]
llc_load_hit=[]
llc_miss_latency=[]
llc_load_acceess=[]
llc_load_mpki=[]


l2c_load_miss=[]
l2c_load_hit=[]
l2c_miss_latency=[]
l2c_load_acceess=[]
l2c_load_mpki=[]

l1d_load_miss=[]
l1d_load_hit=[]
l1d_miss_latency=[]
l1d_load_acceess=[]
l1d_load_mpki=[]


folder_loc=['lru','drrip','dummy_hits_15_30','dummy_hits_20_30','dummy_hits_topk','dummy_hits_topk_epoch','ship','srrip']


for folders in folder_loc:
    
    replacement_policy=folders
    folder_path=log_location+'/'+folders+'/'

    

    for files in os.listdir(folder_path):
        
        replacement_array.append(replacement_policy)
    
        workload_name=files.split("_")[2]
        workload_array.append(workload_name)
        
        file1 = open(folder_path+files, 'r')
        Lines = file1.readlines()
        print(Lines)
        for line in Lines:
            
            line_array=line.replace(':',' ')
            line_array=line.strip()
            line_array=line_array.split()
            print(line_array)
            if len(line_array)>=10 and line_array[0]=='Finished' and line_array[8]=='IPC:':
                ipc_array.append(float(line_array[9]))


            if len(line_array)>=8 and line_array[0]=='LLC' and line_array[1]=='LOAD':
                llc_load_acceess.append(float(line_array[3]))
                llc_load_hit.append(float(line_array[5]))
                llc_load_miss.append(float(line_array[7]))
                llc_load_mpki.append(float(line_array[7])*1000.00/no_of_inst)

            if len(line_array)==6 and line_array[0]=='LLC' and line_array[1]=='AVERAGE' and line_array[3]=='LATENCY:':
                llc_miss_latency.append(line_array[4])

            if len(line_array)>=8 and line_array[0]=='cpu0_L2C' and line_array[1]=='LOAD':
                l2c_load_acceess.append(float(line_array[3]))
                l2c_load_hit.append(float(line_array[5]))
                l2c_load_miss.append(float(line_array[7]))
                l2c_load_mpki.append(float(line_array[7])*1000.00/no_of_inst)

            if len(line_array)==6 and line_array[0]=='cpu0_L2C' and line_array[1]=='AVERAGE' and line_array[3]=='LATENCY:':
                l2c_miss_latency.append(line_array[4])


            if len(line_array)>=8 and line_array[0]=='cpu0_L1D' and line_array[1]=='LOAD':
                l1d_load_acceess.append(float(line_array[3]))
                l1d_load_hit.append(float(line_array[5]))
                l1d_load_miss.append(float(line_array[7]))
                l1d_load_mpki.append(float(line_array[7])*1000.00/no_of_inst)

            if len(line_array)==6 and line_array[0]=='cpu0_L1D' and line_array[1]=='AVERAGE' and line_array[3]=='LATENCY:':
                l1d_miss_latency.append(line_array[4])
                                


df=pd.DataFrame({'Workload':workload_array,
                'ReplacementPolicy':replacement_array,
                'IPC':ipc_array,
                'LLC_Load_Access':llc_load_acceess,
                'LLC_Load_Hit':llc_load_hit,
                'LLC_Load_Miss':llc_load_miss,
                'LLC_Load_MPKI':llc_load_mpki,
                'LLC_Miss_Latency':llc_miss_latency,
                'L2C_Load_Access':l2c_load_acceess,
                'L2C_Load_Hit':l2c_load_hit,
                'L2C_Load_Miss':l2c_load_miss,
                'L2C_Load_MPKI':l2c_load_mpki,
                'L2C_Miss_Latency':l2c_miss_latency,
                'L1D_Load_Access':l1d_load_acceess,
                'L1D_Load_Hit':l1d_load_hit,
                'L1D_Load_Miss':l1d_load_miss,
                'L1D_Load_MPKI':l1d_load_mpki,
                'L1D_Miss_Latency':l1d_miss_latency                                 
                 })
df.to_csv(log_location+'_collated_results.csv')



