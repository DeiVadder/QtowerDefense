# Passes only if the program dies from an illegal instruction, the trap of UBSan.
execute_process(COMMAND "${PROGRAM}" RESULT_VARIABLE result)
message(STATUS "ubsan_selfcheck ended with: ${result}")
if(NOT result MATCHES "Illegal|0xC000001D|0xc000001d|3221225501")
    message(FATAL_ERROR "expected a trap, got: ${result}")
endif()
