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

#include "cache.h"

#include <algorithm>
#include <iterator>

#include "champsim.h"
#include "champsim_constants.h"
#include "util.h"
#include "vmem.h"

#ifndef SANITY_CHECK
#define NDEBUG
#endif

extern VirtualMemory vmem;
extern uint8_t warmup_complete[NUM_CPUS];
extern std::array<CACHE*, NUM_CACHES> caches;

//#define CHECK_ADDRESS 7896855332
//#define CHECK_ADDRESS 3262463704
#define CHECK_ADDRESS -1
//#define CHECK_ADDRESS 2207790808
//#define CHECK_ADDRESS 2552814384
//#define CHECK_ADDRESS 6188511164
//#define CHECK_ADDRESS 2928288180
//#define CHECK_ADDRESS 1323003448
void print_cache( CACHE *c,uint64_t address){
  
  int set=c->get_set(address);
  int st=set*(c->NUM_WAY);
  int en=st+c->NUM_WAY;

  cout<<set<<" "<<st<<" "<<en<<endl;

  for(int i=st;i<en;i++){
    cout<<c->block[i].address<<" "<<c->block[i].valid<<" "<<c->block[i].dirty<<endl;
  }
  
}

//@Hari to check the names of cache 
bool check_string(string a,string req){
    int n=a.size(),m=req.size();

    for(int i=0;i+m-1<n;i++){
      if(a.substr(i,m)==req){
        return 1;
      }
    }

    return 0;
}

void assert_inclusivity(string NAME,uint32_t cpu,uint64_t address){
  bool is_L1D=check_string(NAME,"L1D");
  bool is_L2C=check_string(NAME,"L2C");

  
  if(is_L1D){


     for(auto it:caches){
       if(it->cpu==cpu and check_string(it->NAME,"L2C")){
         uint32_t set=it->get_set(address);
         uint32_t way=it->get_way(address,set);
         if(way==it->NUM_WAY or it->block[set*(it->NUM_WAY)+way].valid==0){
          cout<<"Address: "<<address<<" "<<way<<" "<<it->block[set*(it->NUM_WAY)+way].valid<<" "<<NAME<<" "<<(it->NAME)<<endl;
          assert(0);
         }
       }

       if(check_string(it->NAME,"LLC")){
         uint32_t set=it->get_set(address);
         uint32_t way=it->get_way(address,set);
         if(way==it->NUM_WAY or it->block[set*(it->NUM_WAY)+way].valid==0){
          cout<<"Address: "<<address<<" "<<way<<" "<<it->block[set*(it->NUM_WAY)+way].valid<<" "<<NAME<<" "<<(it->NAME)<<endl;
          //print_cache(it);
          assert(0);
         }
       }
     }
  }

  if(is_L2C){

    for(auto it:caches){
     if(check_string(it->NAME,"LLC")){
         uint32_t set=it->get_set(address);
         uint32_t way=it->get_way(address,set);
         if(way==it->NUM_WAY or it->block[set*(it->NUM_WAY)+way].valid==0){
          cout<<"Address: "<<address<<" "<<way<<" "<<it->block[set*(it->NUM_WAY)+way].valid<<" "<<NAME<<" "<<(it->NAME)<<endl;
          print_cache(it,address);
          assert(0);
         }
       }
    }

  }
}

void assert_exclusivity(string NAME,uint64_t address){
    
    bool is_LLC=check_string(NAME,"LLC");
    bool is_L2C=check_string(NAME,"L2C");
    
    if(is_LLC){

      for(auto it:caches){
        if(check_string(it->NAME,"L2C")){
          uint32_t set=it->get_set(address);
          uint32_t way=it->get_way(address,set);
          if(way<it->NUM_WAY and it->block[set*(it->NUM_WAY)+way].valid){
            cout<<"Address: "<<address<<" Found in LLC  failed!!"<<" "<<NAME<<endl;
            assert(0);
          }
        }
      }

    }else if(is_L2C){
      for(auto it:caches){
        if(check_string(it->NAME,"LLC")){
          uint32_t set=it->get_set(address);
          uint32_t way=it->get_way(address,set);
          if(way<it->NUM_WAY and it->block[set*(it->NUM_WAY)+way].valid){
            cout<<"Address: "<<address<<" Found in L2C failed!!"<<" "<<NAME<<endl;
            assert(0);
          }
        }
      }
    }
}



//@Hari to check if adddress is present in MSHR of a cache
bool inMSHR(CACHE* cache,uint64_t address){
  
   if(cache->MSHR.size()==0){
       return 0;
   }
   
   auto it=cache->MSHR.begin();

   for(auto it:cache->MSHR){
    if(it.address==address){
      return 1;
    }
   }
   
   return 0;
}

//@Hari to check if adddress is present in WQ of a cache
bool inWQ(string NAME,uint64_t address){

    
    CACHE *cache;

    for(auto it:caches){
      if(check_string(it->NAME,NAME)){
        cache=it;
        break;
      }
    }

    if(cache->WQ.size()==0){
      return 0;
    }
	
    for(auto it:cache->WQ){
      if(it.address==address){
        return 1;
      }
    }
    
    return 0;
}


