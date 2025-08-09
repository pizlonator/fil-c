
/*
 * BZip3 - A spiritual successor to BZip2.
 * Copyright (C) 2022-2024 Kamila Szewczyk
 *
 * This program is free software: you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of  MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "libbz3.h"
#include <stdlib.h>
#include <string.h>
#include "libsais.h"

#if defined(__GNUC__) || defined(__clang__)
    #define LIKELY(x)   __builtin_expect(!!(x), 1)
    #define UNLIKELY(x) __builtin_expect(!!(x), 0)
#else
    #define LIKELY(x)   (x)
    #define UNLIKELY(x) (x)
#endif

/* CRC32 implementation. Since CRC32 generally takes less than 1% of the runtime on real-world data (e.g. the
   Silesia corpus), I decided against using hardware CRC32. This implementation is simple, fast, fool-proof and
   good enough to be used with bzip3. */

static const u32 crc32Table[256] = {
    0x00000000L, 0xF26B8303L, 0xE13B70F7L, 0x1350F3F4L, 0xC79A971FL, 0x35F1141CL, 0x26A1E7E8L, 0xD4CA64EBL, 0x8AD958CFL,
    0x78B2DBCCL, 0x6BE22838L, 0x9989AB3BL, 0x4D43CFD0L, 0xBF284CD3L, 0xAC78BF27L, 0x5E133C24L, 0x105EC76FL, 0xE235446CL,
    0xF165B798L, 0x030E349BL, 0xD7C45070L, 0x25AFD373L, 0x36FF2087L, 0xC494A384L, 0x9A879FA0L, 0x68EC1CA3L, 0x7BBCEF57L,
    0x89D76C54L, 0x5D1D08BFL, 0xAF768BBCL, 0xBC267848L, 0x4E4DFB4BL, 0x20BD8EDEL, 0xD2D60DDDL, 0xC186FE29L, 0x33ED7D2AL,
    0xE72719C1L, 0x154C9AC2L, 0x061C6936L, 0xF477EA35L, 0xAA64D611L, 0x580F5512L, 0x4B5FA6E6L, 0xB93425E5L, 0x6DFE410EL,
    0x9F95C20DL, 0x8CC531F9L, 0x7EAEB2FAL, 0x30E349B1L, 0xC288CAB2L, 0xD1D83946L, 0x23B3BA45L, 0xF779DEAEL, 0x05125DADL,
    0x1642AE59L, 0xE4292D5AL, 0xBA3A117EL, 0x4851927DL, 0x5B016189L, 0xA96AE28AL, 0x7DA08661L, 0x8FCB0562L, 0x9C9BF696L,
    0x6EF07595L, 0x417B1DBCL, 0xB3109EBFL, 0xA0406D4BL, 0x522BEE48L, 0x86E18AA3L, 0x748A09A0L, 0x67DAFA54L, 0x95B17957L,
    0xCBA24573L, 0x39C9C670L, 0x2A993584L, 0xD8F2B687L, 0x0C38D26CL, 0xFE53516FL, 0xED03A29BL, 0x1F682198L, 0x5125DAD3L,
    0xA34E59D0L, 0xB01EAA24L, 0x42752927L, 0x96BF4DCCL, 0x64D4CECFL, 0x77843D3BL, 0x85EFBE38L, 0xDBFC821CL, 0x2997011FL,
    0x3AC7F2EBL, 0xC8AC71E8L, 0x1C661503L, 0xEE0D9600L, 0xFD5D65F4L, 0x0F36E6F7L, 0x61C69362L, 0x93AD1061L, 0x80FDE395L,
    0x72966096L, 0xA65C047DL, 0x5437877EL, 0x4767748AL, 0xB50CF789L, 0xEB1FCBADL, 0x197448AEL, 0x0A24BB5AL, 0xF84F3859L,
    0x2C855CB2L, 0xDEEEDFB1L, 0xCDBE2C45L, 0x3FD5AF46L, 0x7198540DL, 0x83F3D70EL, 0x90A324FAL, 0x62C8A7F9L, 0xB602C312L,
    0x44694011L, 0x5739B3E5L, 0xA55230E6L, 0xFB410CC2L, 0x092A8FC1L, 0x1A7A7C35L, 0xE811FF36L, 0x3CDB9BDDL, 0xCEB018DEL,
    0xDDE0EB2AL, 0x2F8B6829L, 0x82F63B78L, 0x709DB87BL, 0x63CD4B8FL, 0x91A6C88CL, 0x456CAC67L, 0xB7072F64L, 0xA457DC90L,
    0x563C5F93L, 0x082F63B7L, 0xFA44E0B4L, 0xE9141340L, 0x1B7F9043L, 0xCFB5F4A8L, 0x3DDE77ABL, 0x2E8E845FL, 0xDCE5075CL,
    0x92A8FC17L, 0x60C37F14L, 0x73938CE0L, 0x81F80FE3L, 0x55326B08L, 0xA759E80BL, 0xB4091BFFL, 0x466298FCL, 0x1871A4D8L,
    0xEA1A27DBL, 0xF94AD42FL, 0x0B21572CL, 0xDFEB33C7L, 0x2D80B0C4L, 0x3ED04330L, 0xCCBBC033L, 0xA24BB5A6L, 0x502036A5L,
    0x4370C551L, 0xB11B4652L, 0x65D122B9L, 0x97BAA1BAL, 0x84EA524EL, 0x7681D14DL, 0x2892ED69L, 0xDAF96E6AL, 0xC9A99D9EL,
    0x3BC21E9DL, 0xEF087A76L, 0x1D63F975L, 0x0E330A81L, 0xFC588982L, 0xB21572C9L, 0x407EF1CAL, 0x532E023EL, 0xA145813DL,
    0x758FE5D6L, 0x87E466D5L, 0x94B49521L, 0x66DF1622L, 0x38CC2A06L, 0xCAA7A905L, 0xD9F75AF1L, 0x2B9CD9F2L, 0xFF56BD19L,
    0x0D3D3E1AL, 0x1E6DCDEEL, 0xEC064EEDL, 0xC38D26C4L, 0x31E6A5C7L, 0x22B65633L, 0xD0DDD530L, 0x0417B1DBL, 0xF67C32D8L,
    0xE52CC12CL, 0x1747422FL, 0x49547E0BL, 0xBB3FFD08L, 0xA86F0EFCL, 0x5A048DFFL, 0x8ECEE914L, 0x7CA56A17L, 0x6FF599E3L,
    0x9D9E1AE0L, 0xD3D3E1ABL, 0x21B862A8L, 0x32E8915CL, 0xC083125FL, 0x144976B4L, 0xE622F5B7L, 0xF5720643L, 0x07198540L,
    0x590AB964L, 0xAB613A67L, 0xB831C993L, 0x4A5A4A90L, 0x9E902E7BL, 0x6CFBAD78L, 0x7FAB5E8CL, 0x8DC0DD8FL, 0xE330A81AL,
    0x115B2B19L, 0x020BD8EDL, 0xF0605BEEL, 0x24AA3F05L, 0xD6C1BC06L, 0xC5914FF2L, 0x37FACCF1L, 0x69E9F0D5L, 0x9B8273D6L,
    0x88D28022L, 0x7AB90321L, 0xAE7367CAL, 0x5C18E4C9L, 0x4F48173DL, 0xBD23943EL, 0xF36E6F75L, 0x0105EC76L, 0x12551F82L,
    0xE03E9C81L, 0x34F4F86AL, 0xC69F7B69L, 0xD5CF889DL, 0x27A40B9EL, 0x79B737BAL, 0x8BDCB4B9L, 0x988C474DL, 0x6AE7C44EL,
    0xBE2DA0A5L, 0x4C4623A6L, 0x5F16D052L, 0xAD7D5351L
};

