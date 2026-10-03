#include "pas_config.h"

#if LIBPAS_ENABLED

#include "pas_flex_large_free_heap.h"

#include "pas_heap_lock.h"
#include "pas_utility_heap.h"

static pas_red_black_tree_jettisoned_nodes dummy_jettisoned_nodes;

static int node_compare_callback(pas_red_black_tree_node* left_node,
                                 pas_red_black_tree_node* right_node)
{
    pas_flex_large_free_node* left;
    pas_flex_large_free_node* right;

    left = (pas_flex_large_free_node*)left_node;
    right = (pas_flex_large_free_node*)right_node;

    if (pas_range_size(left->range) < pas_range_size(right->range))
        return -1;
    if (pas_range_size(left->range) == pas_range_size(right->range))
        return 0;
    return 1;
}

static int node_to_size_compare_callback(pas_red_black_tree_node* left_node,
                                         void* encoded_right_size)
{
    pas_flex_large_free_node* left;
    size_t right_size;

    left = (pas_flex_large_free_node*)left_node;
    right_size = (size_t)encoded_right_size;

    if (pas_range_size(left->range) < right_size)
        return -1;
    if (pas_range_size(left->range) == right_size)
        return 0;
    return 1;
}

void pas_flex_large_free_heap_construct(pas_flex_large_free_heap* heap)
{
    pas_red_black_tree_construct(&heap->tree);
}

pas_range pas_flex_large_free_heap_try_allocate(pas_flex_large_free_heap* heap,
                                                size_t size,
                                                size_t alignment,
                                                pas_large_free_heap_config* config)
{
    static const bool verbose = false;
    
    pas_red_black_tree_node* node;
    pas_aligned_allocation_result aligned_result;
    pas_saved_float_environment saved_float_environment;
    bool slop_says_fail;

    pas_heap_lock_assert_held();

    PAS_ASSERT(pas_is_aligned(size, alignment));
    PAS_ASSERT(alignment >= config->min_alignment);

    if (verbose)
        pas_log("Here! size = %zu\n", size);

    for (node = pas_red_black_tree_find_least_greater_than_or_equal(
             &heap->tree, (void*)size, node_to_size_compare_callback);
         node;
         node = pas_red_black_tree_node_successor(node)) {
        pas_flex_large_free_node* flex_node;

        flex_node = (pas_flex_large_free_node*)node;
        PAS_ASSERT(pas_range_size(flex_node->range) >= size);

        /* The slop heuristic uses floating-point division, which usually raises FE_INEXACT,
           and which would trap if the program unmasked floating-point exceptions.  Our
           heuristics must be invisible to the program, so canonicalize the floating-point
           environment for its sake and restore the program's environment afterwards. */
        pas_save_float_environment(&saved_float_environment);
        pas_set_float_environment_to_libpas_default();

        slop_says_fail =
            (double)pas_range_size(flex_node->range) / (double)size > PAS_MAX_FLEX_HEAP_SLOP;

        pas_restore_float_environment(&saved_float_environment);

        if (slop_says_fail)
            break;

        if (pas_is_aligned(flex_node->range.begin, alignment)) {
            pas_range result;

            /* We have a winner. */
            result = flex_node->range;
            
            pas_red_black_tree_remove(&heap->tree, node, &dummy_jettisoned_nodes);
            pas_utility_heap_deallocate(node);

            return result;
        }
    }

    aligned_result = config->aligned_allocator(
        size, pas_alignment_create_traditional(alignment), config->aligned_allocator_arg);
    if (!aligned_result.result)
        return pas_range_create_empty();

    /* NOTE: If the aligned allocator did happen to give us left slop, we'd have no choice but to
       drop it on the floor. Also, none of the aligned allocators we have to use would do that to
       us. On the other hand, if some aligned allocator ever violated this assert, it would not be
       the end of the world; it just a bit of one-time wasted memory virtual memory. */
    PAS_ASSERT(!aligned_result.left_padding_size);

    /* This should just always be true. */
    PAS_ASSERT((uintptr_t)aligned_result.result + aligned_result.result_size
               == (uintptr_t)aligned_result.right_padding);

    /* But there shouldn't be alignment padding at all, since we passed an aligned size. */
    PAS_ASSERT(!aligned_result.right_padding_size);

    /* And the size we got should be exactly the size we asked for. */
    PAS_ASSERT(aligned_result.result_size == size);

    return pas_range_create((uintptr_t)aligned_result.result, (uintptr_t)aligned_result.result + size);
}

void pas_flex_large_free_heap_deallocate(pas_flex_large_free_heap* heap,
                                         pas_range range)
{
    pas_flex_large_free_node* node;
    
    node = (pas_flex_large_free_node*)pas_utility_heap_allocate(
        sizeof(pas_flex_large_free_node), "pas_flex_large_free_node");

    node->range = range;
    pas_red_black_tree_insert(&heap->tree, &node->node, node_compare_callback, &dummy_jettisoned_nodes);
}

void pas_flex_large_free_heap_for_each_free(pas_flex_large_free_heap* heap,
                                            pas_large_free_visitor visitor,
                                            void* arg)
{
    pas_red_black_tree_node* node;

    for (node = pas_red_black_tree_minimum(&heap->tree);
         node;
         node = pas_red_black_tree_node_successor(node)) {
        pas_flex_large_free_node* flex_node;
        pas_large_free large_free;

        flex_node = (pas_flex_large_free_node*)node;

        large_free.begin = flex_node->range.begin;
        large_free.end = flex_node->range.end;
        large_free.offset_in_type = 0;
        large_free.zero_mode = pas_zero_mode_may_have_non_zero;
        visitor(large_free, arg);
    }
}

size_t pas_flex_large_free_heap_get_num_free_bytes(pas_flex_large_free_heap* heap)
{
    pas_red_black_tree_node* node;
    size_t result;

    result = 0;
    
    for (node = pas_red_black_tree_minimum(&heap->tree);
         node;
         node = pas_red_black_tree_node_successor(node)) {
        pas_flex_large_free_node* flex_node;

        flex_node = (pas_flex_large_free_node*)node;
        result += pas_range_size(flex_node->range);
    }

    return result;
}

#endif /* LIBPAS_ENABLED */

