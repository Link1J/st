cmake_minimum_required(VERSION 3.14)
project(st LANGUAGES C VERSION 0.8.4)
set(CMAKE_COLOR_MAKEFILE ON)

find_package(PkgConfig REQUIRED)
find_package(X11 REQUIRED)
find_package(Fontconfig REQUIRED)
find_package(Freetype REQUIRED)

pkg_check_modules(harfbuzz harfbuzz)

add_executable(st src/boxdraw.c src/hb.c src/st.c src/x.c)
target_compile_definitions(st PRIVATE "VERSION=\"${PROJECT_VERSION}\"")
target_include_directories(st PRIVATE ${harfbuzz_INCLUDE_DIRS} src)
target_link_libraries(st PRIVATE
    X11::X11 X11::Xft X11::Xrender X11::Xcursor
    Fontconfig::Fontconfig Freetype::Freetype
    m rt util ${harfbuzz_LIBRARIES}
)
target_compile_options(st PRIVATE -Werror -fdiagnostics-color)

include(GNUInstallDirs)

install(TARGETS st DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES assets/st.1 DESTINATION ${CMAKE_INSTALL_MANDIR}/man1)
install(FILES assets/st.desktop DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
install(CODE "execute_process(COMMAND tic -sx ${CMAKE_SOURCE_DIR}/assets/st.info)")