static u32 crc32sum(u32 crc, u8 * RESTRICT buf, size_t size) {
    while (size--) crc = crc32Table[((u8)crc ^ *(buf++)) & 0xff] ^ (crc >> 8);
    return crc;
}

/* LZP code. These constants were manually tuned to give the best compression ratio while using relatively
   little resources. The LZP dictionary is only around 1MiB in size and the minimum match length was chosen
   so that LZP would not interfere too much with the Burrows-Wheeler transform and the arithmetic coder, and
   just collapse long redundant data instead (for a major speed-up at a low compression ratio cost - in fact,
   LZP preprocessing often improves compression in some cases). */

/* A heavily modified version of libbsc's LZP predictor w/ unaligned accesses follows. This one has single thread
   performance and provides better compression ratio. It is also mostly UB-free and less brittle during
   AFL fuzzing. */

#define LZP_DICTIONARY 18
#define LZP_MIN_MATCH 40

#define MATCH 0xf2

static u32 lzp_upcast(const u8 * ptr) {
    // val = *(u32 *)ptr; - written this way to avoid UB
    u32 val;
    memcpy(&val, ptr, sizeof(val));
    return val;
}

/**
 * @brief Check if the buffer size is sufficient for decoding a bz3 block
 * 
 * Data passed to the last step can be one of the following:
 * - original data
 * - original data + LZP
 * - original data + RLE
 * - original data + RLE + LZP
 *
 * We must ensure `buffer_size` is large enough to store the data at every step 
 * when walking backwards. The required size may be stored in  either `lzp_size`,
 * `rle_size` OR `orig_size`.
 *
 * @param buffer_size Size of the output buffer
 * @param lzp_size Size after LZP decompression (-1 if LZP not used)
 * @param rle_size Size after RLE decompression (-1 if RLE not used) 
 * @return 1 if buffer size is sufficient, 0 otherwise
 */
static int bz3_check_buffer_size(size_t buffer_size, s32 lzp_size, s32 rle_size, s32 orig_size) {
    // Handle -1 cases to avoid implicit conversion issues
    size_t effective_lzp_size = lzp_size < 0 ? 0 : (size_t)lzp_size;
    size_t effective_rle_size = rle_size < 0 ? 0 : (size_t)rle_size;
    size_t effective_orig_size = orig_size < 0 ? 0 : (size_t)orig_size;

    // Check if buffer can hold intermediate results
    return (effective_lzp_size <= buffer_size) && (effective_rle_size <= buffer_size) && (effective_orig_size <= buffer_size);
}

