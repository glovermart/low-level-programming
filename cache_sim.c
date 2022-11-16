#include <assert.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

typedef enum { dm, fa } cache_map_t;
typedef enum { uc, sc } cache_org_t;
typedef enum { instruction, data } access_t;

typedef struct {
  uint32_t address;
  access_t accesstype;
} mem_access_t;

typedef struct {
  uint64_t accesses;
  uint64_t hits;
  // You can declare additional statistics if
  // you like, however you are now allowed to
  // remove the accesses or hits
} cache_stat_t;

// DECLARE CACHES AND COUNTERS FOR THE STATS HERE

uint32_t cache_size;
uint32_t block_size = 64;
cache_map_t cache_mapping;
cache_org_t cache_org;

uint64_t *unified_cache,*data_cache,*instruction_cache;
uint8_t number_of_blocks;
bool valid = false;  // Assume compulsory miss(Cold start) on start of cache simulator(first cache access)
uint32_t tag, index_i, n_index_i, n_tag;
uint8_t offset = 6 ; // log2 (block_size) offset bits are constant for this system
int cache_block_counter;
int fifo_counter;

// USE THIS FOR YOUR CACHE STATISTICS
cache_stat_t cache_statistics;

/* Reads a memory access from the trace file and returns
 * 1) access type (instruction or data access
 * 2) memory address
 */
mem_access_t read_transaction(FILE* ptr_file) {
  char type;
  mem_access_t access;

  if (fscanf(ptr_file, "%c %x\n", &type, &access.address) == 2) {
    if (type != 'I' && type != 'D') {
      printf("Unkown access type\n");
      exit(0);
    }
    access.accesstype = (type == 'I') ? instruction : data;
    return access;
  }

  /* If there are no more entries in the file,
   * return an address 0 that will terminate the infinite loop in main
   */
  access.address = 0;
  return access;
}

