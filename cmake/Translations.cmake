# The texts to translate: "update-po" extracts them from the sources into
# po/freeserf.pot (xgettext) and brings the translations po/*.po up to
# date with it (msgmerge).

find_program(XGETTEXT_EXECUTABLE xgettext)
find_program(MSGMERGE_EXECUTABLE msgmerge)

if(XGETTEXT_EXECUTABLE AND MSGMERGE_EXECUTABLE)
  file(GLOB TRANSLATED_SOURCES RELATIVE "${PROJECT_SOURCE_DIR}"
       "${PROJECT_SOURCE_DIR}/src/*.cc" "${PROJECT_SOURCE_DIR}/src/*.h")
  list(SORT TRANSLATED_SOURCES)
  file(GLOB PO_FILES "${PROJECT_SOURCE_DIR}/po/*.po")

  set(UPDATE_PO_COMMANDS
      COMMAND "${XGETTEXT_EXECUTABLE}"
              --from-code=UTF-8 --language=C++
              --keyword=_ --keyword=N_ --keyword=C_:1c,2 --keyword=NC_:1c,2
              --add-comments=TRANSLATORS: --sort-by-file
              --package-name=FreeSerf --package-version=${PROJECT_VERSION}
              --msgid-bugs-address=https://github.com/freeserf/freeserf/issues
              --output=po/freeserf.pot ${TRANSLATED_SOURCES})
  foreach(PO_FILE ${PO_FILES})
    list(APPEND UPDATE_PO_COMMANDS
         COMMAND "${MSGMERGE_EXECUTABLE}" --quiet --update --backup=none
                 "${PO_FILE}" po/freeserf.pot)
  endforeach()

  add_custom_target(update-po ${UPDATE_PO_COMMANDS}
                    WORKING_DIRECTORY "${PROJECT_SOURCE_DIR}"
                    COMMENT "Extracting the texts to translate"
                    VERBATIM)
endif()