static s32 lzp_encode_block(const u8 * RESTRICT in, const u8 * in_end, u8 * RESTRICT out, u8 * out_end,
                            s32 * RESTRICT lut) {
    const u8 * ins = in;
    const u8 * outs = out;
    const u8 * out_eob = out_end - 8;
    const u8 * heur = in;

    u32 ctx;

    for (s32 i = 0; i < 4; ++i) *out++ = *in++;

    ctx = ((u32)in[-1]) | (((u32)in[-2]) << 8) | (((u32)in[-3]) << 16) | (((u32)in[-4]) << 24);

    while (in < in_end - LZP_MIN_MATCH - 32 && out < out_eob) {
        u32 idx = (ctx >> 15 ^ ctx ^ ctx >> 3) & ((s32)(1 << LZP_DICTIONARY) - 1);
        s32 val = lut[idx];
        lut[idx] = in - ins;
        if (val > 0) {
            const u8 * RESTRICT ref = ins + val;
            if (memcmp(in + LZP_MIN_MATCH - 4, ref + LZP_MIN_MATCH - 4, sizeof(u32)) == 0 &&
                memcmp(in, ref, sizeof(u32)) == 0) {
                if (heur > in && lzp_upcast(heur) != lzp_upcast(ref + (heur - in))) goto not_found;

                s32 len = 4;
                for (; in + len < in_end - LZP_MIN_MATCH - 32; len += sizeof(u32)) {
                    if (lzp_upcast(in + len) != lzp_upcast(ref + len)) break;
                }

                if (len < LZP_MIN_MATCH) {
                    if (heur < in + len) heur = in + len;
                    goto not_found;
                }

                len += in[len] == ref[len];
                len += in[len] == ref[len];
                len += in[len] == ref[len];

                in += len;
                ctx = ((u32)in[-1]) | (((u32)in[-2]) << 8) | (((u32)in[-3]) << 16) | (((u32)in[-4]) << 24);

                *out++ = MATCH;

                len -= LZP_MIN_MATCH;
                while (len >= 254) {
                    len -= 254;
                    *out++ = 254;
                    if (out >= out_eob) break;
                }

                *out++ = len;
            } else {
            not_found:;
                u8 next = *out++ = *in++;
                ctx = ctx << 8 | next;
                if (next == MATCH) *out++ = 255;
            }
        } else {
            ctx = (ctx << 8) | (*out++ = *in++);
        }
    }

    ctx = ((u32)in[-1]) | (((u32)in[-2]) << 8) | (((u32)in[-3]) << 16) | (((u32)in[-4]) << 24);

    while (in < in_end && out < out_eob) {
        u32 idx = (ctx >> 15 ^ ctx ^ ctx >> 3) & ((s32)(1 << LZP_DICTIONARY) - 1);
        s32 val = lut[idx];
        lut[idx] = (s32)(in - ins);

        u8 next = *out++ = *in++;
        ctx = ctx << 8 | next;
        if (next == MATCH && val > 0) *out++ = 255;
    }

    return out >= out_eob ? -1 : (s32)(out - outs);
}

static s32 lzp_decode_block(const u8 * RESTRICT in, const u8 * in_end, s32 * RESTRICT lut, u8 * RESTRICT out,
                            const u8 * out_end) {
    const u8 * outs = out;

    for (s32 i = 0; i < 4; ++i) *out++ = *in++;

    u32 ctx = ((u32)out[-1]) | (((u32)out[-2]) << 8) | (((u32)out[-3]) << 16) | (((u32)out[-4]) << 24);

    while (in < in_end && out < out_end) {
        u32 idx = (ctx >> 15 ^ ctx ^ ctx >> 3) & ((s32)(1 << LZP_DICTIONARY) - 1);
        s32 val = lut[idx]; // SAFETY: guaranteed to be in-bounds by & mask. 
        lut[idx] = (s32)(out - outs);
        if (*in == MATCH && val > 0) {
            in++;
            // SAFETY: 'in' is advanced here, but it may have been at last index in the case of untrusted bad data.
            if (UNLIKELY(in == in_end)) return -1;
            if (*in != 255) {
                s32 len = LZP_MIN_MATCH;
                while (1) {
                    if (UNLIKELY(in == in_end)) return -1;
                    len += *in;
                    if (*in++ != 254) break;
                }

                const u8 * ref = outs + val;
                const u8 * oe = out + len;
                if (UNLIKELY(oe > out_end)) oe = out_end;

                while (out < oe) *out++ = *ref++;

                ctx = ((u32)out[-1]) | (((u32)out[-2]) << 8) | (((u32)out[-3]) << 16) | (((u32)out[-4]) << 24);
            } else {
                in++;
                ctx = (ctx << 8) | (*out++ = MATCH);
            }
        } else {
            ctx = (ctx << 8) | (*out++ = *in++);
        }
    }

    return out - outs;
}

static s32 lzp_compress(const u8 * RESTRICT in, u8 * RESTRICT out, s32 n, s32 * RESTRICT lut) {
    if (n < LZP_MIN_MATCH + 32) return -1;

    memset(lut, 0, sizeof(s32) * (1 << LZP_DICTIONARY));

    return lzp_encode_block(in, in + n, out, out + n, lut);
}

static s32 lzp_decompress(const u8 * RESTRICT in, u8 * RESTRICT out, s32 n, s32 max, s32 * RESTRICT lut) {
    if (n < 4) return -1;

    memset(lut, 0, sizeof(s32) * (1 << LZP_DICTIONARY));

    return lzp_decode_block(in, in + n, lut, out, out + max);
}

/* RLE code. Unlike RLE in other compressors, we collapse all runs if they yield a net gain
   for a given character and encode this as a set bit in the RLE metadata. This improves the
   performance and reduces the amount of collapsing done in normal blocks (so that BWT+AC can
   be more efficient) while we still filter out all the pathological data. */

