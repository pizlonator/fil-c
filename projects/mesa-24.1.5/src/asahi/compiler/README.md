# Special registers

`r0l` is the hardware nesting counter.

`r1` is the hardware link register.

`r5` and `r6` are preloaded in vertex shaders to the vertex ID and instance ID.

# ABI

The following section describes the ABI used by non-monolithic programs.

## Vertex

Registers have the following layout at the beginning of the vertex shader
(written by the vertex prolog):

* `r0-r4` and `r7` undefined. This avoids preloading into the nesting counter or
  having unaligned values. The prolog is free to use these registers as
  temporaries.
* `r5-r6` retain their usual meanings, even if the vertex shader is running as a
  hardware compute shader. This allows software index fetch code to run in the
  prolog without contaminating the main shader key.
* `r8` onwards contains 128-bit uniform vectors for each attribute.
  Accommodates 30 attributes without spilling, exceeding the 16 attribute API
  minimum. For 32 attributes, we will need to use function calls or the stack.

One useful property is that the GPR usage of the combined program is equal to
the GPR usage of the main shader. The prolog cannot write higher registers than
read by the main shader.

Vertex prologs do not have any uniform registers allocated for preamble
optimization or constant promotion, as this adds complexity without any
legitimate use case.

For a vertex shader reading $n$ attributes, the following layout is used:

* The first $n$ 64-bit uniforms are the base addresses of each attribute.
* The next $n$ 32-bit uniforms are the associated clamps (sizes). Presently
  robustness is always used.
* The next 32-bit uniform is the base instance. This must always be reserved
  because it is unknown at vertex shader compile-time whether any attribute will
  use instancing.
* For a hardware compute shader, the next 32-bit uniform is the base/first
  vertex.
* For a hardware compute shader, the next 64-bit uniform is a pointer to the
  input assembly buffer.

In total, the first $6n + 2$ 16-bit uniform slots are reserved for a hardware
vertex shader, or $6n + 8$ for a hardware compute shader.

## Fragment

When sample shading is enabled in a non-monolithic fragment shader, the fragment
shader has the following register inputs:

* `r0l = 0`. This is the hardware nesting counter.
* `r0h` is the mask of samples currently being shaded. This usually equals to
  `1 << sample ID`, for "true" per-sample shading.

When sample shading is disabled, no register inputs are defined. The fragment
prolog (if present) may clobber whatever registers it pleases.

Registers have the following layout at the end of the fragment shader (read by
the fragment epilog):

* `r0l = 0` if sample shading is enabled. This is implicitly true.
* `r0h` preserved if sample shading is enabled.
* `r2` and `r3l` contain the emitted depth/stencil respectively, if
  depth and/or stencil are written by the fragment shader. Depth/stencil writes
  must be deferred to the epilog for correctness when the epilog can discard
  (i.e. when alpha-to-coverage is enabled).
* The vec4 of 32-bit registers beginning at `r(4 * (i + 1))` contains the colour
  output for render target `i`. When dual source blending is enabled, there is
  only a single render target and the dual source colour is treated as the
  second render target (registers r8-r11).

Uniform registers have the following layout:

* u0_u1: 64-bit render target texture heap
* u2...u5: Blend constant
* u6_u7: Root descriptor, so we can fetch the 64-bit fragment invocation counter
  address and (OpenGL only) the 64-bit polygon stipple address
