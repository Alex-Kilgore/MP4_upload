/*
 File: ContFramePool.C
 
 Author: Alex Kilgore
 Date  : 9/11/23
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */



/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

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
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool* ContFramePool::head = nullptr;

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no)
{
    assert(_n_frames <= FRAME_SIZE * 8);
    base_frame_no = _base_frame_no;
    nframes = _n_frames;
    nFreeFrames = _n_frames;
    info_frame_no = _info_frame_no;
    
    //Choose where to keep bitmap
    if(info_frame_no == 0){
	    bitmap = (unsigned char*) (base_frame_no * FRAME_SIZE);
    } else {
	    bitmap = (unsigned char*) (info_frame_no * FRAME_SIZE);
    }

    //Mark all frames as Free
    for(int fno = 0; fno < nframes; fno++){
	    set_state(fno, FrameState::Free);
    }

    //Mark info frames as used
    set_state(info_frame_no, FrameState::HoS);
    
    //Manage the list of Frame Pools
    if(head == NULL){
	    head = this;
    } else {
        while(head->next != NULL){
	    head = head->next;
	}
	head->next = this;
    } 
    this->next = NULL;
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    unsigned int cons_free = 0;
    unsigned int frame_no = 0;
    unsigned int head_frame = 0;
    while(cons_free < _n_frames){
        // If frame is Free, increment counter and set head frame if first consecutive
        if(get_state(frame_no) == FrameState::Free){
            cons_free++;
            if(cons_free == 1){
            head_frame = frame_no;
            }
        } else{ //Non-consecutive Free frames, reset consecutive counter and head frame
            cons_free = 0;
            head_frame = 0;
        }
        frame_no++;
    }
    
    // Can't find enough consecutive frames
    if(cons_free < _n_frames){
	    Console::puts("Get_frames returns not enought consecutive frames\n");
        return 0;
    }

    // Loop through frames to be allocated and set first frame to HoS
    for(unsigned int i = head_frame; i < head_frame + _n_frames; i++){
	    if(i == head_frame){
		    set_state(i, FrameState::HoS);
	    } else {
		    set_state(i, FrameState::Used);
	    }
    }
    return head_frame + base_frame_no;
}


void ContFramePool::mark_inaccessible(unsigned long _base_frame_no, unsigned long _n_frames)
{
    for(unsigned int i = _base_frame_no; i < _base_frame_no + _n_frames; i++){
	if(i == _base_frame_no){
		set_state(i, FrameState::HoS);
	} else {
		set_state(i, FrameState::Used);
	}
    }
}


void ContFramePool::release_frames(unsigned long _first_frame_no){
    ContFramePool* temp = head;
    
    //No frame pools in list
    if(temp == NULL){
        Console::puts("Frame Pools list empty\n");
   	return;
    }

    while(temp->next != NULL){
        //Is the first frame to release in the current frame pool?
	    if(temp->base_frame_no <= _first_frame_no && temp->base_frame_no + temp->nframes >= _first_frame_no){
            temp->release_frames_in_pool(_first_frame_no);
	    return;
	    }
	temp = temp->next;
    }

    //Temp is not null, but temp->next is null
    //IE Only one frame pool in list
    if(temp->base_frame_no <= _first_frame_no && temp->base_frame_no + temp->nframes >= _first_frame_no){
        temp->release_frames_in_pool(_first_frame_no);
	    return;
    } 
    Console::puts("Requested frame is not in any frame pool\n");
	return;
}


void ContFramePool::release_frames_in_pool(unsigned long _first_frame_no)
{
    //Free the HoS, then loop until you find another HoS or a Free frame
    //After the first HoS, only release Used frames
    unsigned int curr_frame = _first_frame_no;
    do {
	    set_state(curr_frame, FrameState::Free);
        curr_frame++;
    } while(get_state(curr_frame) == FrameState::Used);
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    return _n_frames / (4 * 4096) + (_n_frames % (4096 * 4) > 0 ? 1 : 0);
}


	//Free = 00
	//Used = 01
	//HoS  = 10
ContFramePool::FrameState ContFramePool::get_state(unsigned long _frame_no){
	unsigned int bitmap_index = _frame_no / 4;
	unsigned char mask = 0x3 << (2 * (_frame_no % 4));
    unsigned char bm_and_mask = (bitmap[bitmap_index] & mask);
    
    if((bitmap[bitmap_index] & mask) >> (2 * (_frame_no % 4)) == 0){
        return FrameState::Free;
	} else if((bitmap[bitmap_index] & mask) >> (2 * (_frame_no % 4))== 0x1){
        return FrameState::Used;
	}
	return FrameState::HoS;
}

void ContFramePool::set_state(unsigned long _frame_no, FrameState _state){
	unsigned int bitmap_index = _frame_no / 4;
	//Create mask with 00 at target bits and 1 everywhere else
    unsigned char mask = 0xFF;
    mask &= ~(1 << (2 * (_frame_no % 4)));
    mask &= ~(1 << (2 * (_frame_no % 4) + 1));
	
	//Reset target bits to 00
	bitmap[bitmap_index] &= mask;

	switch(_state) {
		case FrameState::Free:
			break;
		case FrameState::Used:
			//Set mask to 01 for target bits
			mask = 0x1 << (2 * (_frame_no % 4));
			bitmap[bitmap_index] |= mask;
			break;
		case FrameState::HoS:
			//Set mask to 10 for target bits
			mask = 0x1 << (2 * (_frame_no % 4) + 1);
			bitmap[bitmap_index] |= mask;
			break;
	}
}