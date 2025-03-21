// The Computer Language Benchmarks Game
// https://salsa.debian.org/benchmarksgame-team/benchmarksgame/
//
// Contributed by Jeremy Zerfas.
// Copied the idea to use a lookup table for reverse complementing two
// characters at a time from roman blog's C++ program.

// Here's a high level overview of how this program works to help you better
// understand this code, the notation being used for variable/function names,
// and the comments. The main thread reads in sequences one at a time from
// standard input and when the end of a sequence is found (indicated by the
// start of another sequence or the end of input), one or more new threads are
// created to then fully process that sequence before the main thread continues
// reading any additional sequences. The sequence is split up into chunks of a
// modest size, the chunks are processed starting with the ones at the end of
// the sequence and proceeding to the ones at the front of the sequence, and
// multiple chunks can be processed in parallel. The processing of chunks is
// considered to be being handled in three steps, first a chunk gets assigned to
// a thread, then the chunk is reverse complemented, and finally the chunk is
// written. A little extra care needs to be taken with the last step in order to
// ensure that the chunks are being written in the correct order.
//
// There are also a few more additional noteworthy optimizations being used by
// this program. During the reverse complementing step, characters can be
// reversed complemented sixteen or two at a time instead of one at a time by
// making use of SSE instructions or a modestly sized lookup table that is
// quickly computed when first starting the program. Before reverse
// complementing chunks, the program checks to see if the sequence is an
// optimally sized one that consists of only full lines and if that is the case
// it allows a simpler/faster code path to be used to do the reverse
// complementing since it makes it unnecessary to move newlines around in the
// sequence. Finally to help minimize reading/writing to main memory and to
// reduce memory usage, instead of doing an in-place reverse complement of a
// sequence or creating a reversed complemented copy of the whole sequence, each
// thread writes reverse complemented chunks to a per-thread chunk sized area of
// memory (which will likely be cached) and then writes the reversed
// complemented chunks from there.


// This string/character array is used to convert characters into the
// complementing character. There are also two smaller sixteen character
// lookup tables in the reverse_Complement_And_Move_Span() function that are
// used by its SSE code and those lookup tables should also be updated if this
// table is changed.
//
// In order to allow the SSE code to be able to function with just two sixteen
// character lookup tables, it performs lookups using only the lower five bits
// of characters (this causes the values for the ASCII letters A to Z AND a to z
// to change to 1 to 26). Note that some of the reverse complementing code also
// requires newlines to remain unchanged when complemented which is why the
// eleventh character is set to a newline. Since the lower five bits of the
// newline character match those for the letters J and j, the lookup tables for
// the SSE code have the complements for the letters J and j set to a newline
// character and to be consistent, the same was done here. This currently isn't
// a problem since the letters J and j aren't currently used but if that changes
// in the future, then these tables and the SSE code will need to be updated.
#define COMPLEMENT_LOOKUP \
  "          \n                                                     "\
  /*ABCDEFGHIJ KLMNOPQRSTUVWXYZ      abcdefghij klmnopqrstuvwxyz*/\
  " TVGH  CD \nM KN   YSAABW R       TVGH  CD \nM KN   YSAABW R"

// This controls the size of reads from the input and is also used as the
// initial sequence_Capacity.
#define READ_SIZE 65536

// This defines how many characters (including the newline) a full line of input
// should have.
#define LINE_LENGTH 61

// While processing a sequence, the sequence is broken up into chunks which can
// be processed in parallel on computers with multiple CPU cores.
// LINES_PER_CHUNK will set the maximum amount of lines that are allowed in each
// chunk and consequently the maximum size of each chunk will be equal to
// LINES_PER_CHUNK*LINE_LENGTH bytes in size. Each thread will create an array
// on the stack that is large enough to contain these chunks so LINES_PER_CHUNK
// must be set small enough to allow this array to fit inside the stack. For
// best performance, this value should probably be set so that the maximum chunk
// size is somewhere between the L1 data cache size and
// 1/(2*NUMBER_OF_CPU_CORES) of the last level cache size (assuming each CPU
// core has its own L1 cache and the last level cache is shared between CPU
// cores).
#define LINES_PER_CHUNK 8192

