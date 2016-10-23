#create logical links in order to keep legacy names of apps

macro(create_symlink _source _dest)
    set(source ${CMAKE_CURRENT_SOURCE_DIR}/${_source})
    set(dest ${CMAKE_CURRENT_BINARY_DIR}/${_dest})
    list(APPEND symlinks ${dest})
    execute_process(COMMAND ln -sf ${source} ${dest})
endmacro(create_symlink)
