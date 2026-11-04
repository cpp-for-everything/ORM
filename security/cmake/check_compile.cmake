# Compiles one translation unit and asserts the outcome.
#
#   EXPECT=pass  the file must compile.
#   EXPECT=fail  the file must NOT compile, and its diagnostic must match REGEX,
#                so that it is rejected for the reason under test and not some
#                unrelated one.
#
# The error lines are written to LOG before any assertion, so the evidence
# exists whether the check passes or fails. Absolute paths are replaced with
# <repo> so the logs are stable across machines.
string(REPLACE "|" ";" _incs "${INCLUDES}")
set(_args -std=c++23)
foreach(_d IN LISTS _incs)
    list(APPEND _args "-I${_d}")
endforeach()

get_filename_component(_logdir "${LOG}" DIRECTORY)
file(MAKE_DIRECTORY "${_logdir}")

execute_process(
    COMMAND "${CXX}" ${_args} -c "${SRC}" -o "${LOG}.o"
    RESULT_VARIABLE _rc
    OUTPUT_VARIABLE _out
    ERROR_VARIABLE  _err)
file(REMOVE "${LOG}.o")

string(REPLACE "${ROOT}" "<repo>" _err_clean "${_err}")
string(REPLACE "\n" ";" _lines "${_err_clean}")
set(_errors "")
foreach(_l IN LISTS _lines)
    if(_l MATCHES "error")
        string(APPEND _errors "${_l}\n")
    endif()
endforeach()

get_filename_component(_name "${SRC}" NAME_WE)
if(_rc EQUAL 0)
    set(_result "compiled")
else()
    set(_result "rejected")
endif()

if(EXPECT STREQUAL "pass")
    set(_ok FALSE)
    if(_rc EQUAL 0)
        set(_ok TRUE)
    endif()
else()
    set(_ok FALSE)
    if(NOT _rc EQUAL 0 AND _err MATCHES "${REGEX}")
        set(_ok TRUE)
    endif()
endif()

file(WRITE "${LOG}"
"case:     ${_name}\n"
"expected: ${EXPECT}\n"
"result:   ${_result}\n"
"regex:    ${REGEX}\n"
"verdict:  ${_ok}\n"
"compiler: ${CXX_ID}\n"
"\n${_errors}")

if(EXPECT STREQUAL "pass" AND NOT _ok)
    message(FATAL_ERROR "${_name} was expected to compile and did not:\n${_errors}")
endif()
if(EXPECT STREQUAL "fail" AND _rc EQUAL 0)
    message(FATAL_ERROR "${_name} COMPILED, but it must be rejected. The guarantee it tests does not hold.")
endif()
if(EXPECT STREQUAL "fail" AND NOT _ok)
    message(FATAL_ERROR "${_name} was rejected, but not for the reason under test.\n"
                        "Expected a diagnostic matching: ${REGEX}\nGot:\n${_errors}")
endif()
