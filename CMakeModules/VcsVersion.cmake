set(VCS_VERSION_FILE "${PROJECT_BINARY_DIR}/version-vcs.h")
if(EXISTS ${VCS_VERSION_FILE})
    execute_process(COMMAND grep "define VERSION_VCS " "${VCS_VERSION_FILE}"
                    COMMAND sed "s/\"//g"
                    COMMAND cut -d\  -f3
                    COMMAND tr "\n" " "
                    OUTPUT_VARIABLE FREESERF_VERSION)
endif()

execute_process(COMMAND git describe --always --tags --dirty
                COMMAND tr "\n" " "
                OUTPUT_VARIABLE VCS_VERSION)

if(NOT "${FREESERF_VERSION}" STREQUAL "${VCS_VERSION}")
    message("-- Updating version file.")
    file(WRITE "${VCS_VERSION_FILE}" "#ifndef VERSION_VCS_H\n#define VERSION_VCS_H\n\n#define VERSION_VCS \"${VCS_VERSION}\"\n\n#endif /* VERSION_VCS_H */\n")
endif()
