set(INPUT_FILE ${SOURCE_DIR}/cmake/version.h.in)
set(OUTPUT_FILE ${BINARY_DIR}/generated/version.h)

execute_process(
    COMMAND git rev-list HEAD --count
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_COUNT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

execute_process(
    COMMAND git rev-parse --verify --short HEAD
    WORKING_DIRECTORY ${SOURCE_DIR}
    OUTPUT_VARIABLE GIT_COMMIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

if(NOT GIT_COMMIT_COUNT)
    set(GIT_COMMIT_COUNT 0)
endif()

if(NOT GIT_COMMIT_HASH)
    set(GIT_COMMIT_HASH git)
endif()

configure_file(${INPUT_FILE} ${OUTPUT_FILE})