static s32 mrlec(u8 * in, s32 inlen, u8 * out) {
    u8 * ip = in;
    u8 * in_end = in + inlen;
    s32 op = 0;
    s32 c, pc = -1;
    s32 t[256] = { 0 };
    s32 run = 0;
    while ((c = (ip < in_end ? *ip++ : -1)) != -1) {
        if (c == pc)
            t[c] += (++run % 255) != 0;
        else
            --t[c], run = 0;
        pc = c;
    }
    for (s32 i = 0; i < 32; ++i) {
        c = 0;
        for (s32 j = 0; j < 8; ++j) c += (t[i * 8 + j] > 0) << j;
        out[op++] = c;
    }
    ip = in;
    c = pc = -1;
    run = 0;
    do {
        c = ip < in_end ? *ip++ : -1;
        if (c == pc)
            ++run;
        else if (run > 0 && t[pc] > 0) {
            out[op++] = pc;
            for (; run > 255; run -= 255) out[op++] = 255;
            out[op++] = run - 1;
            run = 1;
        } else
            for (++run; run > 1; --run) out[op++] = pc;
        pc = c;
    } while (c != -1);

    return op;
}

static int mrled(u8 * RESTRICT in, u8 * RESTRICT out, s32 outlen, s32 maxin) {
    s32 op = 0, ip = 0;

    s32 c, pc = -1;
    s32 t[256] = { 0 };
    s32 run = 0;

    if (maxin < 32) return 1;

    for (s32 i = 0; i < 32; ++i) {
        c = in[ip++];
        for (s32 j = 0; j < 8; ++j) t[i * 8 + j] = (c >> j) & 1;
    }

    while (op < outlen && ip < maxin) {
        c = in[ip++];
        if (t[c]) {
            for (run = 0; ip < maxin && (pc = in[ip++]) == 255; run += 255)
                ;
            run += pc + 1;
            for (; run > 0 && op < outlen; --run) out[op++] = c;
        } else
            out[op++] = c;
    }

    return op != outlen;
}

/* The entropy coder. Uses an arithmetic coder implementation outlined in Matt Mahoney's DCE. */

typedef struct {
    /* Input/output. */
    u8 *in_queue, *out_queue;
    s32 input_ptr, output_ptr, input_max;

    /* C0, C1 - used for making the initial prediction, C2 used for an APM with a slightly low
       learning rate (6) and 512 contexts. kanzi merges C0 and C1, uses slightly different
       counter initialisation code and prediction code which from my tests tends to be suboptimal. */
    u16 C0[256], C1[256][256], C2[512][17];
} state;

#define write_out(s, c) (s)->out_queue[(s)->output_ptr++] = (c)
#define read_in(s) ((s)->input_ptr < (s)->input_max ? (s)->in_queue[(s)->input_ptr++] : -1)

#define update0(p, x) (p) = ((p) - ((p) >> x))
#define update1(p, x) (p) = ((p) + (((p) ^ 65535) >> x))

static void begin(state * s) {
    prefetch(s);
    for (int i = 0; i < 256; i++) s->C0[i] = 1 << 15;
    for (int i = 0; i < 256; i++)
        for (int j = 0; j < 256; j++) s->C1[i][j] = 1 << 15;
    for (int i = 0; i < 2; i++)
        for (int j = 0; j < 256; j++)
            for (int k = 0; k < 17; k++) s->C2[2 * j + i][k] = (k << 12) - (k == 16);  // Firm difference from stdpack.
}

static void encode_bytes(state * s, u8 * buf, s32 size) {
    /* Arithmetic coding, detecting runs of characters in the file */
    u32 high = 0xFFFFFFFF, low = 0, c1 = 0, c2 = 0, run = 0;

    for (s32 i = 0; i < size; i++) {
        u8 c = buf[i];

        if (c1 == c2)
            ++run;
        else
            run = 0;

        const int f = run > 2;

        int ctx = 1;

        while (ctx < 256) {
            const int p0 = s->C0[ctx];
            const int p1 = s->C1[c1][ctx];
            const int p2 = s->C1[c2][ctx];
            const int p = ((p0 + p1) * 7 + p2 + p2) >> 4;

            const int j = p >> 12;
            const int x1 = s->C2[2 * ctx + f][j];
            const int x2 = s->C2[2 * ctx + f][j + 1];
            const int ssep = x1 + (((x2 - x1) * (p & 4095)) >> 12);

            if (c & 128) {
                high = low + (((u64)(high - low) * (ssep * 3 + p)) >> 18);

                while ((low ^ high) < (1 << 24)) {
                    write_out(s, low >> 24);
                    low <<= 8;
                    high = (high << 8) + 0xFF;
                }

                update1(s->C0[ctx], 2);
                update1(s->C1[c1][ctx], 4);
                update1(s->C2[2 * ctx + f][j], 6);
                update1(s->C2[2 * ctx + f][j + 1], 6);
                ctx += ctx + 1;
            } else {
                low += (((u64)(high - low) * (ssep * 3 + p)) >> 18) + 1;

                // Write identical bits.
                while ((low ^ high) < (1 << 24)) {
                    write_out(s, low >> 24);  // Same as high >> 24
                    low <<= 8;
                    high = (high << 8) + 0xFF;
                }

                update0(s->C0[ctx], 2);
                update0(s->C1[c1][ctx], 4);
                update0(s->C2[2 * ctx + f][j], 6);
                update0(s->C2[2 * ctx + f][j + 1], 6);
                ctx += ctx;
            }

            c <<= 1;
        }

        c2 = c1;
        c1 = ctx & 255;
    }

    write_out(s, low >> 24);
    low <<= 8;
    write_out(s, low >> 24);
    low <<= 8;
    write_out(s, low >> 24);
    low <<= 8;
    write_out(s, low >> 24);
    low <<= 8;
}