//@Hari to check if evcition of a block does not viiolate inclusivity
int make_inclusive(CACHE *cache,uint64_t address,uint64_t instr_id,CACHE *wq_cache,bool check_dirty){


  /*if(inMSHR(cache,address)){
    return 0;
  }*/

  /*if(inWQ(cache,address)){
    return 0;
  }*/

  uint32_t set=cache->get_set(address);
  uint32_t way=cache->get_way(address,set);

  if(way==(cache->NUM_WAY)){
    return 1;
  }

  if(cache->block[set*(cache->NUM_WAY)+way].valid==0){
    return 1;
  }

  int in_write=0;

  if(cache->block[set*(cache->NUM_WAY)+way].dirty and check_dirty){

      PACKET writeback_packet;
	    writeback_packet.fill_level = wq_cache->lower_level->fill_level;
      writeback_packet.cpu = cache->cpu;
      writeback_packet.address = address;
      writeback_packet.data = cache->block[set*(cache->NUM_WAY)+way].data;
      writeback_packet.instr_id = instr_id;
      writeback_packet.ip = 0;
      writeback_packet.type = WRITEBACK;

      auto result = wq_cache->lower_level->add_wq(&writeback_packet);

      if (result == -2){
        return 0;
      }

      in_write=1;
      
  }



  cache->block[set*(cache->NUM_WAY)+way].valid=0;
  cache->block[set*(cache->NUM_WAY)+way].dirty=0;

  if(CHECK_ADDRESS==address){
    cout<<"Invalidated from "<<(cache->NAME)<<" "<<instr_id<<" "<<set<<" "<<way<<" "<<__func__<<endl;
  }

  return 1+in_write;


}


//@Hari to check if eviction is possible
int check_inclusive(string NAME,uint64_t victim_address,uint64_t instr_id,uint32_t cpu,CACHE *wq_cache){
	
		
		//@Hari checking the level of current cache hierarchy
		bool is_LLC=check_string(NAME,"LLC");
		bool is_L2C=check_string(NAME,"L2C");
    bool is_L1D=check_string(NAME,"L1D");

    CACHE *L2C,*L1D;

    for(auto it:caches){

      if(check_string(it->NAME,"L1D") and it->cpu==cpu){
        L1D=it;
      }

      if(check_string(it->NAME,"L2C") and it->cpu==cpu){
        L2C=it;
      }

    }

        

    //@Hari checking eviction in higher levels
    if(is_LLC){

      int res=make_inclusive(L1D,victim_address,instr_id,wq_cache,1);

      if(res==0){
        return 0;
      }

      int check=make_inclusive(L2C,victim_address,instr_id,wq_cache,res!=2);
      
      if(check==0){
        return 0;
      }

      if(res==2){
        check=2;
      }

      return check;
    }


    if(is_L2C or is_L1D){
      int check=make_inclusive(L1D,victim_address,instr_id,wq_cache,1);
      return check;
    }

    return 1;
  
			
}

bool check_hit(string NAME,uint64_t address){
    CACHE *ch;
    CACHE *LLC,*L2C,*L1D;
        
    for(auto it:caches){
      
      if(check_string(it->NAME,NAME)){
        ch=it;
        break;
      }
    }

    uint32_t set=ch->get_set(address);
    uint32_t way=ch->get_way(address,set);

    return (way<ch->NUM_WAY) and (ch->block[set*(ch->NUM_WAY)+way].valid);
}


