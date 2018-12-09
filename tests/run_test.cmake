#
# CMake script to run a single test case
#
message(STATUS "Running GrateRipCLI test")
message(STATUS "  Test run directory: ${TEST_RUN_DIR}")
message(STATUS "  Test src directory: ${TEST_SRC_DIR}")
message(STATUS "  Test binary: ${TEST_BINARY}")
message(STATUS "  Compare binary: ${COMPARE_BINARY}")

#
# number of steps to run the test for
#
set(TEST_NUM_STEPS 800)

#
# make the test directory
#
execute_process(COMMAND ${CMAKE_COMMAND} -E remove_directory ${TEST_RUN_DIR})
execute_process(COMMAND ${CMAKE_COMMAND} -E make_directory ${TEST_RUN_DIR})

#
# copy input files
#
file(COPY ${TEST_SRC_DIR}/Input_Rip1_equil_1938.dat DESTINATION ${TEST_RUN_DIR})
file(COPY ${TEST_SRC_DIR}/hydro_series.dat DESTINATION ${TEST_RUN_DIR})
file(COPY ${TEST_SRC_DIR}/sed_series.dat DESTINATION ${TEST_RUN_DIR})
file(COPY ${TEST_SRC_DIR}/test_out.xml DESTINATION ${TEST_RUN_DIR})

#
# run the code
#
execute_process(
    COMMAND ${CMAKE_COMMAND} -E chdir ${TEST_RUN_DIR} ${TEST_BINARY} ${TEST_NUM_STEPS}
    RESULT_VARIABLE status
)
if (status)
    message(FATAL_ERROR "Error running GrateRipCLI: '${status}'")
endif (status)

#
# check the results
#
#execute_process(
#    COMMAND ${CMAKE_COMMAND} -E compare_files ${TEST_RUN_DIR}/Run_Results.txt
#                                              ${TEST_SRC_DIR}/Run_Results_Ref800.txt
#    RESULT_VARIABLE status
#)
execute_process(
    COMMAND ${COMPARE_BINARY} ${TEST_SRC_DIR}/Run_Results_Ref800.txt
                              ${TEST_RUN_DIR}/Run_Results.txt
    RESULT_VARIABLE status
)
if (status)
    message(FATAL_ERROR "Output file do not match: '${status}'")
endif (status)
