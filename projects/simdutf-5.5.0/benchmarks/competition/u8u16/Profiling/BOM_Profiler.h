/*
    BOM_Profiler.h - Binary Order of Magnitude Execution Time Profiler 
    Copyright (C) 2010  Robert D. Cameron and Dan Lin
    Version 0.4
    Licensed to the general public under Academic Free License version 3.0

BOM_Profiler provides a lightweight multiplatform execution time profiling
utility based on processor cycle counters.  It uses a binary logarithmic
histogram technique that is useful in both highlighting patterns of 
cycle time distributions as well as identifing outliers in timing events
due to interruptions for operating system services.

In essence, BOM profiler is designed to collect statistics over a number
of repetitions of particular timed events.  Statistics are gathered in the
form of a logarithmic histogram of cycle times for processing a fixed number 
of elements between calls to start_BOM_interval and end_BOM_interval.  
For example, an interval may correspond to processing 1024 single-byte
elements by a particular routine.

A timer t is created and initialized by a call to t = init_BOM_timer().  
Given a timer t, start_BOM_interval(t) initiates an interval measurement 
which completes with end_BOM_interval(t, n), where n is the number of 
elements processed.  

dump_BOM_table(t) prints out a rudimentary histogram of the recorded
intervals for a particular timer.  

Although the basic concept of BOM_Profiler is architecture independent,
processor-dependent routines for accessing time-stamp counters and
determining the binary order of magnitude are included through external
files.


*/

#ifndef BOM_Profiler_H
#define BOM_Profiler_H


#include "i386_timer.h"

#define BIT_COUNT 64
#define MAX_TIMER_ENTRIES (1<<18)
#define TIMER_SCALE_FACTOR 1000

struct BOM_Table {
  // current timing interval information
  int timer_on;
  int full;
  timestamp_t interval_start[MAX_TIMER_ENTRIES];
  timestamp_t interval_end[MAX_TIMER_ENTRIES];
  unsigned int interval_elems[MAX_TIMER_ENTRIES];  
  unsigned int current_entry;
};

typedef struct BOM_Table BOM_Table;

BOM_Table * init_BOM_timer () {
  BOM_Table * timer_table = (BOM_Table *) malloc(sizeof(BOM_Table));
  if (!timer_table) {
    printf("Couldn't initialize BOM timer.\n");
    exit(-1);
  }
  timer_table -> current_entry = 0;
  timer_table -> full = 0;
  timer_table -> timer_on = 0;
  return timer_table;
}


static inline void start_BOM_interval(BOM_Table * timer_table) {
  timer_table->timer_on = 1;
  timer_table->interval_start[timer_table->current_entry] = read_cycle_counter();
}

static inline void end_BOM_interval(BOM_Table * timer_table, unsigned int elems) {
  if (timer_table->timer_on) {
    timer_table->interval_end[timer_table->current_entry] = read_cycle_counter();
    timer_table->interval_elems[timer_table->current_entry] = elems;
    timer_table->current_entry++;
    if(timer_table->current_entry >= MAX_TIMER_ENTRIES) {
      timer_table->full=1;
      timer_table->current_entry=0;
    }
    timer_table->timer_on = 0;
  }
  else
  	fprintf(stderr,"Time interval end without a start!\n"); 
}

void dump_BOM_table(BOM_Table * timer_table) {
  // an array of counts and timings per binary order of magnitude
  int BOM_count[BIT_COUNT];
  unsigned int BOM_elems[BIT_COUNT];  
  timestamp_t BOM_total_time[BIT_COUNT];

  int BOM, b;
  int this_count;
  int cum_count = 0;
  unsigned int entry = 0;
  unsigned int total_entries;
  unsigned int this_elems = 0;
  unsigned int cum_elems = 0;
  timestamp_t this_time;
  timestamp_t cum_time = 0ULL;
  timestamp_t this_avg;
  timestamp_t cum_avg;
  timestamp_t accumulated_cycles;  

  for (b = 0; b < BIT_COUNT; b++) {
	BOM_count[b] = 0;
	BOM_elems[b] = 0;
	BOM_total_time[b] = 0;
  }


  total_entries = timer_table->full ? MAX_TIMER_ENTRIES : timer_table->current_entry;
  for(entry = 0; entry < total_entries; entry++){
  	accumulated_cycles = timer_table->interval_end[entry] - timer_table->interval_start[entry];
  	if (accumulated_cycles > 0ULL) {
	    BOM = binary_order_of_magnitude(accumulated_cycles*TIMER_SCALE_FACTOR/timer_table->interval_elems[entry]);
	    BOM_count[BOM]++;
	    BOM_elems[BOM] += timer_table->interval_elems[entry];
	    BOM_total_time[BOM] += accumulated_cycles;
	}
  }
  
  printf("Binary Order of Magnitude Profile\n");
  for (b = 0; b < BIT_COUNT; b++) {
    this_count = BOM_count[b];
    cum_count += this_count;
    this_time = BOM_total_time[b];
    cum_time += this_time;
    this_elems = BOM_elems[b];
    cum_elems += this_elems;
    if (this_count == 0) this_avg = 0ULL;
    else this_avg = (TIMER_SCALE_FACTOR*this_time)/(this_elems);
    if (cum_count == 0) cum_avg = 0ULL;
    else cum_avg = (TIMER_SCALE_FACTOR*cum_time)/(cum_elems);
    if (this_count > 0) {  // Only report intervals with nonzero counts.
      printf("BOM %i: %i ", b, this_count);
      printf("(avg time: %i %s/kElem) ", (int) this_avg, cycle_counter_units);
      printf("Cumulative: %i ", cum_count);
      printf("(avg: %i %s/kElem)\n", (int) cum_avg, cycle_counter_units);
    }
  }           
}
#endif
