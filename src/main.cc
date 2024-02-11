/*
 *    Copyright 2023 The ChampSim Contributors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <algorithm>
#include <array>
#include <fstream>
#include <functional>
#include <getopt.h>
#include <iomanip>
#include <signal.h>
#include <string.h>
#include <vector>
#include <set>
#include <utility>
#include "cache.h"
#include "champsim.h"
#include "champsim_constants.h"
#include "dram_controller.h"
#include "ooo_cpu.h"
#include "operable.h"
#include "tracereader.h"
#include "vmem.h"


#define NO_OF_SETS 2048
#define PRINT_REUSE_STATS 0
#define EPOCH_REFRESH 0
#define PRINT_L2C_motivation 1
#define predictor_sets 256


map<uint64_t,int> block_reuse_counter,reuse_distance_counter,block_last_access_counter,block_access_counter;
map<uint64_t,int> l2c_block_access;
vector<pair<uint64_t,int> > set_access[NO_OF_SETS];
int LLC_access_counter=0;

std::vector< vector<uint64_t> > l2c_set_access[1024];
int L2C_access_counter=0;
map<uint64_t,int> llc_victim_counter;
map<uint64_t,int> llc_victim_l2c_reuse,llc_victim_l2c_reuse_access;
map<uint64_t,int> llc_victim_l2c_cnt;
map<int,uint64_t> l2c_event_counter;

int predictor_hits=0,predictor_access=0,actual_reuse=0,predicted_reuse=0;
deque<pair<int,int> > counters[predictor_sets];

uint8_t warmup_complete[NUM_CPUS] = {}, simulation_complete[NUM_CPUS] = {}, all_warmup_complete = 0, all_simulation_complete = 0,
        MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS, knob_cloudsuite = 0, knob_low_bandwidth = 0;

uint64_t warmup_instructions = 1000000, simulation_instructions = 10000000;

auto start_time = time(NULL);



// For backwards compatibility with older module source.
champsim::deprecated_clock_cycle current_core_cycle;

extern MEMORY_CONTROLLER DRAM;
extern VirtualMemory vmem;
extern std::array<O3_CPU*, NUM_CPUS> ooo_cpu;
extern std::array<CACHE*, NUM_CACHES> caches;
extern std::array<champsim::operable*, NUM_OPERABLES> operables;

std::vector<tracereader*> traces;

uint64_t champsim::deprecated_clock_cycle::operator[](std::size_t cpu_idx)
{
  static bool deprecate_printed = false;
  if (!deprecate_printed) {
    std::cout << "WARNING: The use of 'current_core_cycle[cpu]' is deprecated." << std::endl;
    std::cout << "WARNING: Use 'this->current_cycle' instead." << std::endl;
    deprecate_printed = true;
  }
  return ooo_cpu[cpu_idx]->current_cycle;
}

void print_l2_motivation(){

  /*cout<<"Total blocks Accessed: "<<l2c_block_access.size()<<endl;

  
  uint64_t non_reused_blocks=0;
  uint64_t total_access=0;
  uint64_t access_reuse_blocks=0;
  uint64_t access_nonreuse_blocks=0;

  vector<uint64_t> access_gt(25,0);
  for(auto it:l2c_block_access){
    total_access=total_access+it.second;
    if(it.second==1){
      access_nonreuse_blocks=access_nonreuse_blocks+1;
    }else{
      access_reuse_blocks=access_reuse_blocks+it.second;
    }  

    if(it.second<=20){
      access_gt[it.second-1]++;
    }else{
      access_gt[20]++;
    }
  }

  cout<<"Check Here For Motivation :"<<total_access<<" "<<access_nonreuse_blocks<<" "<<access_reuse_blocks<<endl;

  cout<<"Frequency of blocks accesed"<<endl;
  for(int i=0;i<20;i++){
    cout<<i<<" "<<access_gt[i]<<endl;
  }
  cout<<">20"<<" "<<access_gt[20]<<endl;*/

  cout<<"Reuse Predictor stats :"<<endl;
  cout<<"Total Predictor Access :"<<predictor_access<<endl;
  cout<<"Total Predictor Hits :"<<predictor_hits<<endl;
  cout<<"Actual Reuse:"<<actual_reuse<<endl;
  cout<<"Correctly predicted reuse: "<<predicted_reuse<<endl;
  double hit_rate=0.0,recall_rate=0.0;

  if(predictor_access>0){
    hit_rate=(predictor_hits*1.00)/(predictor_access*1.00);
  }

  if(actual_reuse>0){
    recall_rate=(predicted_reuse*1.00)/(actual_reuse*1.00);
  }

  cout<<"Hit rate : "<<hit_rate<<" Recall: "<<recall_rate<<endl;


}

