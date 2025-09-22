set(LIB_SOURCES
  money.c
)

set(MAIN_SOURCES
  main.c
)

set(HEADERS 
  ${CONFIG_HEADER}
  money.h
)

add_library(money STATIC ${LIB_SOURCES} ${HEADERS})

add_executable(main ${HEADERS} ${MAIN_SOURCES})
target_link_libraries(main money)

install(TARGETS money
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib)

install(FILES ${CMAKE_CURRENT_SOURCE_DIR}/money.h DESTINATION include)
