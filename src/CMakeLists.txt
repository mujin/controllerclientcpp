include_directories(${MUJINCLIENT_INCLUDE_LOCAL_DIRS} ${CURL_INCLUDE_DIRS})
link_directories(${MUJINCLIENT_LINK_DIRS})

add_library(mujincontrollerclient SHARED mujincontrollerclient.cpp)
set_target_properties(mujincontrollerclient PROPERTIES COMPILE_FLAGS "${EXTRA_COMPILE_FLAGS} ${Boost_CFLAGS} -DMUJINCLIENT_DLL_EXPORTS -DMUJINCLIENT_DLL" LINK_FLAGS ""
  OUTPUT_NAME mujincontrollerclient${MUJINCLIENT_LIBRARY_SUFFIX}
  SOVERSION 0 # always have it 0 since we're including the soversion as part of the library name
  VERSION ${MUJINCLIENT_VERSION}
  CLEAN_DIRECT_OUTPUT 1
  )
target_link_libraries(mujincontrollerclient ${CURL_LIBRARIES})
install(TARGETS mujincontrollerclient RUNTIME DESTINATION bin LIBRARY DESTINATION bin ARCHIVE DESTINATION lib${LIB_SUFFIX})

add_executable(mujinclienttest mujinclienttest.cpp)
set_target_properties(mujinclienttest PROPERTIES COMPILE_FLAGS "${Boost_CFLAGS} -DMUJINCLIENT_CORE_DLL" LINK_FLAGS "")
add_dependencies(mujinclienttest mujincontrollerclient)
target_link_libraries (mujinclienttest ${Boost_DATE_TIME_LIBRARY} ${Boost_THREAD_LIBRARY} mujincontrollerclient)
install(TARGETS mujinclienttest DESTINATION bin)