void print_reuse_stats(){

   //map<uint64_t,vector<int> > read_times,evict_times;

  for(int i=0;i<1024;i++){
    //cout<<"For set "<<i<<endl;
    int sz=l2c_set_access[i].size();
    //cout<<l2c_set_access[i].size()<<endl;
    sort(l2c_set_access[i].begin(),l2c_set_access[i].end());
    map<uint64_t,int> last_evicted_llc;
   

    /*for(int j=0;j<sz;j++){
      auto it=l2c_set_access[i][j];
      if(it[2]==0){
        read_times[it[1]].push_back(it[0]);
      }else{
        evict_times[it[1]].push_back(it[0]);
        llc_victim_counter[it[1]]++;
      }
    }*/
    for(int j=0;j<sz;j++){
      auto it=l2c_set_access[i][j];
      if(it[2]==0){
        
        
        if(last_evicted_llc.find(it[1]) != last_evicted_llc.end()){
          
          set<uint64_t> reuse_set;
          int prev_counter=last_evicted_llc[j];
          for(int k=prev_counter+1;k<j;k++){
            if(l2c_set_access[i][k][2]==0){//only read events in l2c
              reuse_set.insert(l2c_set_access[i][k][1]);
            }
          }

          
          int reuse_distance=reuse_set.size();
          //cout<<it[1]<<" "<<prev_counter<<" "<<reuse_distance<<endl;
          llc_victim_l2c_reuse[it[1]]+=reuse_distance;
          llc_victim_l2c_reuse_access[reuse_distance]++;
          llc_victim_l2c_cnt[it[1]]++;
          last_evicted_llc.erase(it[1]);
        }
        
        }else{
          last_evicted_llc[it[1]]=j;
          llc_victim_counter[it[1]]++;
        }

    
    }


  }
  

  int dead_block_cnt=0;

  for(auto it:block_access_counter){
    if(it.second==1){
      dead_block_cnt++;
    }
  }

  cout<<"LLC Load Access : "<<LLC_access_counter<<endl;
  cout<<"Dead Block count: "<<dead_block_cnt<<endl;
  cout<<"Unique Blocks Accessed: "<<block_access_counter.size()<<endl;

  vector<int> cnt(25,0);
  vector<string> labels={"01_0","02_1","03_2","04_3-4","05_5-6","06_7-8","07_9-10","08_11-12","09_13-15","10_16-20",
                         "11_21-30","12_31-40","13_41-50","14_51-60","15_61-70","16_71-80","17_81-100","18_101-150","19_151-200","20_>200"};

  for(auto it:reuse_distance_counter){
      if(it.first==0)cnt[0]+=it.second;
      else if(it.first<=1)cnt[1]+=it.second;
      else if(it.first<=2)cnt[2]+=it.second;
      else if(it.first<=4)cnt[3]+=it.second;
      else if(it.first<=6)cnt[4]+=it.second;
      else if(it.first<=8)cnt[5]+=it.second;
      else if(it.first<=10)cnt[6]+=it.second;
      else if(it.first<=12)cnt[7]+=it.second;
      else if(it.first<=15)cnt[8]+=it.second;
      else if(it.first<=20)cnt[9]+=it.second;
      else if(it.first<=30)cnt[10]+=it.second;
      else if(it.first<=40)cnt[11]+=it.second;
      else if(it.first<=50)cnt[12]+=it.second;
      else if(it.first<=60)cnt[13]+=it.second;
      else if(it.first<=70)cnt[14]+=it.second;
      else if(it.first<=80)cnt[15]+=it.second;
      else if(it.first<=100)cnt[16]+=it.second;
      else if(it.first<=150)cnt[17]+=it.second;
      else if(it.first<=200)cnt[18]+=it.second;
      else cnt[19]+=it.second;
  }

  cout<<"Reuse_distance_Access"<<endl;
  for(int i=0;i<20;i++){
    cout<<labels[i]<<" "<<cnt[i]<<endl;
    cnt[i]=0;
  }

  for(auto it:block_reuse_counter){

    

    int avg_reuse=(it.second)/(block_access_counter[it.first]-1);
    //cout<<avg_reuse<<endl;
    
    if(avg_reuse==0)cnt[0]++;
    else if(avg_reuse<=1)cnt[1]++;
    else if(avg_reuse<=2)cnt[2]++;
    else if(avg_reuse<=4)cnt[3]++;
    else if(avg_reuse<=6)cnt[4]++;
    else if(avg_reuse<=8)cnt[5]++;
    else if(avg_reuse<=10)cnt[6]++;
    else if(avg_reuse<=12)cnt[7]++;
    else if(avg_reuse<=15)cnt[8]++;
    else if(avg_reuse<=20)cnt[9]++;
    else if(avg_reuse<=30)cnt[10]++;
    else if(avg_reuse<=40)cnt[11]++;
    else if(avg_reuse<=50)cnt[12]++;
    else if(avg_reuse<=60)cnt[13]++;
    else if(avg_reuse<=70)cnt[14]++;
    else if(avg_reuse<=80)cnt[15]++;
    else if(avg_reuse<=100)cnt[16]++;
    else if(avg_reuse<=150)cnt[17]++;
    else if(avg_reuse<=200)cnt[18]++;
    else cnt[19]++;

  }

  cout<<"Avg_block_Reuse_Access"<<endl;
  for(int i=0;i<20;i++){
    cout<<labels[i]<<" "<<cnt[i]<<endl;
    cnt[i]=0;
  }


  //Reuse distance for LLC victims in L2C
  for(auto it:llc_victim_l2c_reuse){

    

    int avg_reuse=(it.second)/llc_victim_l2c_cnt[it.first];
    //cout<<avg_reuse<<endl;
    
    if(avg_reuse==0)cnt[0]++;
    else if(avg_reuse<=1)cnt[1]++;
    else if(avg_reuse<=2)cnt[2]++;
    else if(avg_reuse<=4)cnt[3]++;
    else if(avg_reuse<=6)cnt[4]++;
    else if(avg_reuse<=8)cnt[5]++;
    else if(avg_reuse<=10)cnt[6]++;
    else if(avg_reuse<=12)cnt[7]++;
    else if(avg_reuse<=15)cnt[8]++;
    else if(avg_reuse<=20)cnt[9]++;
    else if(avg_reuse<=30)cnt[10]++;
    else if(avg_reuse<=40)cnt[11]++;
    else if(avg_reuse<=50)cnt[12]++;
    else if(avg_reuse<=60)cnt[13]++;
    else if(avg_reuse<=70)cnt[14]++;
    else if(avg_reuse<=80)cnt[15]++;
    else if(avg_reuse<=100)cnt[16]++;
    else if(avg_reuse<=150)cnt[17]++;
    else if(avg_reuse<=200)cnt[18]++;
    else cnt[19]++;

  }

  cout<<"Avg_block_Reuse_Access_for_LLC_victims_in_L2C"<<endl;
  for(int i=0;i<20;i++){
    cout<<labels[i]<<" "<<cnt[i]<<endl;
    cnt[i]=0;
  }

  cout<<"Access_Reuse_for_LLC_victim_Reuse_in_L2C"<<endl;

   for(auto it:llc_victim_l2c_reuse_access){
      if(it.first==0)cnt[0]+=it.second;
      else if(it.first<=1)cnt[1]+=it.second;
      else if(it.first<=2)cnt[2]+=it.second;
      else if(it.first<=4)cnt[3]+=it.second;
      else if(it.first<=6)cnt[4]+=it.second;
      else if(it.first<=8)cnt[5]+=it.second;
      else if(it.first<=10)cnt[6]+=it.second;
      else if(it.first<=12)cnt[7]+=it.second;
      else if(it.first<=15)cnt[8]+=it.second;
      else if(it.first<=20)cnt[9]+=it.second;
      else if(it.first<=30)cnt[10]+=it.second;
      else if(it.first<=40)cnt[11]+=it.second;
      else if(it.first<=50)cnt[12]+=it.second;
      else if(it.first<=60)cnt[13]+=it.second;
      else if(it.first<=70)cnt[14]+=it.second;
      else if(it.first<=80)cnt[15]+=it.second;
      else if(it.first<=100)cnt[16]+=it.second;
      else if(it.first<=150)cnt[17]+=it.second;
      else if(it.first<=200)cnt[18]+=it.second;
      else cnt[19]+=it.second;
  }

  cout<<"Reuse_distance_Access"<<endl;
  for(int i=0;i<20;i++){
    cout<<labels[i]<<" "<<cnt[i]<<endl;
    cnt[i]=0;
  }



  

  for(auto it:block_access_counter){
      if(it.second==0)cnt[0]++;
      else if(it.second<=1)cnt[1]++;
      else if(it.second<=2)cnt[2]++;
      else if(it.second<=4)cnt[3]++;
      else if(it.second<=6)cnt[4]++;
      else if(it.second<=8)cnt[5]++;
      else if(it.second<=10)cnt[6]++;
      else if(it.second<=12)cnt[7]++;
      else if(it.second<=15)cnt[8]++;
      else if(it.second<=20)cnt[9]++;
      else if(it.second<=30)cnt[10]++;
      else if(it.second<=40)cnt[11]++;
      else if(it.second<=50)cnt[12]++;
      else if(it.second<=60)cnt[13]++;
      else if(it.second<=70)cnt[14]++;
      else if(it.second<=80)cnt[15]++;
      else if(it.second<=100)cnt[16]++;
      else if(it.second<=150)cnt[17]++;
      else if(it.second<=200)cnt[18]++;
      else cnt[19]++;
  }

  cout<<"Access_per_block"<<endl;
  for(int i=0;i<20;i++){
    cout<<labels[i]<<" "<<cnt[i]<<endl;
    cnt[i]=0;
  }

  /*for(auto it:l2c_block_access){

    if(it.second>1){
      cout<<it.first<<" "<<it.second<<" "<<llc_victim_counter[it.first]<<endl;
      cout<<"Read times"<<endl;
      for(auto it:read_times[it.first]){
        cout<<it<<endl;
      }
      cout<<"Evict times"<<endl;
      for(auto it:evict_times[it.first]){
        cout<<it<<endl;
      }
    }

  }*/
}
void record_roi_stats(uint32_t cpu, CACHE* cache)
{
  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    cache->roi_access[cpu][i] = cache->sim_access[cpu][i];
    cache->roi_hit[cpu][i] = cache->sim_hit[cpu][i];
    cache->roi_miss[cpu][i] = cache->sim_miss[cpu][i];
  }
}

