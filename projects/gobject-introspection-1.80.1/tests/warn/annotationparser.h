#include "common.h"

/**
 * test_symbol_thrice_documented:
 *
 * Documenting the same thing multiple times can lead to subtle bugs.
 * For example, one comment block might have correct annotations...
 **/
void test_symbol_thrice_documented();

/**
 * test_symbol_thrice_documented:
 *
 * ...and a different comment block (out of sync with the above) might have
 * no annotations at all. The last comment block seen by the parser "wins"...
 **/

// EXPECT:11: Warning: Test: multiple comment blocks documenting 'test_symbol_thrice_documented:' identifier (already seen at annotationparser.h:3).


/**
 * test_symbol_thrice_documented:
 *
 * ...and yet another one.
 **/

// EXPECT:21: Warning: Test: multiple comment blocks documenting 'test_symbol_thrice_documented:' identifier (already seen at annotationparser.h:11).
