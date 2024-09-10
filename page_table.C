#include "assert.H"
#include "exceptions.H"
#include "console.H"
#include "paging_low.H"
#include "page_table.H"

PageTable * PageTable::current_page_table = NULL;
unsigned int PageTable::paging_enabled = 0;
ContFramePool * PageTable::kernel_mem_pool = NULL;
ContFramePool * PageTable::process_mem_pool = NULL;
unsigned long PageTable::shared_size = 0;



void PageTable::init_paging(ContFramePool * _kernel_mem_pool,
                            ContFramePool * _process_mem_pool,
                            const unsigned long _shared_size)
{
   kernel_mem_pool = _kernel_mem_pool;
   process_mem_pool = _process_mem_pool;
   shared_size = _shared_size;
   Console::puts("Initialized Paging System\n");
}

PageTable::PageTable()
{
   //get one frame for page directory
   unsigned long frame_no = kernel_mem_pool->get_frames(1);
   page_directory = (unsigned long*) (frame_no * PAGE_SIZE);

   //get first page table page frame
   unsigned long ptp_frame = process_mem_pool->get_frames(1);
   unsigned long * page_table = (unsigned long*) (ptp_frame * PAGE_SIZE);

   //fill first page table
   unsigned long address = 0;
   for(unsigned int i = 0; i < 1024; i++){
      page_table[i] = address | 0x3;
      address = address + 4096;
   }

   //Assign first spot in directory to first page table page
   page_directory[0] = (unsigned long) page_table; 
   page_directory[0] = page_directory[0] | 0x3;

   //set rest of pages to not present
   for(unsigned int i = 1; i < 1023; i++){
      page_directory[i] = 0 | 0x2; //010 = not present/valid
   }

   //set last entry to point back to page directory
   page_directory[1023] = (unsigned long) page_directory;
   page_directory[1023] = page_directory[1023] | 0x3;
   head = NULL;
}

void PageTable::load()
{
   write_cr3((unsigned long) page_directory);
   current_page_table = this;
   Console::puts("Loaded page table\n");
}

void PageTable::enable_paging()
{
   write_cr0(read_cr0() | 0x80000000); //set paging bit to 1
   paging_enabled = 1;
   Console::puts("Enabled paging\n");
}

void PageTable::handle_fault(REGS * _r)
{
   //read in faulting address
   unsigned long fault_addr = read_cr2();
   
   //Check if addr is legitimate
   VMPool* current = current_page_table->head;
   bool is_legit_addr  = false;
   while(current){
      if(current->is_legitimate(fault_addr)){
         is_legit_addr = true;
         break;
      }
      current = current->next;
   }
   //if not legit, return
   if(!is_legit_addr){
      Console::puts("Segmentation fault: Invalid memory access at ");
      Console::puti(fault_addr); Console::puts("\n");
      return;
   }

   //PDE address = page directory address + page table number
   unsigned long * pde_addr = current_page_table->page_directory + (fault_addr >> 22);
   
   //check if PDE is not present
   if(!(*pde_addr & 0x1)){
      unsigned long pt_frame_no = kernel_mem_pool->get_frames(1);
      unsigned long * page_table = (unsigned long*) (pt_frame_no * PAGE_SIZE);
   
      //initialize all page table entries as not present
      for(unsigned int i = 0; i < 1024; i++){
         page_table[i] = 0;
      }

      //set data at pde_addr to the index of new page table page and change present and R/W bit to 1
      *pde_addr = (pt_frame_no << 12) | 0x3;
   }

   //PTE address = page table address + pte number
   unsigned long * pte_addr = ((unsigned long*)(*pde_addr & ~0xFFF)) + ((fault_addr >> 12) & 0x3FF);
   unsigned long frame_no = process_mem_pool->get_frames(1);
   
   //shift frame_no to bits 31-12 and set valid and write bit
   *pte_addr = (frame_no << 12) | 0x3;
   Console::puts("handled page fault\n");
}

unsigned long * PageTable::PDE_address(unsigned long addr){
   unsigned long * pde_addr = current_page_table->page_directory + (addr >> 22);
   return pde_addr;
}

unsigned long * PageTable::PTE_address(unsigned long addr){
   unsigned long * pde_addr = PDE_address(addr);
   unsigned long * pte_addr = ((unsigned long *) (*pde_addr & ~0xFFF)) + ((addr >> 12) & 0x3FF);
   return pte_addr;
}

void PageTable::register_pool(VMPool * _pool){
   _pool->next = head;
   head = _pool;
}

void PageTable::free_page(unsigned long _page_no){
   unsigned long page_addr = _page_no * PAGE_SIZE; 
   unsigned long* pte_addr = PTE_address(page_addr);

   //if valid, first part of pte is frame number
   if(*pte_addr & 0x1 == 1){
      ContFramePool::release_frames((*pte_addr>>12));
      //mark page table entry as invalid/ clear pte
      *pte_addr = 0;
      //flush the TLB
      write_cr3((unsigned long) page_directory);
   }
}