void CACHE::handle_fill()
{
  while (writes_available_this_cycle > 0) {
	  
	 
    auto fill_mshr = MSHR.begin();
    if (fill_mshr == std::end(MSHR) || fill_mshr->event_cycle > current_cycle)
      return;

    // find victim
    uint32_t set = get_set(fill_mshr->address);

    auto set_begin = std::next(std::begin(block), set * NUM_WAY);
    auto set_end = std::next(set_begin, NUM_WAY);
    auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
    uint32_t way = std::distance(set_begin, first_inv);
	
    if (way == NUM_WAY)
      way = impl_replacement_find_victim(fill_mshr->cpu, fill_mshr->instr_id, set, &block.data()[set * NUM_WAY], fill_mshr->ip, fill_mshr->address,
                                         fill_mshr->type);


    //@Hari finding victim address for eviction
    uint64_t victim_address=block[set*NUM_WAY+way].address;

    
    int possible=1;


    if(MAKE_INCLUSIVE){
      //@Hari check if evicting this address does not violate inclusivity
      
      CACHE *wq_cache;


      for(auto it:caches){
        if(it->NAME=="LLC"){
          wq_cache=it;
          break;
        }
      }


		  possible=check_inclusive(NAME,victim_address,fill_mshr->instr_id,cpu,wq_cache);		
      if(possible==0){
        return ;
      }

      if(possible==2){
        block[set*NUM_WAY+way].dirty=0;
      }

    }

    //@Hari to make cache exclusive
    if(MAKE_EXCLUSIVE){
       
       //@Hari Bypassing filling of LLC and returning data to L2C
       if(check_string(NAME,"LLC")){
          
          if(fill_mshr->address==CHECK_ADDRESS){
            cout<<"LLC fill Bypassed for inserting "<<fill_mshr->instr_id<<" "<<__func__<<endl;
          }

          for (auto ret : fill_mshr->to_return){
            ret->return_data(&(*fill_mshr));
          }

          MSHR.erase(fill_mshr);
          writes_available_this_cycle--;
          return;
        }


        CACHE *LLC,*L2C,*L1D;
        
        for(auto it:caches){
          
          if(check_string(it->NAME,"LLC")){
            LLC=it;
          }

          if(check_string(it->NAME,"L2C")){
            L2C=it;
          }

          if(check_string(it->NAME,"L1D")){
            L1D=it;
          }

        }

        if(check_string(NAME,"L2C")){

          PACKET writeback_packet;
          writeback_packet.fill_level = LLC->fill_level;
          writeback_packet.cpu = cpu;
          writeback_packet.address = victim_address;
          writeback_packet.data=block[set*NUM_WAY+way].data;  
          writeback_packet.instr_id = fill_mshr->instr_id;
          writeback_packet.ip = 0;
          writeback_packet.type = WRITEBACK;
          writeback_packet.is_dirty=block[set*NUM_WAY+way].dirty;


          int result=LLC->add_wq(&writeback_packet);

          if (result == -2){
            return ;
          }

          block[set*NUM_WAY+way].dirty=0;
          block[set*NUM_WAY+way].valid=0;


          //@Hari Invalidation the address to be filled is already present in LLC
          uint32_t LLC_set=LLC->get_set(fill_mshr->address);
          uint32_t LLC_way=LLC->get_way(fill_mshr->address,LLC_set);

          if(LLC_way<LLC->NUM_WAY){
            LLC->block[LLC_set*(LLC->NUM_WAY)+LLC_way].valid=0;
          }


        }

        if(check_string(NAME,"L1D") or check_string(NAME,"L1I")){
      
          PACKET writeback_packet;
          writeback_packet.fill_level = lower_level->fill_level;
          writeback_packet.cpu = cpu;
          writeback_packet.address = victim_address;
          writeback_packet.data=block[set*NUM_WAY+way].data;  
          writeback_packet.instr_id = fill_mshr->instr_id;
          writeback_packet.ip = 0;
          writeback_packet.type = WRITEBACK;
          writeback_packet.is_dirty=block[set*NUM_WAY+way].dirty;


          int result=lower_level->add_wq(&writeback_packet);

          if (result == -2){
            return ;
          }

          block[set*NUM_WAY+way].dirty=0;
          block[set*NUM_WAY+way].valid=0;
        }


      }


      
      bool success = filllike_miss(set, way, *fill_mshr);

  
      if (!success)
        return;

        
    //@Hari check the values
    if(fill_mshr->address==CHECK_ADDRESS){
       cout<<"Inserted into :"<<NAME<<" "<<fill_mshr->instr_id<<" "<<way<<" "<<__func__<<endl;
    }

    //@Hari check the values
    if(victim_address==CHECK_ADDRESS){
        cout<<"Evicted from :"<<NAME<<" "<<fill_mshr->instr_id<<" "<<success<<" "<<way<<" "<<__func__<<endl;
    }

    if (way != NUM_WAY) {
      // update processed packets
      fill_mshr->data = block[set * NUM_WAY + way].data;

      //@Hari Setting the block as valid
      block[set*NUM_WAY+way].valid=1;

      for (auto ret : fill_mshr->to_return)
        ret->return_data(&(*fill_mshr));
    }

    MSHR.erase(fill_mshr);
    writes_available_this_cycle--;
  }
}

