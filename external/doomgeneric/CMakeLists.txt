file(GLOB DOOM_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/*.c")

# Exclude platform-specific implementations — we provide our own via QgsDoomEngine.
# i_system.c is also excluded: QgsDoomEngine provides all its symbols, with a
# custom I_Error that throws a C++ exception instead of calling exit(-1).
list(FILTER DOOM_SOURCES EXCLUDE REGEX
  ".*(doomgeneric_(sdl|win|linuxvt|emscripten|allegro|soso|sosox|xlib)|i_(sdl|allegro)(sound|music)|i_cdmus|i_system)\\.c$"
)

add_library(doomgeneric STATIC ${DOOM_SOURCES})
target_include_directories(doomgeneric PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_options(doomgeneric PRIVATE -w -fPIC)
set_target_properties(doomgeneric PROPERTIES LINKER_LANGUAGE C)
