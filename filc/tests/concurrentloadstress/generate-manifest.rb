puts "libraries:"
1000.times {
    | index |
    puts "  gctor#{index}.so:"
    puts "    isBundle: true"
    puts "    files:"
    puts "      - gctor.c"
}