void CACHE::handle_writeback()
{
  while (writes_available_this_cycle > 0) {
    if (!WQ.has_ready())
      return;

    // handle the oldest entry
    PACKET& handle_pkt = WQ.front();

    // access cache
    uint32_t set = get_set(handle_pkt.address);
    uint32_t way = get_way(handle_pkt.address, set);


    //@Hari Bypassing the LLC fill if address in L2C
    if(check_string(NAME,"LLC") and MAKE_EXCLUSIVE){
      if(check_hit("L2C",handle_pkt.address)){
        WQ.pop_front();
        continue;
      }
    }



     
    bool is_hit=(way<NUM_WAY);

    if(MAKE_INCLUSIVE or MAKE_EXCLUSIVE){
      if(is_hit && block[set*NUM_WAY+way].valid==1){
        is_hit=1;
      }else{
        is_hit=0;
      }
    }

    BLOCK& fill_block = block[set * NUM_WAY + way];
    
    
    if(handle_pkt.address==CHECK_ADDRESS){
      cout<<"Address in WQ of "<<NAME<<" "<<handle_pkt.instr_id<<endl;
    }




    if (is_hit) // HIT
    { 


      impl_replacement_update_state(handle_pkt.cpu, set, way, fill_block.address, handle_pkt.ip, 0, handle_pkt.type, 1);

      if(handle_pkt.address==CHECK_ADDRESS){
        cout<<"Address present in  "<<NAME<<" "<<handle_pkt.instr_id<<" "<<set<<" "<<way<<" "<<__func__<<endl;
      }

      // COLLECT STATS
      sim_hit[handle_pkt.cpu][handle_pkt.type]++;
      sim_access[handle_pkt.cpu][handle_pkt.type]++;

      // mark dirty
      //@Hari changed for exclusivr to is_dirty field


      if(MAKE_EXCLUSIVE ){
        fill_block.dirty=handle_pkt.is_dirty;
      }else{
        fill_block.dirty = 1;
      }
    
    } else // MISS
    {
      bool success;
      if (handle_pkt.type == RFO && handle_pkt.to_return.empty()) {
        success = readlike_miss(handle_pkt);
      } else {

        // find victim
        
        auto set_begin = std::next(std::begin(block), set * NUM_WAY);
        auto set_end = std::next(set_begin, NUM_WAY);
        auto first_inv = std::find_if_not(set_begin, set_end, is_valid<BLOCK>());
        way = std::distance(set_begin, first_inv);
        if (way == NUM_WAY)
          way = impl_replacement_find_victim(handle_pkt.cpu, handle_pkt.instr_id, set, &block.data()[set * NUM_WAY], handle_pkt.ip, handle_pkt.address,
                                             handle_pkt.type);

      //@Hari finding victim address for eviction
      uint64_t victim_address=block[set*NUM_WAY+way].address;
      
      int possible=1;
      
      if(MAKE_INCLUSIVE){

        //@Hari check if evicting this address does not violate inclusivity
        CACHE *wq_cache;

        for(auto it:caches){
          if(it->NAME==NAME){
            wq_cache=it;
            break;
          }
        }

        possible=check_inclusive(NAME,victim_address,handle_pkt.instr_id,cpu,wq_cache);		
        
        if(possible==0){
          return ;
        }

        if(possible==2){
          block[set*NUM_WAY+way].dirty=0;
        }

      }

      if(MAKE_EXCLUSIVE){


        CACHE *LLC,*L2C,*L1D;
        
        for(auto it:caches){
          
          if(check_string(it->NAME,"LLC")){
            LLC=it;
          }

          if(check_string(it->NAME,"L2C")){
            L2C=it;
          }

          if(check_string(it->NAME,"L1D")){
            L1D=it;
          }

        }

       //@Hari evicting non dirty packet and putting it in write queue of LLC
        if(check_string(NAME,"L2C")){

          PACKET writeback_packet;
          writeback_packet.fill_level = lower_level->fill_level;
          writeback_packet.cpu = cpu;
          writeback_packet.address = victim_address;
          writeback_packet.data=block[set*NUM_WAY+way].data;  
          writeback_packet.instr_id = handle_pkt.instr_id;
          writeback_packet.ip = 0;
          writeback_packet.type = WRITEBACK;
          writeback_packet.is_dirty=block[set*NUM_WAY+way].dirty;


          int result=lower_level->add_wq(&writeback_packet);

          if (result == -2){
            return ;
          }

          block[set*NUM_WAY+way].dirty=0;
          block[set*NUM_WAY+way].valid=0;


          //@Hari Invalidation the address to be filled is already present in LLC
          uint32_t LLC_set=LLC->get_set(handle_pkt.address);
          uint32_t LLC_way=LLC->get_way(handle_pkt.address,LLC_set);

          if(LLC_way<LLC->NUM_WAY){
            LLC->block[LLC_set*(LLC->NUM_WAY)+LLC_way].valid=0;
          }


        }




        if(check_string(NAME,"L1D") or check_string(NAME,"L1I")){


        
          PACKET writeback_packet;
          writeback_packet.fill_level = lower_level->fill_level;
          writeback_packet.cpu = cpu;
          writeback_packet.address = victim_address;
          writeback_packet.data=block[set*NUM_WAY+way].data;  
          writeback_packet.instr_id = handle_pkt.instr_id;
          writeback_packet.ip = 0;
          writeback_packet.type = WRITEBACK;
          writeback_packet.is_dirty=block[set*NUM_WAY+way].dirty;


          int result=lower_level->add_wq(&writeback_packet);

          if (result == -2){
            return ;
          }

          block[set*NUM_WAY+way].dirty=0;
          block[set*NUM_WAY+way].valid=0;
          

    
        }
 

      }



      success = filllike_miss(set, way, handle_pkt);

      
      if (!success)
        return;

      //@Hari check the values
      if(handle_pkt.address == CHECK_ADDRESS){
        cout<<"Inserted into :"<<NAME<<" "<<handle_pkt.instr_id<<" "<<way<<" "<<__func__<<endl;
      }

      //@Hari check the values
      if(victim_address == CHECK_ADDRESS){
          cout<<"Evicted from :"<<NAME<<" "<<handle_pkt.instr_id<<" "<<possible<<" "<<__func__<<endl;
      }


    }

      
    }

    // remove this entry from WQ
    writes_available_this_cycle--;
    WQ.pop_front();
  }
}

void CACHE::handle_read()
{
  while (reads_available_this_cycle > 0) {

    if (!RQ.has_ready())
      return;

    // handle the oldest entry
    PACKET& handle_pkt = RQ.front();

    // A (hopefully temporary) hack to know whether to send the evicted paddr or
    // vaddr to the prefetcher
    ever_seen_data |= (handle_pkt.v_address != handle_pkt.ip);

    uint32_t set = get_set(handle_pkt.address);
    uint32_t way = get_way(handle_pkt.address, set);
    

    //@Hari checking hit for inclusivity.
    bool is_hit=(way<NUM_WAY);

    if(MAKE_INCLUSIVE or MAKE_EXCLUSIVE){
        if(way<NUM_WAY && block[set*NUM_WAY+way].valid==1){
            is_hit=1;
        }else{
            is_hit=0;
        }
    }

    if(handle_pkt.address==CHECK_ADDRESS){
      cout<<"Address in RQ of "<<NAME<<" "<<handle_pkt.instr_id<<endl;
    }
    
    if (is_hit) // HIT
    { 
      
      if(handle_pkt.address==CHECK_ADDRESS){
        cout<<"Address is present in "<<NAME<<" "<<handle_pkt.instr_id<<" "<<set<<" "<<way<<" "<<__func__<<endl;
      }

      //@Hari assertion to check the inclusivity
      if(MAKE_INCLUSIVE and warmup_complete[handle_pkt.cpu]){
        assert_inclusivity(NAME,cpu,handle_pkt.address);
      }

      //@Hari on hit in LLC invalidate it for MAKE_EXCLUSIVE 
      if(MAKE_EXCLUSIVE and check_string(NAME,"LLC") ){
        block[set*NUM_WAY+way].valid=0;
      }

      //@Hari assertion to check the MAKE_EXCLUSIVE
      if(MAKE_EXCLUSIVE and warmup_complete[handle_pkt.cpu]){
        assert_exclusivity(NAME,handle_pkt.address);
      }



      readlike_hit(set, way, handle_pkt);


      

    } else {

      if(handle_pkt.address==CHECK_ADDRESS){
        cout<<"Address not present in "<<NAME<<" "<<handle_pkt.instr_id<<" "<<__func__<<endl;
      }

      bool success = readlike_miss(handle_pkt);


      if (!success)
        return;


      //@Hari to align for L2C and LLC stats for EXCLUSIVE Cache hierarchy 
      if(MAKE_EXCLUSIVE){
          if(check_string(NAME,"LLC") or check_string(NAME,"L2C")){
            sim_miss[handle_pkt.cpu][handle_pkt.type]++;
            sim_access[handle_pkt.cpu][handle_pkt.type]++;
          }
      }
    

      if(handle_pkt.address==CHECK_ADDRESS){
        cout<<"Address miss handled in "<<NAME<<" "<<handle_pkt.instr_id<<" "<<__func__<<endl;
      }

    }

    // remove this entry from RQ
    RQ.pop_front();
    reads_available_this_cycle--;
  }
}

