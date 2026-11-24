function(comtrace_check_admin output_variable)
    execute_process(
        COMMAND powershell.exe -NoProfile -NonInteractive -Command "([Security.Principal.WindowsPrincipal]::new([Security.Principal.WindowsIdentity]::GetCurrent())).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)"
        RESULT_VARIABLE admin_check_status
        OUTPUT_VARIABLE admin_check_output
        ERROR_VARIABLE admin_check_error
    )
    if(NOT admin_check_status EQUAL 0)
        message(FATAL_ERROR "Unable to determine whether this test process is elevated: ${admin_check_error}")
    endif()
    string(STRIP "${admin_check_output}" admin_check_output)
    if(admin_check_output STREQUAL "True")
        set(${output_variable} TRUE PARENT_SCOPE)
    else()
        set(${output_variable} FALSE PARENT_SCOPE)
    endif()
endfunction()
