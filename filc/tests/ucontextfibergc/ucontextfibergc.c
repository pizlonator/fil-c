#define _GNU_SOURCE
#include <stdbool.h>
#include <stdfil.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ucontext.h>

#define NUM_FIBERS 6
#define NUM_NODES  1500
#define ROUNDS     4

typedef struct node {
    struct node *next;
    char *label;
} node_t;

typedef struct {
    ucontext_t ctx;
    bool done;
} fiber_slot_t;

static ucontext_t main_ctx;
static fiber_slot_t fibers[NUM_FIBERS];
static int cur_fiber = 0;
static int switch_ticks = 0;

static void switch_next(void)
{
    int prev = cur_fiber;
    for (int step = 1; step < NUM_FIBERS; ++step) {
        int cand = (prev + step) % NUM_FIBERS;
        if (!fibers[cand].done) {
            cur_fiber = cand;
            ZASSERT(swapcontext(&fibers[prev].ctx, &fibers[cand].ctx) == 0);
            ZASSERT(cur_fiber == prev);
            return;
        }
    }
    if (!fibers[prev].done)
        return;
    setcontext(&main_ctx);
    ZASSERT(!"setcontext returned");
}

static void maybe_switch(void)
{
    if (++switch_ticks >= 31) {
        switch_ticks = 0;
        switch_next();
    }
}

static void add_node(node_t **head)
{
    node_t *n = (node_t *)malloc(sizeof(node_t));
    ZASSERT(n != NULL);
    n->next = *head;
    maybe_switch();
    char *lbl = (char *)malloc(48);
    ZASSERT(lbl != NULL);
    snprintf(lbl, 48, "node=%p", (void *)n);
    n->label = lbl;
    *head = n;
}

static void verify_list(node_t *head)
{
    char expect[48];
    for (node_t *n = head; n; n = n->next) {
        snprintf(expect, sizeof(expect), "node=%p", (void *)n);
        maybe_switch();
        ZASSERT(strcmp(n->label, expect) == 0);
    }
}

static void remove_half(node_t **head)
{
    node_t **pp = head;
    int idx = 0;
    while (*pp) {
        maybe_switch();
        if ((idx & 1) != 0)
            pp = &(*pp)->next;
        else
            *pp = (*pp)->next;
        ++idx;
    }
}

static void fiber_worker(void)
{
    node_t *head = NULL;
    for (int i = 0; i < NUM_NODES; ++i)
        add_node(&head);
    verify_list(head);
    for (int r = 0; r < ROUNDS; ++r) {
        remove_half(&head);
        verify_list(head);
        for (int i = 0; i < NUM_NODES / 2; ++i)
            add_node(&head);
        verify_list(head);
    }
    fibers[cur_fiber].done = true;
    switch_next();
    ZASSERT(!"unreachable");
}

static ucontext_t link_parent_ctx;
static ucontext_t link_child_ctx;
static int link_arg_sum = 0;

static void link_child(int a, int b)
{
    link_arg_sum = a + b;
}

int main(void)
{
    /* 1. Multi-fiber swapcontext under allocation and GC pressure. */
    for (int i = 0; i < NUM_FIBERS; ++i) {
        fibers[i].done = false;
        ZASSERT(getcontext(&fibers[i].ctx) == 0);
        fibers[i].ctx.uc_stack.ss_size = 65536;
        fibers[i].ctx.uc_link = NULL;
        makecontext(&fibers[i].ctx, fiber_worker, 0);
    }
    cur_fiber = 0;
    ZASSERT(swapcontext(&main_ctx, &fibers[0].ctx) == 0);
    for (int i = 0; i < NUM_FIBERS; ++i)
        ZASSERT(fibers[i].done);

    /* 2. makecontext argument passing and uc_link return. */
    ZASSERT(getcontext(&link_child_ctx) == 0);
    link_child_ctx.uc_stack.ss_size = 32768;
    link_child_ctx.uc_link = &link_parent_ctx;
    makecontext(&link_child_ctx, (void (*)())link_child, 2, 19, 23);
    ZASSERT(swapcontext(&link_parent_ctx, &link_child_ctx) == 0);
    ZASSERT(link_arg_sum == 42);

    printf("ucontext fiber gc ok\n");
    return 0;
}