static void decode_bytes(state * s, u8 * c, s32 size) {
    u32 high = 0xFFFFFFFF, low = 0, c1 = 0, c2 = 0, run = 0, code = 0;

    code = (code << 8) + read_in(s);
    code = (code << 8) + read_in(s);
    code = (code << 8) + read_in(s);
    code = (code << 8) + read_in(s);

    for (s32 i = 0; i < size; i++) {
        if (c1 == c2)
            ++run;
        else
            run = 0;

        const int f = run > 2;

        int ctx = 1;

        while (ctx < 256) {
            const int p0 = s->C0[ctx];
            const int p1 = s->C1[c1][ctx];
            const int p2 = s->C1[c2][ctx];
            const int p = ((p0 + p1) * 7 + p2 + p2) >> 4;

            const int j = p >> 12;
            const int x1 = s->C2[2 * ctx + f][j];
            const int x2 = s->C2[2 * ctx + f][j + 1];
            const int ssep = x1 + (((x2 - x1) * (p & 4095)) >> 12);

            const u32 mid = low + (((u64)(high - low) * (ssep * 3 + p)) >> 18);
            const u8 bit = code <= mid;
            if (bit)
                high = mid;
            else
                low = mid + 1;
            while ((low ^ high) < (1 << 24)) {
                low <<= 8;
                high = (high << 8) + 255;
                code = (code << 8) + read_in(s);
            }

            if (bit) {
                update1(s->C0[ctx], 2);
                update1(s->C1[c1][ctx], 4);
                update1(s->C2[2 * ctx + f][j], 6);
                update1(s->C2[2 * ctx + f][j + 1], 6);
                ctx += ctx + 1;
            } else {
                update0(s->C0[ctx], 2);
                update0(s->C1[c1][ctx], 4);
                update0(s->C2[2 * ctx + f][j], 6);
                update0(s->C2[2 * ctx + f][j + 1], 6);
                ctx += ctx;
            }
        }

        c2 = c1;
        c[i] = c1 = ctx & 255;
    }
}

/* Public API. */

struct bz3_state {
    u8 * swap_buffer;
    s32 block_size;
    s32 *sais_array, *lzp_lut;
    state * cm_state;
    s8 last_error;
};

BZIP3_API s8 bz3_last_error(struct bz3_state * state) { return state->last_error; }

BZIP3_API const char * bz3_version(void) { return VERSION; }

BZIP3_API size_t bz3_bound(size_t input_size) { return input_size + input_size / 50 + 32; }

BZIP3_API const char * bz3_strerror(struct bz3_state * state) {
    switch (state->last_error) {
        case BZ3_OK:
            return "No error";
        case BZ3_ERR_OUT_OF_BOUNDS:
            return "Data index out of bounds";
        case BZ3_ERR_BWT:
            return "Burrows-Wheeler transform failed";
        case BZ3_ERR_CRC:
            return "CRC32 check failed";
        case BZ3_ERR_MALFORMED_HEADER:
            return "Malformed header";
        case BZ3_ERR_TRUNCATED_DATA:
            return "Truncated data";
        case BZ3_ERR_DATA_TOO_BIG:
            return "Too much data";
        case BZ3_ERR_DATA_SIZE_TOO_SMALL:
            return "Size of buffer `buffer_size` passed to the block decoder (bz3_decode_block) is too small. See function docs for details.";
        default:
            return "Unknown error";
    }
}

BZIP3_API struct bz3_state * bz3_new(s32 block_size) {
    if (block_size < KiB(65) || block_size > MiB(511)) {
        return NULL;
    }

    struct bz3_state * bz3_state = malloc(sizeof(struct bz3_state));

    if (!bz3_state) {
        return NULL;
    }

    bz3_state->cm_state = malloc(sizeof(state));

    bz3_state->swap_buffer = malloc(bz3_bound(block_size));
    bz3_state->sais_array = malloc(BWT_BOUND(block_size) * sizeof(s32));
    memset(bz3_state->sais_array, 0, sizeof(s32) * BWT_BOUND(block_size));

    bz3_state->lzp_lut = calloc(1 << LZP_DICTIONARY, sizeof(s32));

    if (!bz3_state->cm_state || !bz3_state->swap_buffer || !bz3_state->sais_array || !bz3_state->lzp_lut) {
        if (bz3_state->cm_state) free(bz3_state->cm_state);
        if (bz3_state->swap_buffer) free(bz3_state->swap_buffer);
        if (bz3_state->sais_array) free(bz3_state->sais_array);
        if (bz3_state->lzp_lut) free(bz3_state->lzp_lut);
        free(bz3_state);
        return NULL;
    }

    bz3_state->block_size = block_size;

    bz3_state->last_error = BZ3_OK;

    return bz3_state;
}

BZIP3_API void bz3_free(struct bz3_state * state) {
    free(state->swap_buffer);
    free(state->sais_array);
    free(state->cm_state);
    free(state->lzp_lut);
    free(state);
}

#define swap(x, y)    \
    {                 \
        u8 * tmp = x; \
        x = y;        \
        y = tmp;      \
    }