#include <stdint.h>
#include <pthread.h>
#include <immintrin.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

// uintptr_t should be the native integer type on most sane systems.
typedef uintptr_t uintnative_t;


// REVERSE_COMPLEMENT_LOOKUP will be used by the
// reverse_Complement_And_Move_Span() function to help speed up reverse
// complementing by allowing two characters to be reversed and complemented at
// once.
uint16_t REVERSE_COMPLEMENT_LOOKUP[65536];


// Since sequences can be broken up into chunks that can be processed in
// parallel by multiple threads, we need a few shared variables amongst the
// threads to coordinate the processing. shared_Variables_Mutex is used to make
// sure only one thread is reading/writing the other variables at a time.
// sequence_Characters_Left_To_Assign is self explanatory. Processing of the
// sequence starts at the rear and proceeds to the header_Newline in front of
// the sequence. Since processing is split up into a couple steps,
// front_Character_Of_Last_Assigned_Chunk and
// front_Character_Of_Last_Written_Chunk are used to separately keep track of
// the positions of the last assigned and written chunks. The chunks need to be
// written in the correct order after they are reverse complemented so threads
// may need to wait for other threads to finish writing their chunks first,
// write_Finished_Condition is used by the threads to let threads that are
// waiting know when a chunk is written by another thread so they can check to
// see if they should then write their own chunks.
pthread_mutex_t shared_Variables_Mutex=PTHREAD_MUTEX_INITIALIZER;
uintnative_t sequence_Characters_Left_To_Assign;
uint8_t * front_Character_Of_Last_Assigned_Chunk
  , * front_Character_Of_Last_Written_Chunk;
pthread_cond_t write_Finished_Condition=PTHREAD_COND_INITIALIZER;


