#execute_process(COMMAND git submodule update --init
#                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/..
#                OUTPUT_QUIET)
add_subdirectory(external/irkit EXCLUDE_FROM_ALL)
