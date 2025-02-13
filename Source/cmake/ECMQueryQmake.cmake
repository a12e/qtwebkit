find_package(Qt6 REQUIRED COMPONENTS Core QUIET)

set(_qmake_executable_default "qmake-Qt6")
if (TARGET Qt6::qmake)
    get_target_property(_qmake_executable_default Qt6::qmake LOCATION)
endif()
set(QMAKE_EXECUTABLE ${_qmake_executable_default}
    CACHE FILEPATH "Location of the Qt6 qmake executable")

# This is not public API (yet)!
function(query_qmake result_variable qt_variable)
    execute_process(
        COMMAND ${QMAKE_EXECUTABLE} -query "${qt_variable}"
        RESULT_VARIABLE return_code
        OUTPUT_VARIABLE output
    )
    if(return_code EQUAL 0)
        string(STRIP "${output}" output)
        file(TO_CMAKE_PATH "${output}" output_path)
        set(${result_variable} "${output_path}" PARENT_SCOPE)
    else()
        message(WARNING "Failed call: ${QMAKE_EXECUTABLE} -query \"${qt_variable}\"")
        message(FATAL_ERROR "QMake call failed: ${return_code}")
    endif()
endfunction()
