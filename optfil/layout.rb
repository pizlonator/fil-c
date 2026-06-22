#!/usr/bin/env ruby

listAsString = "acl  attr  bash  binutils  bzip2  coreutils  curl  diff  find  flex  gawk
                git  glibc  grep  gzip  icu4c  keyutils  krb5  less  libaudit  libc++
                libedit  libevent  libidn2  libpsl  selinux  libtasn1  libuv  lz4  m4
                make  mg  nghttp2  openssl  openssh  p11-kit  patch  patchelf  pam  pcre2
                pkgconf  procps-ng  psmisc  readline  rsync  sed  sudo  tar  tmux
                unistring  wget  xxhash  xz  zlib  zstd"

list = listAsString.split.sort

class Entry
    attr_reader :name
    attr_accessor :space

    def initialize(name)
        @name = name
        @space = 1
    end

    def normalize
        Entry.new(name)
    end

    def to_s
        result = name
        space.times {
            result += " "
        }
        result
    end

    def length
        name.size + space
    end
end

$rows = [list.map { | x | Entry.new(x) }]

def rowSize(row)
    size = 0
    row.each { | x | size += x.length }
    size
end

def unshiftToNextRow(index)
    if index + 1 == $rows.length
        $rows << []
        raise unless $rows[index + 1]
    end
    $rows[index + 1].unshift($rows[index].pop.normalize)
end

def fixpointColWidth
    result = false
    changed = true
    while changed
        changed = false
        index = 0
        while index < $rows.length
            if rowSize($rows[index]) >= 79
                unshiftToNextRow(index)
                result = changed = true
            end
            index += 1
        end
    end
    result
end

def fixpointEqualRows
    result = false
    changed = true
    while changed
        changed = false
        minRowLength = nil
        index = 0
        while index < $rows.length - 1
            if not minRowLength or $rows[index].length < minRowLength
                minRowLength = $rows[index].length
            end
            index += 1
        end
        if minRowLength
            index = 0
            while index < $rows.length
                if $rows[index].length > minRowLength
                    unshiftToNextRow(index)
                    result = changed = true
                end
                index += 1
            end
        end
    end
    result
end

def fixpointAlignment
    result = false
    changed = true
    while changed
        changed = false
        rowIndex = 0
        while rowIndex < $rows.length - 1
            colIndex = 0
            while colIndex < $rows[rowIndex].length and colIndex < $rows[rowIndex + 1].length
                if $rows[rowIndex][colIndex].length < $rows[rowIndex + 1][colIndex].length
                    $rows[rowIndex][colIndex].space =
                        $rows[rowIndex + 1][colIndex].length - $rows[rowIndex][colIndex].name.length
                    result = changed = true
                elsif $rows[rowIndex][colIndex].length > $rows[rowIndex + 1][colIndex].length
                    $rows[rowIndex + 1][colIndex].space =
                        $rows[rowIndex][colIndex].length - $rows[rowIndex + 1][colIndex].name.length
                    result = changed = true
                end
                raise unless $rows[rowIndex][colIndex].length == $rows[rowIndex + 1][colIndex].length
                colIndex += 1
            end
            rowIndex += 1
        end
    end
    result
end

changed = true
while changed
    changed = false
    changed |= fixpointColWidth
    changed |= fixpointEqualRows
    changed |= fixpointAlignment
    puts
    $rows.each {
        | row |
        puts row.join('')
    }
end