void print_roi_stats(uint32_t cpu, CACHE* cache)
{
  uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;

  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    TOTAL_ACCESS += cache->roi_access[cpu][i];
    TOTAL_HIT += cache->roi_hit[cpu][i];
    TOTAL_MISS += cache->roi_miss[cpu][i];
  }

  if (TOTAL_ACCESS > 0) {
    cout << cache->NAME;
    cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS << endl;

    cout << cache->NAME;
    cout << " LOAD      ACCESS: " << setw(10) << cache->roi_access[cpu][0] << "  HIT: " << setw(10) << cache->roi_hit[cpu][0] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][0] << endl;

    cout << cache->NAME;
    cout << " RFO       ACCESS: " << setw(10) << cache->roi_access[cpu][1] << "  HIT: " << setw(10) << cache->roi_hit[cpu][1] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][1] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  ACCESS: " << setw(10) << cache->roi_access[cpu][2] << "  HIT: " << setw(10) << cache->roi_hit[cpu][2] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][2] << endl;

    cout << cache->NAME;
    cout << " WRITEBACK ACCESS: " << setw(10) << cache->roi_access[cpu][3] << "  HIT: " << setw(10) << cache->roi_hit[cpu][3] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][3] << endl;

    cout << cache->NAME;
    cout << " TRANSLATION ACCESS: " << setw(10) << cache->roi_access[cpu][4] << "  HIT: " << setw(10) << cache->roi_hit[cpu][4] << "  MISS: " << setw(10)
         << cache->roi_miss[cpu][4] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  REQUESTED: " << setw(10) << cache->pf_requested << "  ISSUED: " << setw(10) << cache->pf_issued;
    cout << "  USEFUL: " << setw(10) << cache->pf_useful << "  USELESS: " << setw(10) << cache->pf_useless << endl;

    cout << cache->NAME;
    cout << " AVERAGE MISS LATENCY: " << (1.0 * (cache->total_miss_latency)) / TOTAL_MISS << " cycles" << endl;
    // cout << " AVERAGE MISS LATENCY: " <<
    // (cache->total_miss_latency)/TOTAL_MISS << " cycles " <<
    // cache->total_miss_latency << "/" << TOTAL_MISS<< endl;

    if(cache->NAME=="LLC"){

      if(PRINT_REUSE_STATS){
        print_reuse_stats();
      }

      if(PRINT_L2C_motivation){
        print_l2_motivation();
      }
      
    }
  }
}

