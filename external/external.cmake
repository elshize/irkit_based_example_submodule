#execute_process(COMMAND git submodule update --init external/irkit
#                WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR})
add_subdirectory(external/irkit EXCLUDE_FROM_ALL)