BZIP3_API s32 bz3_encode_block(struct bz3_state * state, u8 * buffer, s32 data_size) {
    u8 *b1 = buffer, *b2 = state->swap_buffer;

    if (data_size > state->block_size) {
        state->last_error = BZ3_ERR_DATA_TOO_BIG;
        return -1;
    }

    u32 crc32 = crc32sum(1, b1, data_size);

    // Ignore small blocks. They won't benefit from the entropy coding step.
    if (data_size < 64) {
        memmove(b1 + 8, b1, data_size);
        write_neutral_s32(b1, crc32);
        write_neutral_s32(b1 + 4, -1);
        return data_size + 8;
    }

    // Back to front:
    // bit 1: lzp | no lzp
    // bit 2: srt | no srt
    s8 model = 0;
    s32 lzp_size, rle_size;

    rle_size = mrlec(b1, data_size, b2);
    if (rle_size < data_size) {
        swap(b1, b2);
        data_size = rle_size;
        model |= 4;
    }

    lzp_size = lzp_compress(b1, b2, data_size, state->lzp_lut);
    if (lzp_size > 0 && lzp_size < data_size) {
        swap(b1, b2);
        data_size = lzp_size;
        model |= 2;
    }

    s32 bwt_idx = libsais_bwt(b1, b2, state->sais_array, data_size, 0, NULL);
    if (bwt_idx < 0) {
        state->last_error = BZ3_ERR_BWT;
        return -1;
    }

    // Compute the amount of overhead dwords.
    s32 overhead = 2;           // CRC32 + BWT index
    if (model & 2) overhead++;  // LZP
    if (model & 4) overhead++;  // RLE

    begin(state->cm_state);
    state->cm_state->out_queue = b1 + overhead * 4 + 1;
    state->cm_state->output_ptr = 0;
    encode_bytes(state->cm_state, b2, data_size);
    data_size = state->cm_state->output_ptr;

    // Write the header. Starting with common entries.
    write_neutral_s32(b1, crc32);
    write_neutral_s32(b1 + 4, bwt_idx);
    b1[8] = model;

    s32 p = 0;
    if (model & 2) write_neutral_s32(b1 + 9 + 4 * p++, lzp_size);
    if (model & 4) write_neutral_s32(b1 + 9 + 4 * p++, rle_size);

    state->last_error = BZ3_OK;

    if (b1 != buffer) memcpy(buffer, b1, data_size + overhead * 4 + 1);

    return data_size + overhead * 4 + 1;
}

BZIP3_API s32 bz3_decode_block(struct bz3_state * state, u8 * buffer, size_t buffer_size, s32 compressed_size, s32 orig_size) {
    // Need minimum bytes for initial header, and compressed_size needs to fit within claimed buffer size.
    if (buffer_size < 9 || buffer_size < compressed_size) {
        state->last_error = BZ3_ERR_DATA_SIZE_TOO_SMALL;
        return -1;
    }

    // Read the header.
    u32 crc32 = read_neutral_s32(buffer);
    s32 bwt_idx = read_neutral_s32(buffer + 4);

    if (compressed_size > bz3_bound(state->block_size) || compressed_size < 0) {
        state->last_error = BZ3_ERR_MALFORMED_HEADER;
        return -1;
    }

    if (bwt_idx == -1) {
        if (compressed_size - 8 > 64 || compressed_size < 8) {
            state->last_error = BZ3_ERR_MALFORMED_HEADER;
            return -1;
        }

        // Ensure there's enough space for the raw copied data.
        if (compressed_size - 8 > buffer_size) {
            state->last_error = BZ3_ERR_DATA_SIZE_TOO_SMALL;
            return -1;
        }

        memmove(buffer, buffer + 8, compressed_size - 8);

        if (crc32sum(1, buffer, compressed_size - 8) != crc32) {
            state->last_error = BZ3_ERR_CRC;
            return -1;
        }

        return compressed_size - 8;
    }

    s8 model = buffer[8];

    // Ensure we have sufficient bytes for the rle/lzp sizes.
    size_t needed_header_size = 9 + ((model & 2) * 4) + ((model & 4) * 4);
    if (buffer_size < needed_header_size) {
        state->last_error = BZ3_ERR_DATA_SIZE_TOO_SMALL;
        return -1;
    }

    s32 lzp_size = -1, rle_size = -1, p = 0;
    if (model & 2) lzp_size = read_neutral_s32(buffer + 9 + 4 * p++);
    if (model & 4) rle_size = read_neutral_s32(buffer + 9 + 4 * p++);
    p += 2;

    compressed_size -= p * 4 + 1;

    if (((model & 2) && (lzp_size > bz3_bound(state->block_size) || lzp_size < 0)) ||
        ((model & 4) && (rle_size > bz3_bound(state->block_size) || rle_size < 0))) {
        state->last_error = BZ3_ERR_MALFORMED_HEADER;
        return -1;
    }

    if (orig_size > bz3_bound(state->block_size) || orig_size < 0) {
        state->last_error = BZ3_ERR_MALFORMED_HEADER;
        return -1;
    }

    // Size that undoing BWT+BCM should decompress into.
    s32 size_before_bwt;

    if (model & 2)
        size_before_bwt = lzp_size;
    else if (model & 4)
        size_before_bwt = rle_size;
    else
        size_before_bwt = orig_size;

    // Note(sewer): It's technically valid within the spec to create a bzip3 block
    // where the size after LZP/RLE is larger than the original input. Some earlier encoders
    // even (mistakenly?) were able to do this.
    if (!bz3_check_buffer_size(buffer_size, lzp_size, rle_size, orig_size)) {
        state->last_error = BZ3_ERR_DATA_SIZE_TOO_SMALL;
        return -1;
    }

    // Decode the data.
    u8 *b1 = buffer, *b2 = state->swap_buffer;

    begin(state->cm_state);
    state->cm_state->in_queue = b1 + p * 4 + 1;
    state->cm_state->input_ptr = 0;
    state->cm_state->input_max = compressed_size;

    decode_bytes(state->cm_state, b2, size_before_bwt);
    swap(b1, b2);

    if (bwt_idx > size_before_bwt) {
        state->last_error = BZ3_ERR_MALFORMED_HEADER;
        return -1;
    }

    // Undo BWT
    memset(state->sais_array, 0, sizeof(s32) * BWT_BOUND(state->block_size));
    memset(b2, 0, size_before_bwt); // buffer b2, swap b1
    if (libsais_unbwt(b1, b2, state->sais_array, size_before_bwt, NULL, bwt_idx) < 0) {
        state->last_error = BZ3_ERR_BWT;
        return -1;
    }
    swap(b1, b2);

    s32 size_src = size_before_bwt;

    // Undo LZP
    if (model & 2) {
        size_src = lzp_decompress(b1, b2, lzp_size, bz3_bound(state->block_size), state->lzp_lut);
        if (size_src == -1) {
            state->last_error = BZ3_ERR_CRC;
            return -1;
        }
        // SAFETY(sewer): An attacker formed bzip3 data which decompresses as valid lzp.
        // The headers above were set to ones that pass validation (size within bounds), but the 
        // data itself tries to escape buffer_size. Don't allow it to.
        if (size_src > buffer_size) {
            state->last_error = BZ3_ERR_DATA_SIZE_TOO_SMALL;    
            return -1;
        }
        swap(b1, b2);
    }

    if (model & 4) { 
        // SAFETY: mrled is capped at orig_size, which is in bounds.
        int err = mrled(b1, b2, orig_size, size_src);
        if (err) {
            state->last_error = BZ3_ERR_CRC;
            return -1;
        }
        size_src = orig_size;
        swap(b1, b2);
    }

    state->last_error = BZ3_OK;

    if (size_src > state->block_size || size_src < 0) {
        state->last_error = BZ3_ERR_MALFORMED_HEADER;
        return -1;
    }

    if (b1 != buffer) memcpy(buffer, b1, size_src);

    if (crc32 != crc32sum(1, buffer, size_src)) {
        state->last_error = BZ3_ERR_CRC;
        return -1;
    }

    return size_src;
}