void CACHE::handle_prefetch()
{
  while (reads_available_this_cycle > 0) {
    if (!PQ.has_ready())
      return;

    // handle the oldest entry
    PACKET& handle_pkt = PQ.front();

    uint32_t set = get_set(handle_pkt.address);
    uint32_t way = get_way(handle_pkt.address, set);
    
    


    if (way < NUM_WAY) // HIT
    {
      readlike_hit(set, way, handle_pkt);

    } else {
      bool success = readlike_miss(handle_pkt);
      if (!success)
        return;
    }


    // remove this entry from PQ
    PQ.pop_front();
    reads_available_this_cycle--;
  }
}

void CACHE::readlike_hit(std::size_t set, std::size_t way, PACKET& handle_pkt)
{
  DP(if (warmup_complete[handle_pkt.cpu]) {
    std::cout << "[" << NAME << "] " << __func__ << " hit";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });

  BLOCK& hit_block = block[set * NUM_WAY + way];

  handle_pkt.data = hit_block.data;

  // update prefetcher on load instruction
  if (should_activate_prefetcher(handle_pkt.type) && handle_pkt.pf_origin_level < fill_level) {
    cpu = handle_pkt.cpu;
    uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    handle_pkt.pf_metadata = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, 1, handle_pkt.type, handle_pkt.pf_metadata);
  }

  // update replacement policy
  impl_replacement_update_state(handle_pkt.cpu, set, way, hit_block.address, handle_pkt.ip, 0, handle_pkt.type, 1);

  // COLLECT STATS
  sim_hit[handle_pkt.cpu][handle_pkt.type]++;
  sim_access[handle_pkt.cpu][handle_pkt.type]++;

  for (auto ret : handle_pkt.to_return)
    ret->return_data(&handle_pkt);

  // update prefetch stats and reset prefetch bit
  if (hit_block.prefetch) {
    pf_useful++;
    hit_block.prefetch = 0;
  }
}

bool CACHE::readlike_miss(PACKET& handle_pkt)
{
  DP(if (warmup_complete[handle_pkt.cpu]) {
    std::cout << "[" << NAME << "] " << __func__ << " miss";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });

  // check mshr
  auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(handle_pkt.address, OFFSET_BITS));
  bool mshr_full = (MSHR.size() == MSHR_SIZE);
  

  if (mshr_entry != MSHR.end()) // miss already inflight
  {
    // update fill location
    mshr_entry->fill_level = std::min(mshr_entry->fill_level, handle_pkt.fill_level);

    packet_dep_merge(mshr_entry->lq_index_depend_on_me, handle_pkt.lq_index_depend_on_me);
    packet_dep_merge(mshr_entry->sq_index_depend_on_me, handle_pkt.sq_index_depend_on_me);
    packet_dep_merge(mshr_entry->instr_depend_on_me, handle_pkt.instr_depend_on_me);
    packet_dep_merge(mshr_entry->to_return, handle_pkt.to_return);

    if (mshr_entry->type == PREFETCH && handle_pkt.type != PREFETCH) {
      // Mark the prefetch as useful
      if (mshr_entry->pf_origin_level == fill_level)
        pf_useful++;

      uint64_t prior_event_cycle = mshr_entry->event_cycle;
      *mshr_entry = handle_pkt;

      // in case request is already returned, we should keep event_cycle
      mshr_entry->event_cycle = prior_event_cycle;
    }
  } else {
    if (mshr_full)  // not enough MSHR resource
      return false; // TODO should we allow prefetches anyway if they will not
                    // be filled to this level?

    bool is_read = prefetch_as_load || (handle_pkt.type != PREFETCH);

    // check to make sure the lower level queue has room for this read miss
    int queue_type = (is_read) ? 1 : 3;
    if (lower_level->get_occupancy(queue_type, handle_pkt.address) == lower_level->get_size(queue_type, handle_pkt.address))
      return false;

    // Allocate an MSHR
    if (handle_pkt.fill_level <= fill_level) {
      auto it = MSHR.insert(std::end(MSHR), handle_pkt);
      it->cycle_enqueued = current_cycle;
      it->event_cycle = std::numeric_limits<uint64_t>::max();
    }

    if (handle_pkt.fill_level <= fill_level)
      handle_pkt.to_return = {this};
    else
      handle_pkt.to_return.clear();

    if (!is_read)
      lower_level->add_pq(&handle_pkt);
    else
      lower_level->add_rq(&handle_pkt);
  }

  // update prefetcher on load instructions and prefetches from upper levels
  if (should_activate_prefetcher(handle_pkt.type) && handle_pkt.pf_origin_level < fill_level) {
    cpu = handle_pkt.cpu;
    uint64_t pf_base_addr = (virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    handle_pkt.pf_metadata = impl_prefetcher_cache_operate(pf_base_addr, handle_pkt.ip, 0, handle_pkt.type, handle_pkt.pf_metadata);
  }

  

  return true;
}