void print_sim_stats(uint32_t cpu, CACHE* cache)
{
  uint64_t TOTAL_ACCESS = 0, TOTAL_HIT = 0, TOTAL_MISS = 0;

  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    TOTAL_ACCESS += cache->sim_access[cpu][i];
    TOTAL_HIT += cache->sim_hit[cpu][i];
    TOTAL_MISS += cache->sim_miss[cpu][i];
  }

  if (TOTAL_ACCESS > 0) {
    cout << cache->NAME;
    cout << " TOTAL     ACCESS: " << setw(10) << TOTAL_ACCESS << "  HIT: " << setw(10) << TOTAL_HIT << "  MISS: " << setw(10) << TOTAL_MISS << endl;

    cout << cache->NAME;
    cout << " LOAD      ACCESS: " << setw(10) << cache->sim_access[cpu][0] << "  HIT: " << setw(10) << cache->sim_hit[cpu][0] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][0] << endl;

    cout << cache->NAME;
    cout << " RFO       ACCESS: " << setw(10) << cache->sim_access[cpu][1] << "  HIT: " << setw(10) << cache->sim_hit[cpu][1] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][1] << endl;

    cout << cache->NAME;
    cout << " PREFETCH  ACCESS: " << setw(10) << cache->sim_access[cpu][2] << "  HIT: " << setw(10) << cache->sim_hit[cpu][2] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][2] << endl;

    cout << cache->NAME;
    cout << " WRITEBACK ACCESS: " << setw(10) << cache->sim_access[cpu][3] << "  HIT: " << setw(10) << cache->sim_hit[cpu][3] << "  MISS: " << setw(10)
         << cache->sim_miss[cpu][3] << endl;
  }
}

