FILE(REMOVE_RECURSE
  "CMakeFiles/check_style_data"
)

# Per-language clean rules from dependency scanning.
FOREACH(lang)
  INCLUDE(CMakeFiles/check_style_data.dir/cmake_clean_${lang}.cmake OPTIONAL)
ENDFOREACH(lang)