bool CACHE::filllike_miss(std::size_t set, std::size_t way, PACKET& handle_pkt)
{
  DP(if (warmup_complete[handle_pkt.cpu]) {
    std::cout << "[" << NAME << "] " << __func__ << " miss";
    std::cout << " instr_id: " << handle_pkt.instr_id << " address: " << std::hex << (handle_pkt.address >> OFFSET_BITS);
    std::cout << " full_addr: " << handle_pkt.address;
    std::cout << " full_v_addr: " << handle_pkt.v_address << std::dec;
    std::cout << " type: " << +handle_pkt.type;
    std::cout << " cycle: " << current_cycle << std::endl;
  });

  bool bypass = (way == NUM_WAY);
#ifndef LLC_BYPASS
  assert(!bypass);
#endif
  assert(handle_pkt.type != WRITEBACK || !bypass);

  BLOCK& fill_block = block[set * NUM_WAY + way];
  bool evicting_dirty = !bypass && (lower_level != NULL) && fill_block.dirty;
  uint64_t evicting_address = 0;

  if (!bypass) {
    if (evicting_dirty) {
      PACKET writeback_packet;

      writeback_packet.fill_level = lower_level->fill_level;
      writeback_packet.cpu = handle_pkt.cpu;
      writeback_packet.address = fill_block.address;
      writeback_packet.data = fill_block.data;
      writeback_packet.instr_id = handle_pkt.instr_id;
      writeback_packet.ip = 0;
      writeback_packet.type = WRITEBACK;

      auto result = lower_level->add_wq(&writeback_packet);
      if (result == -2)
        return false;
    }

    if (ever_seen_data)
      evicting_address = fill_block.address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);
    else
      evicting_address = fill_block.v_address & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS);

    if (fill_block.prefetch)
      pf_useless++;

    if (handle_pkt.type == PREFETCH)
      pf_fill++;

    fill_block.valid = true;
    fill_block.prefetch = (handle_pkt.type == PREFETCH && handle_pkt.pf_origin_level == fill_level);
    fill_block.dirty = (handle_pkt.type == WRITEBACK || (handle_pkt.type == RFO && handle_pkt.to_return.empty()));

    if(handle_pkt.is_dirty==0){
      fill_block.dirty=0;
    }

    fill_block.address = handle_pkt.address;
    fill_block.v_address = handle_pkt.v_address;
    fill_block.data = handle_pkt.data;
    fill_block.ip = handle_pkt.ip;
    fill_block.cpu = handle_pkt.cpu;
    fill_block.instr_id = handle_pkt.instr_id;
  }

  if (warmup_complete[handle_pkt.cpu] && (handle_pkt.cycle_enqueued != 0))
    total_miss_latency += current_cycle - handle_pkt.cycle_enqueued;

  // update prefetcher
  cpu = handle_pkt.cpu;
  handle_pkt.pf_metadata =
      impl_prefetcher_cache_fill((virtual_prefetch ? handle_pkt.v_address : handle_pkt.address) & ~bitmask(match_offset_bits ? 0 : OFFSET_BITS), set, way,
                                 handle_pkt.type == PREFETCH, evicting_address, handle_pkt.pf_metadata);

  // update replacement policy
  impl_replacement_update_state(handle_pkt.cpu, set, way, handle_pkt.address, handle_pkt.ip, 0, handle_pkt.type, 0);

  // COLLECT STATS
  sim_miss[handle_pkt.cpu][handle_pkt.type]++;
  sim_access[handle_pkt.cpu][handle_pkt.type]++;

  return true;
}

void CACHE::operate()
{
  operate_writes();
  operate_reads();

  impl_prefetcher_cycle_operate();
}

void CACHE::operate_writes()
{
  // perform all writes
  writes_available_this_cycle = MAX_WRITE;
  handle_fill();
  handle_writeback();

  WQ.operate();
}

void CACHE::operate_reads()
{
  // perform all reads
  reads_available_this_cycle = MAX_READ;
  handle_read();
  va_translate_prefetches();
  handle_prefetch();

  RQ.operate();
  PQ.operate();
  VAPQ.operate();
}

uint32_t CACHE::get_set(uint64_t address) { return ((address >> OFFSET_BITS) & bitmask(lg2(NUM_SET))); }

uint32_t CACHE::get_way(uint64_t address, uint32_t set)
{
  auto begin = std::next(block.begin(), set * NUM_WAY);
  auto end = std::next(begin, NUM_WAY);
  return std::distance(begin, std::find_if(begin, end, eq_addr<BLOCK>(address, OFFSET_BITS)));
}

