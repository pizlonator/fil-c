#include <stdbool.h>
#include "common.h"

typedef struct {
   int i;
} MyStruct;

// EXPECT:6: Warning: Test: symbol='MyStruct': Unknown namespace for identifier 'MyStruct'

typedef enum {
  TEST_MY_ENUM_A = 0
} MyEnum;

// EXPECT:12: Warning: Test: symbol='MyEnum': Unknown namespace for identifier 'MyEnum'

typedef enum {
  MY_ENUM_A = 0
} TestMyEnum2;

// EXPECT:18: Warning: Test: symbol='TestMyEnum2': Unknown namespace for symbol 'MY_ENUM_A'

void test_bool3 (gboolean b);

void test_f16 (_Float16 f);
// EXPECT:24: Warning: Test: test_f16: argument f: Unresolved type: '_Float16'

void test_f32 (_Float32 f);
// EXPECT:27: Warning: Test: test_f32: argument f: Unresolved type: '_Float32'

void test_f64 (_Float64 f);
// EXPECT:30: Warning: Test: test_f64: argument f: Unresolved type: '_Float64'

void test_f128 (_Float128 f);
// EXPECT:33: Warning: Test: test_f128: argument f: Unresolved type: '_Float128'

void test_f32x (_Float32x f);
// EXPECT:36: Warning: Test: test_f32x: argument f: Unresolved type: '_Float32x'

void test_f64x (_Float64x f);
// EXPECT:39: Warning: Test: test_f64x: argument f: Unresolved type: '_Float64x'

void test_f128x (_Float128x f);
// EXPECT:42: Warning: Test: test_f128x: argument f: Unresolved type: '_Float128x'

// For backward compatibility we don't warn about _Bool
_Bool test_ret_bool1 ();
bool test_ret_bool2 ();
gboolean test_ret_bool3 ();
void test_bool2 (bool b);
void test_bool1 (_Bool b);
