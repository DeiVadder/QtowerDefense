# Plays through stdin and stdout of qtd_headless, as an outside agent would. With both kinds of line
# ends, an empty line, and a Ctrl-Z, which must not end the input.
string(ASCII 26 ctrlZ)
set(requests "${CMAKE_CURRENT_BINARY_DIR}/control_requests.txt")
foreach(style UNIX DOS)
    file(CONFIGURE OUTPUT "${requests}" NEWLINE_STYLE ${style} @ONLY CONTENT
"{\"action\":\"startMap\",\"map\":\"${MAP}\",\"seed\":\"7\"}

{\"action\":\"buildTower\",\"slot\":0,\"towerType\":0}
{\"action\":\"observe\"}${ctrlZ} garbage
{\"action\":\"advance\",\"ticks\":10}
")
    execute_process(COMMAND "${PROGRAM}" --control
        INPUT_FILE "${requests}" OUTPUT_VARIABLE answers RESULT_VARIABLE result)
    file(REMOVE "${requests}")
    if(NOT result EQUAL 0 OR NOT answers MATCHES "\"result\":\"accepted\"")
        message(FATAL_ERROR "${style}: exit ${result}, answers: ${answers}")
    endif()
    if(NOT answers MATCHES "\"error\"" OR NOT answers MATCHES "\"gold\":70[^\n]*\"tick\":10")
        message(FATAL_ERROR "${style}: unexpected answers: ${answers}")
    endif()
endforeach()