int CACHE::invalidate_entry(uint64_t inval_addr)
{
  uint32_t set = get_set(inval_addr);
  uint32_t way = get_way(inval_addr, set);

  if (way < NUM_WAY)
    block[set * NUM_WAY + way].valid = 0;

  return way;
}

int CACHE::add_rq(PACKET* packet)
{
  assert(packet->address != 0);
  RQ_ACCESS++;

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_RQ] " << __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << (packet->address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet->address << " v_address: " << packet->v_address << std::dec << " type: " << +packet->type
              << " occupancy: " << RQ.occupancy();
  })

  // check for the latest writebacks in the write queue
  champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address, match_offset_bits ? 0 : OFFSET_BITS));

  if (found_wq != WQ.end()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_WQ" << std::endl;)

    packet->data = found_wq->data;
    for (auto ret : packet->to_return)
      ret->return_data(packet);

    WQ_FORWARD++;
    return -1;
  }

  // check for duplicates in the read queue
  auto found_rq = std::find_if(RQ.begin(), RQ.end(), eq_addr<PACKET>(packet->address, OFFSET_BITS));
  if (found_rq != RQ.end()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_RQ" << std::endl;)

    packet_dep_merge(found_rq->lq_index_depend_on_me, packet->lq_index_depend_on_me);
    packet_dep_merge(found_rq->sq_index_depend_on_me, packet->sq_index_depend_on_me);
    packet_dep_merge(found_rq->instr_depend_on_me, packet->instr_depend_on_me);
    packet_dep_merge(found_rq->to_return, packet->to_return);

    RQ_MERGED++;

    return 0; // merged index
  }

  // check occupancy
  if (RQ.full()) {
    RQ_FULL++;

    DP(if (warmup_complete[packet->cpu]) std::cout << " FULL" << std::endl;)

    return -2; // cannot handle this request
  }

  // if there is no duplicate, add it to RQ
  if (warmup_complete[cpu])
    RQ.push_back(*packet);
  else
    RQ.push_back_ready(*packet);

  DP(if (warmup_complete[packet->cpu]) std::cout << " ADDED" << std::endl;)

  RQ_TO_CACHE++;
  return RQ.occupancy();
}

int CACHE::add_wq(PACKET* packet)
{
  WQ_ACCESS++;

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << (packet->address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet->address << " v_address: " << packet->v_address << std::dec << " type: " << +packet->type
              << " occupancy: " << RQ.occupancy();
  })

  // check for duplicates in the write queue
  champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address, match_offset_bits ? 0 : OFFSET_BITS));

  if (found_wq != WQ.end()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED" << std::endl;)

    WQ_MERGED++;
    return 0; // merged index
  }

  // Check for room in the queue
  if (WQ.full()) {
    DP(if (warmup_complete[packet->cpu]) std::cout << " FULL" << std::endl;)

    ++WQ_FULL;
    return -2;
  }

  // if there is no duplicate, add it to the write queue
  if (warmup_complete[cpu])
    WQ.push_back(*packet);
  else
    WQ.push_back_ready(*packet);

  DP(if (warmup_complete[packet->cpu]) std::cout << " ADDED" << std::endl;)

  WQ_TO_CACHE++;
  WQ_ACCESS++;

  return WQ.occupancy();
}

int CACHE::prefetch_line(uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  pf_requested++;

  PACKET pf_packet;
  pf_packet.type = PREFETCH;
  pf_packet.fill_level = (fill_this_level ? fill_level : lower_level->fill_level);
  pf_packet.pf_origin_level = fill_level;
  pf_packet.pf_metadata = prefetch_metadata;
  pf_packet.cpu = cpu;
  pf_packet.address = pf_addr;
  pf_packet.v_address = virtual_prefetch ? pf_addr : 0;

  if (virtual_prefetch) {
    if (!VAPQ.full()) {
      VAPQ.push_back(pf_packet);
      return 1;
    }
  } else {
    int result = add_pq(&pf_packet);
    if (result != -2) {
      if (result > 0)
        pf_issued++;
      return 1;
    }
  }

  return 0;
}

int CACHE::prefetch_line(uint64_t ip, uint64_t base_addr, uint64_t pf_addr, bool fill_this_level, uint32_t prefetch_metadata)
{
  static bool deprecate_printed = false;
  if (!deprecate_printed) {
    std::cout << "WARNING: The extended signature CACHE::prefetch_line(ip, "
                 "base_addr, pf_addr, fill_this_level, prefetch_metadata) is "
                 "deprecated."
              << std::endl;
    std::cout << "WARNING: Use CACHE::prefetch_line(pf_addr, fill_this_level, "
                 "prefetch_metadata) instead."
              << std::endl;
    deprecate_printed = true;
  }
  return prefetch_line(pf_addr, fill_this_level, prefetch_metadata);
}

void CACHE::va_translate_prefetches()
{
  // TEMPORARY SOLUTION: mark prefetches as translated after a fixed latency
  if (VAPQ.has_ready()) {
    VAPQ.front().address = vmem.va_to_pa(cpu, VAPQ.front().v_address).first;

    // move the translated prefetch over to the regular PQ
    int result = add_pq(&VAPQ.front());

    // remove the prefetch from the VAPQ
    if (result != -2)
      VAPQ.pop_front();

    if (result > 0)
      pf_issued++;
  }
}

