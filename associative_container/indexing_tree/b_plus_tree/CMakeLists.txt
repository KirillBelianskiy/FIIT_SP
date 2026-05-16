add_subdirectory(tests)

add_library(
        sys_prog_assctv_cntnr_indxng_tr_b_pls_tr
        include/b_plus_tree.h
        src/hhh.cpp)

target_include_directories(
        sys_prog_assctv_cntnr_indxng_tr_b_pls_tr
        PUBLIC
        ./include)

target_link_libraries(
        sys_prog_assctv_cntnr_indxng_tr_b_pls_tr
        PUBLIC
        sys_prog_assctv_cntnr
        Boost::container)