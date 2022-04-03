cmake_minimum_required(VERSION 3.20)
project(st LANGUAGES C CXX VERSION 0.8.4)
set(CMAKE_COLOR_MAKEFILE ON)
include(FetchContent)

if(UNIX AND NOT APPLE)
    set(LINUX TRUE)
endif()

FetchContent_Declare(
    ljh
    GIT_REPOSITORY https://github.com/Link1J/ljh.git
)
FetchContent_MakeAvailable(ljh)

add_executable(st WIN32
    src/st.cpp
    src/utf8.cpp

    src/boxdraw_data.c

    src/con/term.cpp
    src/con/selection.cpp
    src/con/tty.cpp
    src/con/escape.cpp
)

target_compile_definitions(st PRIVATE "VERSION=\"${PROJECT_VERSION}\"")
target_include_directories(st PRIVATE src)
set_target_properties(st PROPERTIES
    C_STANDARD 17
    CXX_STANDARD 23
    CXX_EXTENSIONS NO
)
target_link_libraries(st PRIVATE
    ljh::ljh
)

if(WIN32)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        target_compile_options(st PUBLIC
            -Xclang -fcoroutines-ts
        )
        target_compile_definitions(st PRIVATE
            __cpp_lib_coroutine
        )
    endif()

    target_sources(st PRIVATE
        src/win/wcwidth.cpp
        src/win/x.cpp
    )
    target_compile_definitions(st PRIVATE 
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
        strdup=_strdup
    )
    target_link_libraries(st PRIVATE
        Dwrite.lib
        Kernel32.lib
    )
endif()

if(LINUX)
    target_sources(st PRIVATE
        src/linux/boxdraw.cpp
        src/linux/hb.cpp
        src/linux/x.cpp
    )

    find_package(PkgConfig REQUIRED)
    find_package(X11 REQUIRED)
    find_package(Fontconfig REQUIRED)
    find_package(Freetype REQUIRED)

    pkg_check_modules(harfbuzz harfbuzz)
    target_include_directories(st PRIVATE ${harfbuzz_INCLUDE_DIRS})
    target_link_libraries(st PRIVATE
        X11::X11 X11::Xft X11::Xrender X11::Xcursor
        Fontconfig::Fontconfig Freetype::Freetype
        m rt util ${harfbuzz_LIBRARIES}
    )
    target_compile_options(st PRIVATE -Werror -fdiagnostics-color)

    target_compile_options(st PRIVATE -fno-omit-frame-pointer -fsanitize=address)
    target_link_options(st PRIVATE -fno-omit-frame-pointer -fsanitize=address)

    include(GNUInstallDirs)

    install(TARGETS st DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES assets/st.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
    install(FILES assets/st.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(CODE "execute_process(COMMAND tic -sx ${CMAKE_SOURCE_DIR}/assets/st.info)")
endif()