// Reverse complement and move a span of characters. The span to be reverse
// complemented ends just before character_After_Src_Rear_Character and the
// reverse complemented span is moved so that its first character will be at
// dst_Front_Character. The size of the spans is span_Length characters long.
void reverse_Complement_And_Move_Span(uint8_t * dst_Front_Character
  , uint8_t * character_After_Src_Rear_Character, uintnative_t span_Length){

	// The loops farther down below will simultaneously reverse complement and
	// move either sixteen or two characters so consequently they can only
	// properly handle even span_Lengths. If the span_Lengths are odd, then this
	// if block below will do a single preliminary complement and move to make
	// the remaining span_Lengths an even number that the last loop can handle.
	if(span_Length%2){
		*dst_Front_Character++
		  =COMPLEMENT_LOOKUP[*--character_After_Src_Rear_Character];
		span_Length--;
	}


	// If this program is compiled for an architecture that supports SSE 4.1,
	// then this loop can use it to simultaneously reverse complement and move a
	// sixteen character subspan at a time if the span_Length is at least that
	// large.
	#ifdef __SSE4_1__
		for(; span_Length>=16; span_Length-=16, dst_Front_Character+=16){

			// Load the subspan before character_After_Src_Rear_Character.
			__m128i subspan=_mm_lddqu_si128(
			  (__m128i *)(character_After_Src_Rear_Character-=16));


			// Reverse the subspan.
			subspan=_mm_shuffle_epi8(subspan,_mm_set_epi8(
			  0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15));


			// AND all the characters in the subspan with 0b11111 which will
			// cause the values for the ASCII letters A to Z AND a to z to
			// change to 1 to 26. This allows a smaller lookup table to be used
			// which is useful here because we are going to be using 128 bit
			// (e.g. sixteen eight bit characters) SSE registers as lookup
			// tables which means we'll need to use at least two SSE registers,
			// do two lookups, and then select the appropriate result to use.
			// Larger lookup tables would require using more registers and doing
			// more lookups which will result in slower code.
			subspan=_mm_and_si128(subspan, _mm_set1_epi8(0b11111));


			// Now we load two sixteen character lookup tables into two SSE
			// registers, lookup each character of the subspan in both lookup
			// tables, and then figure out which lookup table has the correct
			// complement for the character based on whether the character's
			// lower five bits value was 0-15 or 16-31. Finally we write the
			// reverse complemented subspan to dst_Front_Character.
			//
			// Note that if the below lookup tables are changed, then the
			// COMPLEMENT_LOOKUP table up above should also be changed. It would
			// also be a good idea to read the comment above the
			// COMPLEMENT_LOOKUP table for why there is a newline in this lookup
			// table and why it is set as the complement of the letter J.
			__m128i complements_For_0_To_15_Values
			  =_mm_shuffle_epi8(_mm_setr_epi8(
			  //      A    B    C    D    E    F    G
			     0 , 'T', 'V', 'G', 'H',  0 ,  0 , 'C'
			  // H    I    J    K    L    M    N    O
			  , 'D',  0 ,'\n', 'M',  0 , 'K', 'N',  0), subspan);
			__m128i complements_For_16_To_31_Values
			  =_mm_shuffle_epi8(_mm_setr_epi8(
			  // P    Q    R    S    T    U    V    W
			     0 ,  0 , 'Y', 'S', 'A', 'A', 'B', 'W'
			  // X    Y    Z
			  ,  0 , 'R',  0 ,  0 ,  0 ,  0 ,  0 ,  0), subspan);
			__m128i mask_To_Select_complements_For_16_To_31_Values
			  =_mm_cmpgt_epi8(subspan, _mm_set1_epi8(15));
			_mm_storeu_si128((__m128i *)dst_Front_Character
			  , _mm_blendv_epi8(complements_For_0_To_15_Values
			  , complements_For_16_To_31_Values
			  , mask_To_Select_complements_For_16_To_31_Values));
		}
	#endif


	// Reverse complement and move any remaining characters and use the
	// REVERSE_COMPLEMENT_LOOKUP table that was set up during startup to allow
	// us to reverse complement two characters at a time. This is faster than
	// just complementing and moving one character at a time and at least for
	// some older architectures it isn't much slower than using the more complex
	// SSE code above, this can really help out when compiling for architectures
	// that don't support SSE 4.1.
	for(; span_Length>=2; span_Length-=2, dst_Front_Character+=2)
		*(uint16_t *)dst_Front_Character=REVERSE_COMPLEMENT_LOOKUP[
		  *(uint16_t *)(character_After_Src_Rear_Character-=2)];
}