#undef swap

#ifdef PTHREAD

    #include <pthread.h>

typedef struct {
    struct bz3_state * state;
    u8 * buffer;
    s32 size;
} encode_thread_msg;

typedef struct {
    struct bz3_state * state;
    u8 * buffer;
    size_t buffer_size;
    s32 size;
    s32 orig_size;
} decode_thread_msg;

static void * bz3_init_encode_thread(void * _msg) {
    encode_thread_msg * msg = _msg;
    msg->size = bz3_encode_block(msg->state, msg->buffer, msg->size);
    pthread_exit(NULL);
    return NULL;  // unreachable
}

static void * bz3_init_decode_thread(void * _msg) {
    decode_thread_msg * msg = _msg;
    bz3_decode_block(msg->state, msg->buffer, msg->buffer_size, msg->size, msg->orig_size);
    pthread_exit(NULL);
    return NULL;  // unreachable
}

BZIP3_API void bz3_encode_blocks(struct bz3_state * states[], u8 * buffers[], s32 sizes[], s32 n) {
    encode_thread_msg messages[n];
    pthread_t threads[n];
    for (s32 i = 0; i < n; i++) {
        messages[i].state = states[i];
        messages[i].buffer = buffers[i];
        messages[i].size = sizes[i];
        pthread_create(&threads[i], NULL, bz3_init_encode_thread, &messages[i]);
    }
    for (s32 i = 0; i < n; i++) pthread_join(threads[i], NULL);
    for (s32 i = 0; i < n; i++) sizes[i] = messages[i].size;
}

BZIP3_API void bz3_decode_blocks(struct bz3_state * states[], u8 * buffers[], size_t buffer_sizes[], s32 sizes[], s32 orig_sizes[], s32 n) {
    decode_thread_msg messages[n];
    pthread_t threads[n];
    for (s32 i = 0; i < n; i++) {
        messages[i].state = states[i];
        messages[i].buffer = buffers[i];
        messages[i].buffer_size = buffer_sizes[i];
        messages[i].size = sizes[i];
        messages[i].orig_size = orig_sizes[i];
        pthread_create(&threads[i], NULL, bz3_init_decode_thread, &messages[i]);
    }
    for (s32 i = 0; i < n; i++) pthread_join(threads[i], NULL);
}

#endif

/* High level API implementations. */

