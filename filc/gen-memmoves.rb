#!/usr/bin/env ruby

require 'fileutils'

$numTestsInFile = 0
$numFiles = 0

def startFile
    FileUtils.mkdir_p("filc/tests/memmoves#{$numFiles}")
    IO::write("filc/tests/memmoves#{$numFiles}/manifest", "return: success")
    $outp = File.open("filc/tests/memmoves#{$numFiles}/memmoves#{$numFiles}.c", "w")
    $outp.puts "#include <filc_test_support.h>"
    $outp.puts "#include <stdlib.h>"
    $outp.puts "#include <string.h>"
    $outp.puts "#include \"utils.h\""
end

def endFile
    $outp.puts "int main()"
    $outp.puts "{"
    $numTestsInFile.times {
        | index |
        $outp.puts "    test#{index}();"
    }
    $outp.puts "    return 0;"
    $outp.puts "}"
    $outp.close
    $numFiles += 1
end

startFile

(1..40).each {
    | size |
    if size <= 24
        range = 0..7
    else
        range = [0, 1, 6, 7]
    end
    range.each {
        | offset |
        if size <= 24
            memmoves = ["zmemmove_union", "zmemmove"]
        else
            memmoves = ["zmemmove_union"]
        end
        memmoves.each {
            | memmove |
            if size + offset >= 8
                ptrCases = [false, true]
            else
                ptrCases = [false]
            end
            ptrCases.each {
                | srcHasPtrs |
                ptrCases.each {
                    | dstHasPtrs |
                    [false, true].each {
                        | doOpaqueEarly |
                        if size <= 24
                            opaques = [false, true]
                        else
                            opaques = [false]
                        end
                        opaques.each {
                            | doOpaqueMid |
                            opaques.each {
                                | doOpaqueLate |
                                if srcHasPtrs && size <= 16
                                    atomics = ["", "_Atomic"]
                                else
                                    atomics = [""]
                                end
                                atomics.each {
                                    | atomicSrc |
                                    if $numTestsInFile >= 100
                                        endFile
                                        $numTestsInFile = 0
                                        startFile
                                    end
                                    
                                    $outp.puts "static __attribute__((noinline)) void"
                                    $outp.puts "test#{$numTestsInFile}(void)"
                                    $outp.puts "{"
                                    $outp.puts "    int*#{atomicSrc}* src ="
                                    $outp.puts "        zgc_alloc(#{size + offset});"
                                    $outp.puts "    int** origSrc = zgc_alloc(#{size + offset});"
                                    $outp.puts "    int** dst = zgc_alloc(#{size + offset});"
                                    $outp.puts "    int** origDst = zgc_alloc(#{size + offset});"
                                    if doOpaqueEarly
                                        $outp.puts "    src = opaque(src);"
                                        $outp.puts "    dst = opaque(dst);"
                                    end
                                    ((size + offset + 7) / 8).times {
                                        | index |
                                        if srcHasPtrs
                                            $outp.puts "    src[#{index}] = zgc_alloc(sizeof(int));"
                                            $outp.puts "    *src[#{index}] = #{index + 666};"
                                            $outp.puts "    origSrc[#{index}] = src[#{index}];"
                                        end
                                        if dstHasPtrs
                                            $outp.puts "    dst[#{index}] = zgc_alloc(sizeof(int));"
                                            $outp.puts "    *dst[#{index}] = #{index + 1410};"
                                            $outp.puts "    origDst[#{index}] = dst[#{index}];"
                                        end
                                    }
                                    if doOpaqueMid
                                        $outp.puts "    src = opaque(src);"
                                        $outp.puts "    dst = opaque(dst);"
                                    end
                                    $outp.puts "    #{memmove}("
                                    $outp.puts "        (char*)dst + #{offset},"
                                    $outp.puts "        (char*)src + #{offset},"
                                    $outp.puts "        #{size});"
                                    if doOpaqueLate
                                        $outp.puts "    src = opaque(src);"
                                        $outp.puts "    dst = opaque(dst);"
                                    end
                                    index = 0
                                    remaining = size
                                    if offset != 0
                                        $outp.puts "    ZASSERT(!zhasvalidcap(dst[#{index}]));"
                                        index += 1
                                        remaining -= 8 - offset
                                    end
                                    while remaining >= 8
                                        if srcHasPtrs
                                            $outp.puts "    ZASSERT(zhasvalidcap(dst[#{index}]));"
                                            $outp.puts "    ZASSERT(opaque(dst[#{index}]) =="
                                            $outp.puts "            opaque(src[#{index}]));"
                                            $outp.puts "    ZASSERT(*dst[#{index}] =="
                                            $outp.puts "            *src[#{index}]);"
                                        else
                                            $outp.puts "    ZASSERT(!zhasvalidcap(dst[#{index}]));"
                                            $outp.puts "    ZASSERT(!opaque(dst[#{index}]));"
                                        end
                                        index += 1
                                        remaining -= 8
                                    end
                                    if remaining > 0
                                        $outp.puts "    ZASSERT(!zhasvalidcap(dst[#{index}]));"
                                    end
                                    $outp.puts "    size_t index;"
                                    $outp.puts "    for (index = #{(size + offset + 7) & -8};"
                                    $outp.puts "         index--;) {"
                                    $outp.puts "        int** expected ="
                                    $outp.puts "            (index >= #{offset} &&"
                                    $outp.puts "             index < #{offset + size})"
                                    $outp.puts "            ? src : origDst;"
                                    $outp.puts "        ZASSERT(((char*)dst)[index] =="
                                    $outp.puts "                ((char*)expected)[index]);"
                                    $outp.puts "    }"
                                    $outp.puts "    for (index = #{(size + offset + 7) / 8};"
                                    $outp.puts "         index--;) {"
                                    $outp.puts "        ((int*_Atomic*)dst)[index] = zgc_alloc(0);"
                                    $outp.puts "        ZASSERT(src[index] == origSrc[index]);"
                                    $outp.puts "    }"
                                    $outp.puts "}"
                                    $numTestsInFile += 1
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

endFile
