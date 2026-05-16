add_executable(
        sys_prog_allctr_allctr_rb_tr_tests
        allocator_red_black_tree_tests.cpp)

target_link_libraries(
        sys_prog_allctr_allctr_rb_tr_tests
        PRIVATE
        gtest_main)
target_link_libraries(
        sys_prog_allctr_allctr_rb_tr_tests
        PRIVATE
        sys_prog_allctr_allctr_rb_tr
        sys_prog_allctr_allctr_glbl_hp)

add_executable(
        demo_rb_tree_allocator
        demo_rb_tree_allocator.cpp)

target_link_libraries(
        demo_rb_tree_allocator
        PRIVATE
        sys_prog_allctr_allctr_rb_tr
        sys_prog_allctr_allctr_glbl_hp)

add_executable(
        simple_demo
        simple_demo.cpp)

target_link_libraries(
        simple_demo
        PRIVATE
        sys_prog_allctr_allctr_rb_tr
        sys_prog_allctr_allctr_glbl_hp)