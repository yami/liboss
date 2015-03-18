include_directories("${PROJECT_SOURCE_DIR}")
include_directories("unity")

add_definitions("-DUNITY_FLOAT_PRECISION=0.00001" "-DUNITY_FLOAT_TYPE=double")

set(test_common_srcs
  unity/unity.c
  ftutil.c)

add_executable(ft_object ${test_common_srcs} ft_object.c)

target_link_libraries(ft_object liboss)