// Look for chunks to assign, reverse complement, and write in order until there
// are no more sequence_Characters_Left_To_Assign.
void * process_Chunks(void * unused){

	// Keep assigning, reverse complementing, and writing chunks until one of
	// the inner statements notices that there are no
	// sequence_Characters_Left_To_Assign and returns to the calling
	// function/thread.
	for(;;){

		pthread_mutex_lock(&shared_Variables_Mutex);

			// Check if there are no more sequence_Characters_Left_To_Assign and
			// if so, then exit the function/thread.
			if(!sequence_Characters_Left_To_Assign){
				pthread_mutex_unlock(&shared_Variables_Mutex);
				return NULL;
			}

			// full_Line_Front_Span_Length is the number of nucleobases that
			// show up on the last line of the chunk (and also the sequence).
			// If the last line of the chunk is a non-full line, then
			// full_Line_Rear_Span_Length is the number of nucleobases that
			// would be required to be added to make it a full line, otherwise
			// it will be zero if the last line is a full line.
			uintnative_t full_Line_Front_Span_Length
			  =(sequence_Characters_Left_To_Assign-1)%LINE_LENGTH;
			uintnative_t full_Line_Rear_Span_Length
			  =LINE_LENGTH-1-full_Line_Front_Span_Length;

			// Determine the chunk_Size we should use and decrement
			// sequence_Characters_Left_To_Assign by that amount so that the
			// next thread looking for a chunk to work on knows the number of
			// sequence_Characters_Left_To_Assign. Also initialize
			// chunk_Characters_Left_To_Reverse_Complement with the chunk_Size
			// that was determined.
			uintnative_t chunk_Size
			  =sequence_Characters_Left_To_Assign>LINES_PER_CHUNK*LINE_LENGTH
			  ? LINES_PER_CHUNK*LINE_LENGTH
			  : sequence_Characters_Left_To_Assign;
			sequence_Characters_Left_To_Assign-=chunk_Size;
			uintnative_t chunk_Characters_Left_To_Reverse_Complement=chunk_Size;

			// Get a copy of the front_Character_Of_Last_Assigned_Chunk that we
			// can use to keep track of the
			// last_Reverse_Complemented_Chunk_Character.
			uint8_t * last_Reverse_Complemented_Chunk_Character
			  =front_Character_Of_Last_Assigned_Chunk;

			// The current chunk can't be written until the position of the
			// front_Character_Of_Last_Written_Chunk is backed up to match the
			// current position of the front_Character_Of_Last_Assigned_Chunk.
			// Record that position into first_Character_After_Chunk so that we
			// can later determine when it's OK to write this chunk.
			uint8_t * first_Character_After_Chunk
			  =front_Character_Of_Last_Assigned_Chunk;

			// Decrement front_Character_Of_Last_Assigned_Chunk by our
			// chunk_Size so that the next thread looking for a chunk to work on
			// will know to continue working from the front of our chunk.
			front_Character_Of_Last_Assigned_Chunk-=chunk_Size;

		pthread_mutex_unlock(&shared_Variables_Mutex);


		// Instead of doing an in-place reverse complement of the sequence, we
		// instead write the reverse complement of a smaller chunk to
		// chunk_To_Write and then we write chunk_To_Write after we finish
		// reverse complementing the chunk and all prior chunks have been
		// written. By writing/reading to/from chunk_To_Write, which should be
		// mostly cached, we avoid having to do another write/read of the entire
		// sequence to/from main memory.
		uint8_t chunk_To_Write[LINES_PER_CHUNK*LINE_LENGTH];
		uint8_t * chunk_Pos=chunk_To_Write;


		// We now have all information we need to know and can start reverse
		// complementing our chunk.

		// First we check to see if we have an optimal size sequence that
		// entirely consists of full lines (which will be the case when
		// full_Line_Rear_Span_Length is zero). In this case we can just make a
		// single call to reverse_Complement_And_Move_Span() and have it reverse
		// complement the entire chunk. This does require the
		// reverse_Complement_And_Move_Span() function to leave newlines
		// unchanged when they are complemented though.
		if(!full_Line_Rear_Span_Length){
			reverse_Complement_And_Move_Span(chunk_Pos
			  , last_Reverse_Complemented_Chunk_Character
			  , chunk_Characters_Left_To_Reverse_Complement);
			chunk_Characters_Left_To_Reverse_Complement=0;
		}


		// This loop reverse complements and moves full lines and also knows how
		// to skip over newlines as needed and then reinsert them at the
		// appropriate positions. This loop does most of the reverse
		// complementing work.
		for(; chunk_Characters_Left_To_Reverse_Complement>=LINE_LENGTH
		  ; chunk_Characters_Left_To_Reverse_Complement-=LINE_LENGTH){

			// Reverse complement and move a full_Line_Front_Span_Length of
			// nucleobases ending in front of the
			// last_Reverse_Complemented_Chunk_Character to chunk_Pos (which
			// will now be pointing at the postion for the first nucleobase of a
			// line). Then chunk_Pos and
			// last_Reverse_Complemented_Chunk_Character are
			// incremented/decremented respectively by
			// full_Line_Front_Span_Length and
			// last_Reverse_Complemented_Chunk_Character is decremented by one
			// more to skip over the newline in the preceding line.
			reverse_Complement_And_Move_Span(chunk_Pos
			  , last_Reverse_Complemented_Chunk_Character
			  , full_Line_Front_Span_Length);
			chunk_Pos+=full_Line_Front_Span_Length;
			last_Reverse_Complemented_Chunk_Character
			  -=full_Line_Front_Span_Length+1;

			// Reverse complement and move a full_Line_Rear_Span_Length of
			// nucleobases ending in front of the
			// last_Reverse_Complemented_Chunk_Character (which will now be
			// pointing at the newline after the last nucleobase of a line) to
			// chunk_Pos. Then chunk_Pos and
			// last_Reverse_Complemented_Chunk_Character are
			// incremented/decremented respectively by
			// full_Line_Rear_Span_Length.
			reverse_Complement_And_Move_Span(chunk_Pos
			  , last_Reverse_Complemented_Chunk_Character
			  , full_Line_Rear_Span_Length);
			chunk_Pos+=full_Line_Rear_Span_Length;
			last_Reverse_Complemented_Chunk_Character
			  -=full_Line_Rear_Span_Length;

			// Finally insert a newline at chunk_Pos and increment it because of
			// the newline.
			*chunk_Pos++='\n';
		}


		// If chunk_Size isn't evenly divisible by LINE_LENGTH, then there will
		// be one last partial line of
		// chunk_Characters_Left_To_Reverse_Complement and the line will contain
		// a full_Line_Front_Span_Length of nucelobases plus a preceding
		// newline. Reverse complement and move that partial line here if
		// necessary.
		if(chunk_Characters_Left_To_Reverse_Complement)
			reverse_Complement_And_Move_Span(chunk_Pos
			  , last_Reverse_Complemented_Chunk_Character
			  , full_Line_Front_Span_Length+1);


		// Before we can write the chunk_To_Write that we just finished reverse
		// complementing, we need to wait for all previous chunks to have been
		// written first which can be checked by seeing if
		// front_Character_Of_Last_Written_Chunk has been backed up enough to
		// equal first_Character_After_Chunk. Once that happens we can then
		// write the chunk_To_Write, back up
		// front_Character_Of_Last_Written_Chunk to reflect that the chunk is
		// now written, and broadcast the write_Finished_Condition in order to
		// wake up another thread that may be waiting for this chunk to have
		// been written.
		pthread_mutex_lock(&shared_Variables_Mutex);

			for(; front_Character_Of_Last_Written_Chunk
			  >first_Character_After_Chunk;)
				pthread_cond_wait(&write_Finished_Condition
				  , &shared_Variables_Mutex);

			(void)!write(STDOUT_FILENO, chunk_To_Write, chunk_Size);
			front_Character_Of_Last_Written_Chunk-=chunk_Size;
			pthread_cond_broadcast(&write_Finished_Condition);

		pthread_mutex_unlock(&shared_Variables_Mutex);
	}
}


