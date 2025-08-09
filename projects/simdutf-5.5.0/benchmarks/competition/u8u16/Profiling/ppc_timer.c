#define cycle_counter_units "tbu"

__inline__ unsigned long long int read_cycle_counter () {
  union {long long int ts_long_long;
         struct {int upper; 
                 int lower;
	 } ts_struct;
  } ts;
  int ts_upper1;
  do {
    asm volatile ("mftbu %0\n\t"
                  "mftb %1\n\t"
                  "mftbu %2\n\t"
                  : "=r"(ts_upper1), "=r"(ts.ts_struct.lower), "=r"(ts.ts_struct.upper));
  } while (ts.ts_struct.upper != ts_upper1);
  return(ts.ts_long_long);
}


__inline__ int binary_order_of_magnitude(unsigned long long int ts) {
  union {long long int ts_long_long;
         struct {int upper; 
                 int lower;
	 } ts_struct;
  } ts_s;
  ts_s.ts_long_long = ts;
  int leading_zeroes;
  asm volatile("cntlzw %0, %1\n" : "=r" (leading_zeroes) : "r" (ts_s.ts_struct.upper));
  if (leading_zeroes == 32) {
    asm volatile("cntlzw %0, %1\n" : "=r" (leading_zeroes) : "r" (ts_s.ts_struct.lower));
    return 32 - leading_zeroes;
  }
  return 64 - leading_zeroes;
}
