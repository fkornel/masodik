include_guard(GLOBAL)

include("${CMAKE_ROOT}/Modules/CTest.cmake")

function(masodik_register_ctest target_name)
    if(BUILD_TESTING AND TARGET "${target_name}")
        add_test(NAME "${target_name}" COMMAND "${target_name}")
        set_tests_properties("${target_name}" PROPERTIES TIMEOUT 1800)
    endif()
endfunction()
