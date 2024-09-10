/*
 File: vm_pool.C
 
 Author:
 Date  :
 
 */

/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "vm_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"
#include "simple_keyboard.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   V M P o o l */
/*--------------------------------------------------------------------------*/

VMPool::VMPool(unsigned long  _base_address,
               unsigned long  _size,
               ContFramePool *_frame_pool,
               PageTable     *_page_table) {
    base_address = _base_address;
    size = _size;
    frame_pool = _frame_pool;
    page_table = _page_table;
    
    //register current pool
    page_table->register_pool(this);

    allocated_regions = (MemRegion*) _base_address;
    free_regions = (MemRegion*) (_base_address + PageTable::PAGE_SIZE/2);

    free_regions[0].start = 1;
    free_regions[0].length = (size/PageTable::PAGE_SIZE) -1;

    //set each alloc region index to invalid
    for(unsigned int i = 0; i < 256; i++){
        allocated_regions[i].start = 0;
        allocated_regions[i].length = 0;
    }

    //set rest of free region indicies as invalid
    for(unsigned int i = 1; i < 256; i++){
        free_regions[i].start = 0;
        free_regions[i].length = 0;
    }
}

unsigned long VMPool::allocate(unsigned long _size) {
    //get number of bytes to allocate on a page size boundary
    int numPages = (int) (_size / PageTable::PAGE_SIZE);
    if(_size % PageTable::PAGE_SIZE != 0){numPages++;}
    unsigned long numBytes = numPages * PageTable::PAGE_SIZE;

    //find first invalid index
    unsigned int i = 0;
    while(allocated_regions[i].start != 0){
        i++;
        if(i>255){
            Console::puts("allocation failed\n");
            return 0;
        }
    }
    allocated_regions[i].start = free_regions[0].start;
    allocated_regions[i].length = numBytes;
    free_regions[0].start = free_regions[0].start + numBytes;
    free_regions[0].length = free_regions[0].length - numBytes;

    return allocated_regions[i].start;    
}

void VMPool::release(unsigned long _start_address) {
    //find matching allocated region
    unsigned int i = 0;
    while(allocated_regions[i].start != _start_address && i < 256){
        i++;
    }
    //find first unused free region index
    unsigned int j = 0;
    while(free_regions[j].start != 0 && j < 256){
        j++;
    }
    //move released region to free_regions list
    free_regions[j].start = allocated_regions[i].start;
    free_regions[j].length = allocated_regions[i].length;
    
    unsigned int start_page = _start_address / PageTable::PAGE_SIZE;
    unsigned int numPages = allocated_regions[i].length / PageTable::PAGE_SIZE;
    //loop through pages and free each one
    for(unsigned int k = start_page; k <  start_page + numPages; k++){
        page_table->PageTable::free_page(k);
    }
    
    //clear allocated region
    allocated_regions[i].length = 0;
    allocated_regions[i].start = 0;
}

bool VMPool::is_legitimate(unsigned long _address) {
    //allocated and free region lists are always legitimate
    if(base_address < _address < base_address + PageTable::PAGE_SIZE){
        return true;
    }    
    //loop through allocated regions 
    for(unsigned int i = 0; i < 256; i++){
        //if address is between start and start + length
        if(allocated_regions[i].start < _address < allocated_regions[i].start + allocated_regions[i].length){
            return true;
        }
    }
    return false;
}