void print_branch_stats()
{
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    cout << endl << "CPU " << i << " Branch Prediction Accuracy: ";
    cout << (100.0 * (ooo_cpu[i]->num_branch - ooo_cpu[i]->branch_mispredictions)) / ooo_cpu[i]->num_branch;
    cout << "% MPKI: " << (1000.0 * ooo_cpu[i]->branch_mispredictions) / (ooo_cpu[i]->num_retired - warmup_instructions);
    cout << " Average ROB Occupancy at Mispredict: " << (1.0 * ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict) / ooo_cpu[i]->branch_mispredictions
         << endl;

    /*
    cout << "Branch types" << endl;
    cout << "NOT_BRANCH: " << ooo_cpu[i]->total_branch_types[0] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[0])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_DIRECT_JUMP: "
    << ooo_cpu[i]->total_branch_types[1] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[1])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_INDIRECT: " <<
    ooo_cpu[i]->total_branch_types[2] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[2])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_CONDITIONAL: "
    << ooo_cpu[i]->total_branch_types[3] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[3])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_DIRECT_CALL: "
    << ooo_cpu[i]->total_branch_types[4] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[4])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_INDIRECT_CALL:
    " << ooo_cpu[i]->total_branch_types[5] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[5])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_RETURN: " <<
    ooo_cpu[i]->total_branch_types[6] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[6])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl; cout << "BRANCH_OTHER: " <<
    ooo_cpu[i]->total_branch_types[7] << " " <<
    (100.0*ooo_cpu[i]->total_branch_types[7])/(ooo_cpu[i]->num_retired -
    ooo_cpu[i]->begin_sim_instr) << "%" << endl << endl;
    */

    cout << "Branch type MPKI" << endl;
    cout << "BRANCH_DIRECT_JUMP: " << (1000.0 * ooo_cpu[i]->branch_type_misses[1] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_INDIRECT: " << (1000.0 * ooo_cpu[i]->branch_type_misses[2] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_CONDITIONAL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[3] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_DIRECT_CALL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[4] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_INDIRECT_CALL: " << (1000.0 * ooo_cpu[i]->branch_type_misses[5] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl;
    cout << "BRANCH_RETURN: " << (1000.0 * ooo_cpu[i]->branch_type_misses[6] / (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) << endl << endl;
  }
}

void print_dram_stats()
{
  uint64_t total_congested_cycle = 0;
  uint64_t total_congested_count = 0;

  std::cout << std::endl;
  std::cout << "DRAM Statistics" << std::endl;
  for (uint32_t i = 0; i < DRAM_CHANNELS; i++) {
    std::cout << " CHANNEL " << i << std::endl;

    auto& channel = DRAM.channels[i];
    std::cout << " RQ ROW_BUFFER_HIT: " << std::setw(10) << channel.RQ_ROW_BUFFER_HIT << " ";
    std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.RQ_ROW_BUFFER_MISS;
    std::cout << std::endl;

    std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
    if (channel.dbus_count_congested)
      std::cout << std::setw(10) << ((double)channel.dbus_cycle_congested / channel.dbus_count_congested);
    else
      std::cout << "-";
    std::cout << std::endl;

    std::cout << " WQ ROW_BUFFER_HIT: " << std::setw(10) << channel.WQ_ROW_BUFFER_HIT << " ";
    std::cout << " ROW_BUFFER_MISS: " << std::setw(10) << channel.WQ_ROW_BUFFER_MISS << " ";
    std::cout << " FULL: " << std::setw(10) << channel.WQ_FULL;
    std::cout << std::endl;

    std::cout << std::endl;

    total_congested_cycle += channel.dbus_cycle_congested;
    total_congested_count += channel.dbus_count_congested;
  }

  if (DRAM_CHANNELS > 1) {
    std::cout << " DBUS AVG_CONGESTED_CYCLE: ";
    if (total_congested_count)
      std::cout << std::setw(10) << ((double)total_congested_cycle / total_congested_count);
    else
      std::cout << "-";

    std::cout << std::endl;
  }
}

void reset_cache_stats(uint32_t cpu, CACHE* cache)
{
  for (uint32_t i = 0; i < NUM_TYPES; i++) {
    cache->sim_access[cpu][i] = 0;
    cache->sim_hit[cpu][i] = 0;
    cache->sim_miss[cpu][i] = 0;
  }

  cache->pf_requested = 0;
  cache->pf_issued = 0;
  cache->pf_useful = 0;
  cache->pf_useless = 0;
  cache->pf_fill = 0;

  cache->total_miss_latency = 0;

  cache->RQ_ACCESS = 0;
  cache->RQ_MERGED = 0;
  cache->RQ_TO_CACHE = 0;

  cache->WQ_ACCESS = 0;
  cache->WQ_MERGED = 0;
  cache->WQ_TO_CACHE = 0;
  cache->WQ_FORWARD = 0;
  cache->WQ_FULL = 0;
}

void finish_warmup()
{
  uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
  elapsed_minute -= elapsed_hour * 60;
  elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

  // reset core latency
  // note: since re-ordering he function calls in the main simulation loop, it's
  // no longer necessary to add
  //       extra latency for scheduling and execution, unless you want these
  //       steps to take longer than 1 cycle.
  // PAGE_TABLE_LATENCY = 100;
  // SWAP_LATENCY = 100000;

  cout << endl;
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    cout << "Warmup complete CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
    cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;

    ooo_cpu[i]->begin_sim_cycle = ooo_cpu[i]->current_cycle;
    ooo_cpu[i]->begin_sim_instr = ooo_cpu[i]->num_retired;

    // reset branch stats
    ooo_cpu[i]->num_branch = 0;
    ooo_cpu[i]->branch_mispredictions = 0;
    ooo_cpu[i]->total_rob_occupancy_at_branch_mispredict = 0;

    for (uint32_t j = 0; j < 8; j++) {
      ooo_cpu[i]->total_branch_types[j] = 0;
      ooo_cpu[i]->branch_type_misses[j] = 0;
    }

    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
      reset_cache_stats(i, *it);
  }
  cout << endl;

  // reset DRAM stats
  for (uint32_t i = 0; i < DRAM_CHANNELS; i++) {
    DRAM.channels[i].WQ_ROW_BUFFER_HIT = 0;
    DRAM.channels[i].WQ_ROW_BUFFER_MISS = 0;
    DRAM.channels[i].RQ_ROW_BUFFER_HIT = 0;
    DRAM.channels[i].RQ_ROW_BUFFER_MISS = 0;
  }
}

void signal_handler(int signal)
{
  cout << "Caught signal: " << signal << endl;
  exit(1);
}

int main(int argc, char** argv)
{
  // interrupt signal hanlder
  struct sigaction sigIntHandler;
  sigIntHandler.sa_handler = signal_handler;
  sigemptyset(&sigIntHandler.sa_mask);
  sigIntHandler.sa_flags = 0;
  sigaction(SIGINT, &sigIntHandler, NULL);

  cout << endl << "*** ChampSim Multicore Out-of-Order Simulator ***" << endl << endl;

  // initialize knobs
  uint8_t show_heartbeat = 1;

  // check to see if knobs changed using getopt_long()
  int traces_encountered = 0;
  static struct option long_options[] = {{"warmup_instructions", required_argument, 0, 'w'},
                                         {"simulation_instructions", required_argument, 0, 'i'},
                                         {"hide_heartbeat", no_argument, 0, 'h'},
                                         {"cloudsuite", no_argument, 0, 'c'},
                                         {"traces", no_argument, &traces_encountered, 1},
                                         {0, 0, 0, 0}};

  int c;
  while ((c = getopt_long_only(argc, argv, "w:i:hc", long_options, NULL)) != -1 && !traces_encountered) {
    switch (c) {
    case 'w':
      warmup_instructions = atol(optarg);
      break;
    case 'i':
      simulation_instructions = atol(optarg);
      break;
    case 'h':
      show_heartbeat = 0;
      break;
    case 'c':
      knob_cloudsuite = 1;
      MAX_INSTR_DESTINATIONS = NUM_INSTR_DESTINATIONS_SPARC;
      break;
    case 0:
      break;
    default:
      abort();
    }
  }

  cout << "Warmup Instructions: " << warmup_instructions << endl;
  cout << "Simulation Instructions: " << simulation_instructions << endl;
  cout << "Number of CPUs: " << NUM_CPUS << endl;

  long long int dram_size = DRAM_CHANNELS * DRAM_RANKS * DRAM_BANKS * DRAM_ROWS * DRAM_COLUMNS * BLOCK_SIZE / 1024 / 1024; // in MiB
  std::cout << "Off-chip DRAM Size: ";
  if (dram_size > 1024)
    std::cout << dram_size / 1024 << " GiB";
  else
    std::cout << dram_size << " MiB";
  std::cout << " Channels: " << DRAM_CHANNELS << " Width: " << 8 * DRAM_CHANNEL_WIDTH << "-bit Data Rate: " << DRAM_IO_FREQ << " MT/s" << std::endl;

  std::cout << std::endl;
  std::cout << "VirtualMemory physical capacity: " << std::size(vmem.ppage_free_list) * vmem.page_size;
  std::cout << " num_ppages: " << std::size(vmem.ppage_free_list) << std::endl;
  std::cout << "VirtualMemory page size: " << PAGE_SIZE << " log2_page_size: " << LOG2_PAGE_SIZE << std::endl;

  std::cout << std::endl;
  for (int i = optind; i < argc; i++) {
    std::cout << "CPU " << traces.size() << " runs " << argv[i] << std::endl;

    traces.push_back(get_tracereader(argv[i], traces.size(), knob_cloudsuite));

    if (traces.size() > NUM_CPUS) {
      printf("\n*** Too many traces for the configured number of cores ***\n\n");
      assert(0);
    }
  }

  if (traces.size() != NUM_CPUS) {
    printf("\n*** Not enough traces for the configured number of cores ***\n\n");
    assert(0);
  }
  // end trace file setup

  // SHARED CACHE
  for (O3_CPU* cpu : ooo_cpu) {
    cpu->initialize_core();
  }

  for (auto it = caches.rbegin(); it != caches.rend(); ++it) {
    (*it)->impl_prefetcher_initialize();
    (*it)->impl_replacement_initialize();
  }

  // simulation entry point
  while (std::any_of(std::begin(simulation_complete), std::end(simulation_complete), std::logical_not<uint8_t>())) {

    uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
    elapsed_minute -= elapsed_hour * 60;
    elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

    for (auto op : operables) {
      try {
        op->_operate();
      } catch (champsim::deadlock& dl) {
        // ooo_cpu[dl.which]->print_deadlock();
        // std::cout << std::endl;
        // for (auto c : caches)
        for (auto c : operables) {
          c->print_deadlock();
          std::cout << std::endl;
        }

        abort();
      }
    }
    std::sort(std::begin(operables), std::end(operables), champsim::by_next_operate());

    for (std::size_t i = 0; i < ooo_cpu.size(); ++i) {
      // read from trace
      while (ooo_cpu[i]->fetch_stall == 0 && ooo_cpu[i]->instrs_to_read_this_cycle > 0) {
        ooo_cpu[i]->init_instruction(traces[i]->get());
      }

      // heartbeat information
      if (show_heartbeat && (ooo_cpu[i]->num_retired >= ooo_cpu[i]->next_print_instruction)) {
        float cumulative_ipc;
        if (warmup_complete[i])
          cumulative_ipc = (1.0 * (ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr)) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
        else
          cumulative_ipc = (1.0 * ooo_cpu[i]->num_retired) / ooo_cpu[i]->current_cycle;
        float heartbeat_ipc = (1.0 * ooo_cpu[i]->num_retired - ooo_cpu[i]->last_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->last_sim_cycle);

        cout << "Heartbeat CPU " << i << " instructions: " << ooo_cpu[i]->num_retired << " cycles: " << ooo_cpu[i]->current_cycle;
        cout << " heartbeat IPC: " << heartbeat_ipc << " cumulative IPC: " << cumulative_ipc;
        cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;
        ooo_cpu[i]->next_print_instruction += STAT_PRINTING_PERIOD;

        ooo_cpu[i]->last_sim_instr = ooo_cpu[i]->num_retired;
        ooo_cpu[i]->last_sim_cycle = ooo_cpu[i]->current_cycle;
      }

      // check for warmup
      // warmup complete
      if ((warmup_complete[i] == 0) && (ooo_cpu[i]->num_retired > warmup_instructions)) {
        warmup_complete[i] = 1;
        all_warmup_complete++;
      }
      if (all_warmup_complete == NUM_CPUS) { // this part is called only once
                                             // when all cores are warmed up
        all_warmup_complete++;
        finish_warmup();
      }

      // simulation complete
      if ((all_warmup_complete > NUM_CPUS) && (simulation_complete[i] == 0)
          && (ooo_cpu[i]->num_retired >= (ooo_cpu[i]->begin_sim_instr + simulation_instructions))) {
        simulation_complete[i] = 1;
        ooo_cpu[i]->finish_sim_instr = ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr;
        ooo_cpu[i]->finish_sim_cycle = ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle;

        cout << "Finished CPU " << i << " instructions: " << ooo_cpu[i]->finish_sim_instr << " cycles: " << ooo_cpu[i]->finish_sim_cycle;
        cout << " cumulative IPC: " << ((float)ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle);
        cout << " (Simulation time: " << elapsed_hour << " hr " << elapsed_minute << " min " << elapsed_second << " sec) " << endl;

        for (auto it = caches.rbegin(); it != caches.rend(); ++it)
          record_roi_stats(i, *it);
      }
    }
  }

  uint64_t elapsed_second = (uint64_t)(time(NULL) - start_time), elapsed_minute = elapsed_second / 60, elapsed_hour = elapsed_minute / 60;
  elapsed_minute -= elapsed_hour * 60;
  elapsed_second -= (elapsed_hour * 3600 + elapsed_minute * 60);

  cout << endl << "ChampSim completed all CPUs" << endl;
  if (NUM_CPUS > 1) {
    cout << endl << "Total Simulation Statistics (not including warmup)" << endl;
    for (uint32_t i = 0; i < NUM_CPUS; i++) {
      cout << endl
           << "CPU " << i
           << " cumulative IPC: " << (float)(ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr) / (ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle);
      cout << " instructions: " << ooo_cpu[i]->num_retired - ooo_cpu[i]->begin_sim_instr
           << " cycles: " << ooo_cpu[i]->current_cycle - ooo_cpu[i]->begin_sim_cycle << endl;
      for (auto it = caches.rbegin(); it != caches.rend(); ++it)
        print_sim_stats(i, *it);
    }
  }

  cout << endl << "Region of Interest Statistics" << endl;
  for (uint32_t i = 0; i < NUM_CPUS; i++) {
    cout << endl << "CPU " << i << " cumulative IPC: " << ((float)ooo_cpu[i]->finish_sim_instr / ooo_cpu[i]->finish_sim_cycle);
    cout << " instructions: " << ooo_cpu[i]->finish_sim_instr << " cycles: " << ooo_cpu[i]->finish_sim_cycle << endl;
    for (auto it = caches.rbegin(); it != caches.rend(); ++it)
      print_roi_stats(i, *it);
  }

  for (auto it = caches.rbegin(); it != caches.rend(); ++it)
    (*it)->impl_prefetcher_final_stats();

  for (auto it = caches.rbegin(); it != caches.rend(); ++it)
    (*it)->impl_replacement_final_stats();

#ifndef CRC2_COMPILE
  print_dram_stats();
  print_branch_stats();
#endif

  return 0;
}