void main(int argc, char** argv) {
  // Reset statistics:
  memset(&cache_statistics, 0, sizeof(cache_stat_t));

  /* Read command-line parameters and initialize:
   * cache_size, cache_mapping and cache_org variables
   */
  /* IMPORTANT: *IF* YOU ADD COMMAND LINE PARAMETERS (you really don't need to),
   * MAKE SURE TO ADD THEM IN THE END AND CHOOSE SENSIBLE DEFAULTS SUCH THAT WE
   * CAN RUN THE RESULTING BINARY WITHOUT HAVING TO SUPPLY MORE PARAMETERS THAN
   * SPECIFIED IN THE UNMODIFIED FILE (cache_size, cache_mapping and cache_org)
   */
  if (argc != 4) { /* argc should be 2 for correct execution */
    printf(
        "Usage: ./cache_sim [cache size: 128-4096] [cache mapping: dm|fa] "
        "[cache organization: uc|sc]\n");
    exit(0);
  } else {
    /* argv[0] is program name, parameters start with argv[1] */

    /* Set cache size */
    cache_size = atoi(argv[1]);

    /* Set Cache Mapping */
    if (strcmp(argv[2], "dm") == 0) {
      cache_mapping = dm;
    } else if (strcmp(argv[2], "fa") == 0) {
      cache_mapping = fa;
    } else {
      printf("Unknown cache mapping\n");
      exit(0);
    }

    /* Set Cache Organization */
    if (strcmp(argv[3], "uc") == 0) {
      cache_org = uc;
    } else if (strcmp(argv[3], "sc") == 0) {
      cache_org = sc;
    } else {
      printf("Unknown cache organization\n");
      exit(0);
    }
  }

  /* Open the file mem_trace.txt to read memory accesses */
  FILE* ptr_file;
  ptr_file = fopen("mem_trace1.txt", "r");

  if (!ptr_file) {
    printf("Unable to open the trace file\n");
    exit(1);
  }

  //Check if cache size is a power of 2
  if(ceil(log2(cache_size)) != floor(log2(cache_size))){
    printf("Cache size must be a power of 2");
    exit(1);
  }
  //Check if cache size is of size 128 to 4096 bytes
  if (cache_size < 128 || cache_size > 4096){
    printf("Cache size must be a power of 2 between 128 and 4096 bytes");
    exit(1);
  }

  number_of_blocks = cache_size / block_size ;
  n_index_i = log2(number_of_blocks);
  n_tag = 32- offset - n_index_i; // 32 bit system assumed from specifications

  

  if(cache_org == uc )
  { 
    // Allocate memory for unified cache and initialize to zero
    unified_cache = (uint64_t*)calloc(number_of_blocks ,block_size) ;
     if (unified_cache == NULL) 
    { 
    printf("Memory has not been allocated for Unified Cache!!\n");
    }
  }
  
  else if (cache_org = sc)
  { 
    // Allocate memory for data and instruction cache and initialize to zero
    data_cache = (uint64_t*)calloc (number_of_blocks/2,block_size) ;
    if (data_cache == NULL) 
    { 
    printf("Memory has not been allocated for Data Cache!!\n");
    }
    instruction_cache = (uint64_t*)calloc(number_of_blocks/2,block_size);
    if (instruction_cache == NULL) 
    { 
    printf("Memory has not been allocated for Instruction Cache!!\n");
    }
    number_of_blocks /= 2;
  }

  /* Loop until whole trace file has been read */
  mem_access_t access;
  while (1) {
    access = read_transaction(ptr_file);
    // If no transactions left, break out of loop
    if (access.address == 0) break;
    printf("%d %x\n", access.accesstype, access.address);
    /* Do a cache access */
    // ADD YOUR CODE HERE
    switch (cache_mapping){

      case dm:
        index_i = (access.address >> offset) & (number_of_blocks-1); // Get index_i for DM cache
        tag = (access.address >>(offset + n_index_i)) & ((1<<n_tag)-1); //Get tag for DM cache

        /*check that tag matches and data/instruction is valid(cache has been written to with useful data) ;
        record as a hit otherwise fetch data/instruction into the cache from memory(RAM)*/

        switch(cache_org){

          case uc:
            if (unified_cache[index_i] == tag  && valid == true) 
            {
              cache_statistics.hits++;
            }
            else 
            {
              unified_cache[index_i] = tag;
              valid = true;
            }
          break;
          case sc:
          if (access.accesstype == data){
            if (data_cache[index_i] == tag  && valid == true)
            {
              cache_statistics.hits++;
            }
            else 
            {
              data_cache[index_i] = tag;
              valid = true;
            }
          }
          else{
            if (instruction_cache[index_i] == tag && valid == true)
            {
              cache_statistics.hits++;
            }
            else 
            {
              instruction_cache[index_i] = tag;
              valid = true;
            }
          }
          break;

        }
      break;
      case fa:
        tag = access.address >> offset;  //Get tag for FA cache; IGNORES index_i bits (part of tag bits)

        /*check that tag matches and data/instruction is valid(cache has been written to with useful data) ;
        record as a hit otherwise fetch data/instruction into the cache from memory(RAM). Do this for each cache block;
        Replace the first used block if all blocks are filled(FIFO replacement policy)*/
        switch ( cache_org){
        
          case uc:
            for (cache_block_counter = 0; cache_block_counter < number_of_blocks; cache_block_counter++)
            {
            if (unified_cache[cache_block_counter] == tag && valid == true)
            {
            cache_statistics.hits++;
            break;
            }
            }

            if (cache_block_counter == number_of_blocks)
            { 
              if (fifo_counter == number_of_blocks)
              {
                fifo_counter = 0;
              }
              unified_cache[fifo_counter] = tag;
              valid = true;
              fifo_counter++;
            }
          break;

          case sc:
             if (access.accesstype == data){
            for (cache_block_counter = 0; cache_block_counter < number_of_blocks; cache_block_counter++)
            {
            if (data_cache[cache_block_counter] == tag && valid == true)
            {
            cache_statistics.hits++;
            break;
            }
            }

            if (cache_block_counter == number_of_blocks)
            { 
              if (fifo_counter == number_of_blocks)
              {
                fifo_counter = 0;
              }
              data_cache[fifo_counter] = tag;
              valid = true;
              fifo_counter++;
            }
            }
            else{
              for (cache_block_counter = 0; cache_block_counter < number_of_blocks; cache_block_counter++)
            {
            if (instruction_cache[cache_block_counter] == tag && valid == true)
            {
            cache_statistics.hits++;
            break;
            }
            }

            if (cache_block_counter == number_of_blocks)
            { 
              if (fifo_counter == number_of_blocks)
              {
                fifo_counter = 0;
              }
              instruction_cache[fifo_counter] = tag;
              valid = true;
              fifo_counter++;
            }
           
            }
          break;

        }
         
      break;
    }
  cache_statistics.accesses++; //Record accesses to the cache
  }

  /* Print the statistics */
  // DO NOT CHANGE THE FOLLOWING LINES!
  printf("\nCache Statistics\n");
  printf("-----------------\n\n");
  printf("Accesses: %ld\n", cache_statistics.accesses);
  printf("Hits:     %ld\n", cache_statistics.hits);
  printf("Hit Rate: %.4f\n",
         (double)cache_statistics.hits / cache_statistics.accesses);
  // DO NOT CHANGE UNTIL HERE
  // You can extend the memory statistic printing if you like!

  //Deallocate (free) memory so other programs/processes can use memory
  free(unified_cache);
  free(data_cache);
  free(instruction_cache);
  /* Close the trace file */
  fclose(ptr_file);
}
