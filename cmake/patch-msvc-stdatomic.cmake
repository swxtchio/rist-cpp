# MSVC 14.52 rejects librist's compat/msvc/stdatomic.h: its preprocessor no longer accepts the undefined
# function-like macro __has_include_next after a false "defined(__clang__) &&" (C7780, then C1017).
# clang-cl always ships stdatomic.h, so the clang check alone keeps the same choice.
set(header "${SOURCE_DIR}/compat/msvc/stdatomic.h")
file(READ "${header}" text)
string(REPLACE "#if defined(__clang__) && __has_include_next(<stdatomic.h>)" "#if defined(__clang__)" text "${text}")
file(WRITE "${header}" "${text}")