BZIP3_API int bz3_compress(u32 block_size, const u8 * const in, u8 * out, size_t in_size, size_t * out_size) {
    if (block_size > in_size) block_size = in_size + 16;
    block_size = block_size <= KiB(65) ? KiB(65) : block_size;

    struct bz3_state * state = bz3_new(block_size);
    if (!state) return BZ3_ERR_INIT;

    u8 * compression_buf = malloc(bz3_bound(block_size));
    if (!compression_buf) {
        bz3_free(state);
        return BZ3_ERR_INIT;
    }

    size_t buf_max = *out_size;
    *out_size = 0;

    u32 n_blocks = in_size / block_size;
    if (in_size % block_size) n_blocks++;

    if (buf_max < 13 || buf_max < bz3_bound(in_size)) {
        bz3_free(state);
        free(compression_buf);
        return BZ3_ERR_DATA_TOO_BIG;
    }

    out[0] = 'B';
    out[1] = 'Z';
    out[2] = '3';
    out[3] = 'v';
    out[4] = '1';
    write_neutral_s32(out + 5, block_size);
    write_neutral_s32(out + 9, n_blocks);
    *out_size += 13;

    // Compress and write the blocks.
    size_t in_offset = 0;
    for (u32 i = 0; i < n_blocks; i++) {
        s32 size = block_size;
        if (i == n_blocks - 1) size = in_size % block_size;
        memcpy(compression_buf, in + in_offset, size);
        s32 out_size_block = bz3_encode_block(state, compression_buf, size);
        if (bz3_last_error(state) != BZ3_OK) {
            s8 last_error = state->last_error;
            bz3_free(state);
            free(compression_buf);
            return last_error;
        }
        memcpy(out + *out_size + 8, compression_buf, out_size_block);
        write_neutral_s32(out + *out_size, out_size_block);
        write_neutral_s32(out + *out_size + 4, size);
        *out_size += out_size_block + 8;
        in_offset += size;
    }

    bz3_free(state);
    free(compression_buf);
    return BZ3_OK;
}

BZIP3_API int bz3_decompress(const uint8_t * in, uint8_t * out, size_t in_size, size_t * out_size) {
    if (in_size < 13) return BZ3_ERR_MALFORMED_HEADER;
    if (in[0] != 'B' || in[1] != 'Z' || in[2] != '3' || in[3] != 'v' || in[4] != '1') {
        return BZ3_ERR_MALFORMED_HEADER;
    }
    u32 block_size = read_neutral_s32(in + 5);
    u32 n_blocks = read_neutral_s32(in + 9);
    in_size -= 13;
    in += 13;

    struct bz3_state * state = bz3_new(block_size);
    if (!state) return BZ3_ERR_INIT;

    size_t compression_buf_size = bz3_bound(block_size);
    u8 * compression_buf = malloc(compression_buf_size);
    if (!compression_buf) {
        bz3_free(state);
        return BZ3_ERR_INIT;
    }

    size_t buf_max = *out_size;
    *out_size = 0;

    for (u32 i = 0; i < n_blocks; i++) {
        if (in_size < 8) {
        malformed_header:
            bz3_free(state);
            free(compression_buf);
            return BZ3_ERR_MALFORMED_HEADER;
        }
        s32 size = read_neutral_s32(in);
        if (size < 0 || size > block_size) goto malformed_header;
        if (in_size < size + 8) {
            bz3_free(state);
            free(compression_buf);
            return BZ3_ERR_TRUNCATED_DATA;
        }
        s32 orig_size = read_neutral_s32(in + 4);
        if (orig_size < 0) goto malformed_header;
        if (buf_max < *out_size + orig_size) {
            bz3_free(state);
            free(compression_buf);
            return BZ3_ERR_DATA_TOO_BIG;
        }
        memcpy(compression_buf, in + 8, size);
        bz3_decode_block(state, compression_buf, compression_buf_size, size, orig_size);
        if (bz3_last_error(state) != BZ3_OK) {
            s8 last_error = state->last_error;
            bz3_free(state);
            free(compression_buf);
            return last_error;
        }
        memcpy(out + *out_size, compression_buf, orig_size);
        *out_size += orig_size;
        in += size + 8;
        in_size -= size + 8;
    }

    bz3_free(state);
    return BZ3_OK;
}

BZIP3_API size_t bz3_min_memory_needed(int32_t block_size) {
    if (block_size < KiB(65) || block_size > MiB(511)) {
        return 0;
    }

    size_t total_size = 0;

    // This is based on bz3_new.
    // Core state structure
    total_size += sizeof(struct bz3_state);

    // cm_state
    total_size += sizeof(state);

    // Swap buffer (needs to handle expanded size) (swap_buffer)
    total_size += bz3_bound(block_size);

    // SAIS array
    total_size += BWT_BOUND(block_size) * sizeof(int32_t);

    // LZP lookup table (lzp_lut)
    total_size += (1 << LZP_DICTIONARY) * sizeof(int32_t);
    return total_size;
}


BZIP3_API int bz3_orig_size_sufficient_for_decode(const u8 * block, size_t block_size, s32 orig_size) {
    // Need at least 9 bytes for the initial header (4 bytes BWT index + 4 bytes CRC + 1 byte model)
    if (block_size < 9) {
        return -1;
    }

    s32 bwt_idx = read_neutral_s32(block + 4);
    if (bwt_idx == -1) {
        // Uncompressed literals.
        // Original size always sufficient for uncompressed blocks
        return 1;  
    }

    s8 model = block[8];
    s32 lzp_size = -1, rle_size = -1;
    size_t header_size = 9;  // Start after model byte

    // Ensure we have sufficient bytes for the rle/lzp sizes.
    size_t needed_header_size = 9 + ((model & 2) * 4) + ((model & 4) * 4);
    if (block_size < needed_header_size) {
        return -1;
    }

    // Need additional 4 bytes for each size field that might be present
    if (model & 2) {
        lzp_size = read_neutral_s32(block + header_size);
        header_size += 4;
    }
    if (model & 4) rle_size = read_neutral_s32(block + header_size);
    return bz3_check_buffer_size((size_t)orig_size, lzp_size, rle_size, orig_size);
}
