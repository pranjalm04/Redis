include_guard(GLOBAL)

function(redis_set_project_warnings target warnings_as_errors)
  if(MSVC)
    set(WARNINGS
      /W4
      /wd4267
      /wd4996
    )
    if(warnings_as_errors)
      list(APPEND WARNINGS /WX)
    endif()
  else()
    set(WARNINGS
      -Wall
      -Wextra
      -Wpedantic
      -Wconversion
      -Wsign-conversion
      -Wshadow
      -Wformat=2
      -Wnull-dereference
      -Wdouble-promotion
      -Wimplicit-fallthrough
    )
    if(warnings_as_errors)
      list(APPEND WARNINGS -Werror)
    endif()
  endif()

  target_compile_options(${target} PRIVATE ${WARNINGS})
endfunction()