void process_Sequence(uint8_t * sequence, uintnative_t sequence_Size){
	// sequence will point at the '>' in the header and sequence_Size is the
	// amount of characters including the '>' in the header and the last newline
	// of the sequence.

	uint8_t * header_Newline=(uint8_t *)memchr(sequence, '\n', sequence_Size);


	// Write the header line including the header_Newline.
	(void)!write(STDOUT_FILENO, sequence, header_Newline-sequence+1);


	// front_Character_Of_Last_Assigned_Chunk and
	// front_Character_Of_Last_Written_Chunk are initially set to point to the
	// last newline of the sequence (if the sequence was one chunk larger and
	// the first chunk was already assigned and written, then this is where the
	// front character of that chunk would have been).
	front_Character_Of_Last_Assigned_Chunk=front_Character_Of_Last_Written_Chunk
	  =sequence+sequence_Size-1;

	sequence_Characters_Left_To_Assign
	  =front_Character_Of_Last_Assigned_Chunk-header_Newline;


	// Figure out the number_Of_Threads_To_Use based on how many CPU cores are
	// available. Note that using sysconf(_SC_NPROCESSORS_ONLN) will return the
	// number of processors online on the system but not necessarily
	// available to the process (if something like `taskset` is used). Using
	// something like sched_getaffinity() with CPU_COUNT() would be more
	// accurate on systems like Linux but is less portable.
	uintnative_t number_Of_Threads_To_Use
	  =(uintnative_t)labs(sysconf(_SC_NPROCESSORS_ONLN));
	pthread_t chunks_Processing_Threads[number_Of_Threads_To_Use];

	// Create number_Of_Threads_To_Use chunks_Processing_Threads and have them
	// all process_Chunks.
	for(uintnative_t i=number_Of_Threads_To_Use; i-->0;)
		pthread_create(&chunks_Processing_Threads[i], NULL, process_Chunks
		  , NULL);

	// Wait for all the chunks_Processing_Threads to finish writing the reverse
	// complement of the sequence.
	for(uintnative_t i=number_Of_Threads_To_Use; i-->0;)
		pthread_join(chunks_Processing_Threads[i], NULL);
}


