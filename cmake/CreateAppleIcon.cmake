function(ScaleImage size png target_path)
  execute_process(COMMAND sips -z ${ARGV0} ${ARGV0} ${ARGV1} --out ${ARGV2}/icon_${ARGV0}x${ARGV0}.png)
endfunction(ScaleImage)

function(ScaleImage2x size size2x png target_path)
  execute_process(COMMAND sips -z ${ARGV0} ${ARGV0} ${ARGV2} --out ${ARGV3}/icon_${ARGV1}x${ARGV1}@2x.png)
endfunction(ScaleImage2x)

function(CreateAppleIcon png target)
  message("CreateAppleIcon(" ${ARGV0} " " ${ARGV1} ")")

  set(ICONSET_PATH ${CMAKE_CURRENT_BINARY_DIR}/Iconset.iconset)
  file(MAKE_DIRECTORY ${ICONSET_PATH})

  message("ICONSET_PATH: " ${ICONSET_PATH})

  ScaleImage(16 ${ARGV0} ${ICONSET_PATH})
  ScaleImage2x(32 16 ${ARGV0} ${ICONSET_PATH})
  ScaleImage(32 ${ARGV0} ${ICONSET_PATH})
  ScaleImage2x(64 32 ${ARGV0} ${ICONSET_PATH})
  ScaleImage(64 ${ARGV0} ${ICONSET_PATH})
  ScaleImage2x(128 64 ${ARGV0} ${ICONSET_PATH})
  ScaleImage(128 ${ARGV0} ${ICONSET_PATH})
  ScaleImage2x(256 128 ${ARGV0} ${ICONSET_PATH})
  ScaleImage(256 ${ARGV0} ${ICONSET_PATH})
  ScaleImage2x(512 256 ${ARGV0} ${ICONSET_PATH})
  ScaleImage(512 ${ARGV0} ${ICONSET_PATH})
  ScaleImage2x(1024 512 ${ARGV0} ${ICONSET_PATH})

  execute_process(COMMAND iconutil --convert icns ${ICONSET_PATH} --output ${ARGV1})
endfunction(CreateAppleIcon)
