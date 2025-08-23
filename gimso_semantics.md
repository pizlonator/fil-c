Fil-C achieves memory safety by introducing a *garbage in, memory safety out* (GIMSO) pass to LLVM. This pass ascribes a memory safe semantics to the incoming LLVM IR so that given any possible LLVM module (including one created adversarially), the resulting module will obey Fil-C pointer capability rules.

But what are those rules, and what are the semantics that Fil-C's pass ascribes to LLVM IR? This document describes these GIMSO semantics.

[This document shows examples of these semantics in action.](https://github.com/pizlonator/fil-c/blob/deluge/invisicaps_by_example.md)

# Pointers In Flight 

We say that a value is "in flight" when it is carried in LLVM data flow rather than when it is stored in memory. For example, if a `call` instruction's first argument operand uses a `load` instruction of `ptr` type, then this data flow edge has a "pointer in flight".

Under GIMSO, pointers in flight carry two separate pieces of information:

- The pointer's integer value (intval). Under normal LLVM semantics, this is *all* that the a pointer in flight would carry. This could be any integer value whose bit width matches the module data layout's pointer size. *We do not trust this value and it may be garbage or adversarial.*

- The pointer's capability. This is a pointer as well, but it is not forgeable by the the LLVM IR that uses pointers. The capability pointer points to a garbage collected object. Three kinds of capabilities are possible: the null capability is for pointers that cannot be dereferenced at all, plain capabilities have a lower and upper bounds that tell the range of addresses that can be accessed using that capability, and special capabilities are for things like function pointers (though other special types are also recognized, and more may be added in the future). Plain capabilities may monotonically transition from having their original bounds to having lower = upper; this happens when objects get freed. Other than the free operation, there's no operation that can change the lower or upper bounds. Capabilities must be pointers to objects, as opposed to being carried around "in place" to enable aliasing of the capability between all pointers that use that capability. If two pointers have the same capability and one of them is passed to the `free()` function, then both pointers will now see a freed capability (lower and upper bounds will be equal). The capability objects are not visible to LLVM IR. One way that capabilities might be implementated is if there was a totally separate address space in which the capability objects are allocated. But a simpler option exists (and what Fil-C actually does): the capability objects are at addresses that are outside of any capability's lower/upper bounds. So, it's never possible for LLVM IR to load or store to the capability objects.

It's possible to change the pointer's integer value using a variety of LLVM IR operations. On the other hand, capabilities may be created at allocation time, or they may be freed, but otherwise they just travel with pointers.

It's legal to access N bytes (with a load, store, or atomic operation) in a flight pointer P if:

- `P.intval >= P.capability->lower`

- `P.intval < P.capabiltiy->upper`

- `P.intval + N <= P.capability->upper`

Illegal accesses are guaranteed to cause a Fil-C safety error. Legal accesses are guaranteed to succeed.

Accesses that LLVM claims to be aligned are either checked for alignment, or have their alignment
annotation dropped. On X86-64, it's common for C programs to perform integer and float accesses that
are not aligned, so Fil-C drops the alignment annotation for those. But vector and pointer accesses have their alignment preserved. For those accesses where alignment is preserved, Fil-C checks that the alignment holds: the access is only legal if `P mod A = 0`.

# Capability Allocation

Capabilities can only be created by allocation. Allocations happen via libc, such as by calling `malloc`, via the LLVM IR `alloca` instruction, and LLVM global variables.

Creating a capability allocates both the capability object itself and the *payload* object. The payload is the memory that the user code gets to see with loads and stores. The capability's lower/upper bounds tell where the payload object is and how large it is.

New capabilities will choose a range of memory for the capability itself and for the payload that is free according to the garbage collector. So, a newly created capability never overlaps with any reachable capability.

New capabilities have their payload initialized to zero.

# Pointers At Rest

We say that a value is "at rest" when it is stored in memory.

Under GIMSO, pointers at rest use an *invisible capability* (InvisiCap). The pointer's intval is stored within the object's payload, while the capability pointer is stored at an invisible location.

It's easiest to understand the semantics by thinking of the capability pointers of pointers at rest living in a shadow address space. That's not how Fil-C implements it because Fil-C's OS interface does not rely on large memory reservations and the implementation does not require support for overcommit. However, the semantics are "as if" there is a shadow address space. So for the purpose of this document, the abstract machine has a primary address space and a shadow address space. All non-pointer-type accesses operate only on the primary address space. Pointer-type accesses (i.e. memory accesses whose access type is `ptr`) operate on both address spaces at once.

New capabilities initialize the corresponding shadow addresses to the null capability.

Non-pointer accesses operate only on the primary address space.

Pointer-type accesses are required to have pointer alignment (so 8 on 64-bit platforms, 4 on 32-bit platforms, and so forth).

## Simplified Rules, Ignoring Atomics

If we ignore the need for pointer atomics, pointers at rest are quite simple. Pointers at rest have their intval stored in the primary address space and their capability pointer stored in the secondary address space. We do not allow any other user-defined or hardware-defined address spaces (the address space number on all pointers must be 0; note that we could devise a Fil-C semantics for safe pointers to other address spaces, but I haven't done that, because I haven't found a need for it yet).

This permits pointer-integer aliasing as follows. Storing a pointer to a location and then loading it as an integer is like a `ptrtoint` cast. Storing a pointer to a location and then storing an integer to that same location, and then loading a pointer, yields a pointer with the capability of the pointer store and an intval from the integer store. Loading a pointer from a location that never had a pointer stored to it yields a pointer with a null capability.

This arrangement also prevents races from ever corrupting the capability. The capability pointer stores and loads always happen using LLVM `monotonic` atomic ordering. Hence, although the intval and capability may tear in a race (you may get an intval from one pointer and a capability from another), the result is a pointer that is illegal to access and always traps. No memory-unsafe outcome is possible from racing on non-atomic pointers.

## Complete Rules, Including Atomics

Fil-C allows atomic pointer accesses, where the intval and capability do not tear. This is achieved by giving the secondary address space an extra power: locations in the shadow address space may be in *non-atomic* or *atomic* mode. The *non-atomic* mode is the default. Upon allocation, the entire payload's shadow memory is put in *non-atomic* mode. Performing an atomic store of pointer type switches the shadow location to *atomic* mode.

If the shadow location is in *non-atomic* mode, then the pointer stored at that address has its intval stored in the primary address space and its capability stored in the shadow address space (as described in the previous section). This means that it's possible to alias pointers and integers, and it's possible to have intval-capability tearing.

If the shadow location is in *atomic* mode, then the pointer stored at that address has its intval and capability stored atomically in the shadow address space and the intval is replicated to the primary address space. In other words, the shadow address space behaves as if it has twice as many bits as the primary one, so that for any pointer address, it's possible to store both the intval and the capability at that location in the shadow space.

The behavior of the *atomic* mode is strange, but preserves pointer atomicity for atomic pointer accesses, preserves memory safety for non-atomic pointer accesses, allows aliasing between pointers and integers so long as the accesses are not atomic, and guarantees lock-freedom of atomic pointer accesses. An example of how a program might observe the strangeness is the following sequence:

1. Atomically store pointer V to the address P.

2. Store integer W to the address P.

3. Load a pointer from P.

4. Load an integer from P.

In this case, the load in (3) will see the value V and the load in (4) will see the value W. Note that if the store in (1) had not been atomic, then the load in (3) would see a pointer with V's capability but W as the intval.

# Basic Operations

Let's first consider these basic operations: load, store, gep (aka getelementptr), ptrtoint, and inttoptr.

## Load

The LLVM `load` operation has the following two syntaxes:

    <result> = load [volatile] <ty>, ptr <pointer>[, align <alignment>]
    <result> = load atomic [volatile] <ty>, ptr <pointer> [syncscope("<target-scope>")] <ordering>, align <alignment>

Note that the `<ty>` can be a compound type, like a struct that has both pointers and non-pointers. Compound types are only possible for non-atomic loads. The first step to understanding GIMSO `load` semantics is to decompose compound type loads *where the compound type has pointers* into loads of each individual element of the compound value, and then reconstruct the compount value after doing all of the loads. For example, this:

    %val = load {i32, ptr}, ptr %ptr

becomes:

    %elem1 = load i32, ptr %ptr
    %ptr2 = getelementptr {i32, ptr}, %ptr, i32 0, i32 1
    %elem2 = load ptr, ptr %ptr2
    %agg1 = insertvalue {i32, ptr} zeroinitializer, %elem1, 0
    %val = insertvalue {i32, ptr} %agg1, %elem2, 1

On the other hand, we don't do this decomposition for compound types that don't have pointers. For example:

    %val = load {i32, i32}, ptr %ptr

is kept as-is.

Once all loads are decomposed (possibly recursively, if the compound type is deep), then each load is implemented as follows. In this description, we use P to refer to the pointer being loaded from and N is the size of the type being loaded. All loads start with mandatory checks. Check failure leads to a Fil-C safety error.

1. If the access type requires alignment according to Fil-C's rules for that target, then we check that `P.intval mod A = 0`, where A is the alignment requirement of the load. Pointers and vectors require an alignment check. Integers and floats don't on X86, but may require alignment on other platforms. A compound type requires alignment if any member requires alignment.

2. `P.capability` must be a plain capability (cannot be a null or special capability).

3. `P.intval >= P.capability->lower`

4. `P.intval < P.capabiltiy->upper`

5. `P.intval + N <= P.capability->upper`

Then the load is executed; this is a bit different depending on the type.

### Int Loads

*For simplicity, we refer to all non-pointer types as "ints". This includes floats, vectors and arrays of ints and floats, structs of ints and floats, etc.*

Int loads simpliy load using `P.intval` as the address to load from, while keeping the original LLVM flags on the load (atomic/volatile/ordering/etc). If the load was checked for alignment, then the alignment annotation is preserved; otherwise the alignment annotation is set to 1. 

### Non-Atomic Ptr Loads

Non-atomic ptr loads access the capability from the shadow space or the atomic box and the intval from the primary space.

Pseudocode:

    CapabilityOrAtomicBox = LoadFromShadowSpace(P.intval)
    Intval = LoadFromPrimarySpace(P.intval)
    if (CapabilityOrAtomicBox is AtomicBox)
        return MakePointer(capability = LoadCapabilityFromAtomicBox(CapabilityOrAtomicBox),
                           intval = Intval)
    return MakePointer(capability = CapabilityOrAtomicBox, intval = Intval)

Note that `LoadFromShadowSpace` uses the LLVM `monotonic` atomic ordering.

### Atomic Ptr Loads

If an atomic ptr load encounters a capability in the shadow space, then the loaded pointer uses the capability from shadow space and the intval from the primary space.

If an atomic ptr load encounters an atomic box in the shadow space, then the loaded pointer uses the capability and intval from the atomic box.

Pseudocode:

    CapabilityOrAtomicBox = LoadFromShadowSpace(P.intval)
    if (CapabilityOrAtomicBox is AtomicBox)
        return LoadPointerFromAtomixBox(CapabilityOrAtomicBox)
    return MakePointer(capability = CapabilityOrAtomicBox,
                       intval = LoadFromPrimarySpace(P.intval))

Note that the atomic versus non-atomic behavior of shadow space depends on whether the location in shadow space contains a capability or an atomic box.

## Store

The LLVM `store` operation has the following two syntaxes:

    store [volatile] <ty> <value>, ptr <pointer>[, align <alignment>]
    store atomic [volatile] <ty> <value>, ptr <pointer> [syncscope("<target-scope>")] <ordering>, align <alignment>

Note that the `<ty>` can be a compound type, like a struct that has both pointers and non-pointers. Compound types are only possible for non-atomic stores. The first step to understanding GIMSO `store` semantics is to decompose compound type stores *where the compound type has pointers* into stores of each individual element of the compound value. For example, this:

    store {i32, ptr} %val, ptr %ptr

becomes:

    %elem1 = extractvalue {i32, ptr} %val, 0
    store i32 %elem1, ptr %ptr
    %elem2 = extractvalue {i32, ptr} %val, 1
    %ptr2 = getelementptr {i32, ptr}, %ptr, i32 0, i32 1
    store ptr %elem2, ptr %ptr2

On the other hand, we don'd do this decomposition for compound types that don't have pointers. For example:

    store {i32, i32} %val, ptr %ptr

is kept as-is.

Once all stores are decomposed (possibly recursively, if the compound type is deep), then each store is implemented as follows. In this description, we use P to refer to the pointer being stored to, V to refer to the value being stored, and N is the size of the type being stored. All stores start with mandatory checks. Check failure leads to a Fil-C safety error.

1. If the access type requires alignment according to Fil-C's rules for that target, then we check that `P.intval mod A = 0`, where A is the alignment requirement of the store. Pointers and vectors require an alignment check. Integers and floats don't on X86, but may require alignment on other platforms. A compound type requires alignment if any member requires alignment.

2. `P.capability` must be a plain capability (cannot be a null or special capability).

3. `P.intval >= P.capability->lower`

4. `P.intval < P.capabiltiy->upper`

5. `P.intval + N <= P.capability->upper`

Then the store is executed; this is a bit different depending on the type.

### Int Stores

Int stores simply store using `P.intval` as the address to store to, while keeping the original LLVM flags on the store (atomic/volatile/ordering/etc). If the store was checked for alignment, then the alignment annotation is preserved; otherwise the alignment annotation is set to 1.

### Non-Atomic Ptr Stores

Non-atomic ptr stores put the intval into the primary space and the capability in the shadow space.

Pseudocode:

    StoreToPrimarySpace(P.intval, V.intval)
    StoreToShadowSpace(P.intval, V.capability)

Note that this makes the shadow space location have non-atomic mode, since future loads will see a capability at that location rather than an atomic box.

Note that `StoreToShadowSpace` uses the LLVM `monotonic` atomic ordering.

### Atomic Ptr Stores

Atomic ptr stores use an atomic box to store both intval and capability atomically. Semantically, this is as if a new atomic box was created on each atomic ptr store; however, it's possible to optimize this to reuse atomic boxes on repeated accesses to the same location.

Additionally, the intval is replicated to the primary space for the benefit of non-atomic loads.

Pseudocode:

    StoreToShadowSpace(P.intval, MakeAtomicBox(capability = V.capability, intval = V.intval))
    StoreToPrimarySpace(P.intval, V.intval)

Note that this makes the shadow space location have atomic mode, since future laods will see an atomic box at that location rather than a capability.

## Gep

The LLVM `getelementptr` instruction, or *gep* for short, has the following syntax:

    <result> = getelementptr [UB flags] <ty>, ptr <ptrval>{, <ty> <idx>}*

GIMSO means dropping any UB flags from the gep (so inbounds, nusw, nuw, and inrange are all deleted).

The gep is really just pointer arithmetic; the effect of all of the indices passed to the gep is that a DataLayout-dependent integer value is computed, which we will call the `addend`. Then the `addend` is added to the incoming `ptrval`. The pseudocode for GIMSO semantics are:

    result = MakePointer(capability = ptrval.capability,
                         intval = ptrval.intval + addend)

Hence, the `result` retains exactly the same capability as `ptrval`, but the intval is is changed.

## Ptrtoint

The LLVM `ptrtoint` instruction returns a pointer's integer value. The syntax is:

    <result> = ptrtoint <ty> <value> to <ty2>

Where `ty` has to be `ptr` and `ty2` has to be some integer type (we'll ignore vectors of pointers for now, but without loss of generality). Note that `ty2` may have more or less bits than the pointer (so the bits are either truncated or extended). Let's define `IntCast<t>(X)` to mean either zero extending or truncating X depending on whether `t` is larger or smaller (respectively) than X. Then the semantics in pseudocode are just:

    result = IntCast<ty2>(value.intval)

## Inttoptr

The LLVM `inttoptr` instruction creates a pointer from an integer value. This is super unsafe! Fil-C has to do special things for this instruction. The syntax is:

    <result> = inttoptr <ty> <value> to <ty2>

Where `ty` has to be an integer type and `ty2` has to be `ptr`. Then *an approximation of* the semantics in pseudocode are just:

    result = MakePointer(capability = null,
                         intval = IntCast<intptr>(value))

This means that you get a pointer, but it lacks a capability, and so cannot be accessed. The only valid operations on it are comparisons and casting back to int. But, these aren't the complete semantics. Fil-C goes to great length to make code like this work:

    int* p = ...;
    p = (int*)(((uintptr_t)p & MASK) + (stuff() ? get_offset() : 0));

Here, a pointer is cast to integer (`ptrtoint`), that integer goes through some math (which includes both control flow and effects), and then the resulting integer gets cast back to a pointer. The Fil-C compiler includes an abstract interpreter (with very simple rules) that checks if the integer being cast to a pointer came from exactly one pointer via `ptrtoint`. I'll describe it here.

The abstract domain is a mapping from SSA instructions that produce integers to inferred capabilities.

An inferred capability is either BOTTOM, Definite(C), or TOP. The Definite(C) case points to an SSA value of `ptr` type and indicates that we've inferred that to be the capability we should use for an integer.

The start state of the interpreter has:

- all `ptrtoint` instructions initialize their inferred capability to Definite(X), where X is their input operand.

- all calls, loads, atomics, comparisons, vaargs, extracts, landing pads, and FP casts that return int have their inferred capapability set to BOTTOM.

- all other integer instructions are handed off to the interpreter. The interpreter only interprets these instructions.

Then the interpreter executes all of the instructions it knows about using the following rule:

- for each input operand that is an instruction:

    - If the current instruction is a `phi` or `select`:

        - if the input's inferred capability is not bottom, and the current instruction's inferred capability is bottom, then create a `phi` or `select` right next to the current instruction that picks the capability. the current instruction's inferred capability is set to Definite(P), where P is the new `phi` or `select` that we created. Note that this means that `phi` and `select` never go to TOP.

        - else, *merge* the input's inferred capability into the current instruction's inferred capability.

The interpreter stops when none of these rules changes any inferred capabilities.

The merge rule (merge X into Y) is:

   1. if X is BOTTOM, do nothing.

   2. if X == Y, do nothing.

   3. if Y is BOTTOM, set Y to X.

   4. if Y is Definite(C), set it to TOP. (Note that this rule rules after the *if X == &, do nothing* rule, hence merging Definite(C) into Definite(C) results in Definite(C).)

   5. if Y is TOP, do nothing.

After running this interpreter, any `inttoptr`'s whose input `value` has an inferred capability Definite(`cap`) get executed with the following pseudocode

    result = MakePointer(capability = cap.capability,
                         intval = IntCast<intptr>(value))

Note that BOTTOM or TOP inferred capabilities still get the original treatment (i.e. a pointer with a null capability).
