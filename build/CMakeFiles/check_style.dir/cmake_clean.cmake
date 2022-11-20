FILE(REMOVE_RECURSE
  "CMakeFiles/check_style"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang)
  INCLUDE(CMakeFiles/check_style.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