int CACHE::add_pq(PACKET* packet)
{
  assert(packet->address != 0);
  PQ_ACCESS++;

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_WQ] " << __func__ << " instr_id: " << packet->instr_id << " address: " << std::hex << (packet->address >> OFFSET_BITS);
    std::cout << " full_addr: " << packet->address << " v_address: " << packet->v_address << std::dec << " type: " << +packet->type
              << " occupancy: " << RQ.occupancy();
  })

  // check for the latest wirtebacks in the write queue
  champsim::delay_queue<PACKET>::iterator found_wq = std::find_if(WQ.begin(), WQ.end(), eq_addr<PACKET>(packet->address, match_offset_bits ? 0 : OFFSET_BITS));

  if (found_wq != WQ.end()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_WQ" << std::endl;)

    packet->data = found_wq->data;
    for (auto ret : packet->to_return)
      ret->return_data(packet);

    WQ_FORWARD++;
    return -1;
  }

  // check for duplicates in the PQ
  auto found = std::find_if(PQ.begin(), PQ.end(), eq_addr<PACKET>(packet->address, OFFSET_BITS));
  if (found != PQ.end()) {
    DP(if (warmup_complete[packet->cpu]) std::cout << " MERGED_PQ" << std::endl;)

    found->fill_level = std::min(found->fill_level, packet->fill_level);
    packet_dep_merge(found->to_return, packet->to_return);

    PQ_MERGED++;
    return 0;
  }

  // check occupancy
  if (PQ.full()) {

    DP(if (warmup_complete[packet->cpu]) std::cout << " FULL" << std::endl;)

    PQ_FULL++;
    return -2; // cannot handle this request
  }

  // if there is no duplicate, add it to PQ
  if (warmup_complete[cpu])
    PQ.push_back(*packet);
  else
    PQ.push_back_ready(*packet);

  DP(if (warmup_complete[packet->cpu]) std::cout << " ADDED" << std::endl;)

  PQ_TO_CACHE++;
  return PQ.occupancy();
}

void CACHE::return_data(PACKET* packet)
{
  // check MSHR information
  auto mshr_entry = std::find_if(MSHR.begin(), MSHR.end(), eq_addr<PACKET>(packet->address, OFFSET_BITS));
  auto first_unreturned = std::find_if(MSHR.begin(), MSHR.end(), [](auto x) { return x.event_cycle == std::numeric_limits<uint64_t>::max(); });

  // sanity check
  if (mshr_entry == MSHR.end()) {
    std::cerr << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << packet->instr_id << " cannot find a matching entry!";
    std::cerr << " address: " << std::hex << packet->address;
    std::cerr << " v_address: " << packet->v_address;
    std::cerr << " address: " << (packet->address >> OFFSET_BITS) << std::dec;
    std::cerr << " event: " << packet->event_cycle << " current: " << current_cycle << std::endl;
    assert(0);
  }

  // MSHR holds the most updated information about this request
  mshr_entry->data = packet->data;
  mshr_entry->pf_metadata = packet->pf_metadata;
  mshr_entry->event_cycle = current_cycle + (warmup_complete[cpu] ? FILL_LATENCY : 0);

  DP(if (warmup_complete[packet->cpu]) {
    std::cout << "[" << NAME << "_MSHR] " << __func__ << " instr_id: " << mshr_entry->instr_id;
    std::cout << " address: " << std::hex << (mshr_entry->address >> OFFSET_BITS) << " full_addr: " << mshr_entry->address;
    std::cout << " data: " << mshr_entry->data << std::dec;
    std::cout << " index: " << std::distance(MSHR.begin(), mshr_entry) << " occupancy: " << get_occupancy(0, 0);
    std::cout << " event: " << mshr_entry->event_cycle << " current: " << current_cycle << std::endl;
  });

  // Order this entry after previously-returned entries, but before non-returned
  // entries
  std::iter_swap(mshr_entry, first_unreturned);
}

uint32_t CACHE::get_occupancy(uint8_t queue_type, uint64_t address)
{
  if (queue_type == 0)
    return std::count_if(MSHR.begin(), MSHR.end(), is_valid<PACKET>());
  else if (queue_type == 1)
    return RQ.occupancy();
  else if (queue_type == 2)
    return WQ.occupancy();
  else if (queue_type == 3)
    return PQ.occupancy();

  return 0;
}

uint32_t CACHE::get_size(uint8_t queue_type, uint64_t address)
{
  if (queue_type == 0)
    return MSHR_SIZE;
  else if (queue_type == 1)
    return RQ.size();
  else if (queue_type == 2)
    return WQ.size();
  else if (queue_type == 3)
    return PQ.size();

  return 0;
}

bool CACHE::should_activate_prefetcher(int type) { return (1 << static_cast<int>(type)) & pref_activate_mask; }

void CACHE::print_deadlock()
{
  if (!std::empty(MSHR)) {
    std::cout << NAME << " MSHR Entry" << std::endl;
    std::size_t j = 0;
    for (PACKET entry : MSHR) {
      std::cout << "[" << NAME << " MSHR] entry: " << j++ << " instr_id: " << entry.instr_id;
      std::cout << " address: " << std::hex << (entry.address >> LOG2_BLOCK_SIZE) << " full_addr: " << entry.address << std::dec << " type: " << +entry.type;
      std::cout << " fill_level: " << +entry.fill_level << " event_cycle: " << entry.event_cycle << std::endl;
    }
  } else {
    std::cout << NAME << " MSHR empty" << std::endl;
  }
}