int main(void){

	// We initialize the REVERSE_COMPLEMENT_LOOKUP table here but only fill in
	// elements for characters that will be in the range of ASCII characters
	// that we can expect to see in inputted data.
	for(uintnative_t i='\n'; i<sizeof(COMPLEMENT_LOOKUP)-1; i++)
		for(uintnative_t j='\n'; j<sizeof(COMPLEMENT_LOOKUP)-1; j++)
			REVERSE_COMPLEMENT_LOOKUP[i<<8 | j]
			  =(uint16_t)COMPLEMENT_LOOKUP[j]<<8
			  | (uint16_t)COMPLEMENT_LOOKUP[i];


	// Allocate memory for the initial sequence (assuming there is one).
	uintnative_t sequence_Capacity=READ_SIZE, sequence_Size=0;
	uint8_t * sequence=(uint8_t *)malloc(sequence_Capacity);

	// Read in sequence data until we reach the end of the file or encounter an
	// error.
	for(uintnative_t bytes_Read
	  ; (bytes_Read=read(STDIN_FILENO, &sequence[sequence_Size], READ_SIZE));){

		// Search the read in chunk of data for a '>' to see if any sequences
		// are being started.
		for(uint8_t * sequence_Start; (sequence_Start
		  =(uint8_t *)memchr(&sequence[sequence_Size], '>', bytes_Read));){

			// Update the sequence_Size to reflect any data before the '>' that
			// was read in.
			uintnative_t number_Of_Preceding_Bytes
			  =sequence_Start-&sequence[sequence_Size];
			sequence_Size+=number_Of_Preceding_Bytes;


			// If there is any data for the current sequence, then process it
			// and update things for processing the next sequence.
			if(sequence_Size){

				// Process the current sequence.
				process_Sequence(sequence, sequence_Size);

				// Copy the read-in '>' and any data following it to the front
				// of sequence.
				memmove(sequence, sequence_Start
				  , bytes_Read-number_Of_Preceding_Bytes);

				// Reset sequence_Size to 0 as we start processing the next
				// sequence.
				sequence_Size=0;
			}


			// Update sequence_Size and bytes_Read to reflect the read in '>'
			// and any data that preceded it.
			sequence_Size++;
			bytes_Read-=number_Of_Preceding_Bytes+1;
		}


		// Update sequence_Size to reflect the bytes that were read in.
		sequence_Size+=bytes_Read;

		// If there potentially isn't enough free space for all the data from
		// the next read, then double the capacity of the sequence.
		if(sequence_Size>sequence_Capacity-READ_SIZE)
			sequence=(uint8_t *)realloc(sequence, sequence_Capacity*=2);
	}


	// If there is any data for a last sequence, process it.
	if(sequence_Size)
		process_Sequence(sequence, sequence_Size);

	free(sequence);
	pthread_cond_destroy(&write_Finished_Condition);
	pthread_mutex_destroy(&shared_Variables_Mutex);
}
