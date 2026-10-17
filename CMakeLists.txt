cmake_minimum_required(VERSION 3.15)

project(preflate LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(SRCDIR ${CMAKE_CURRENT_SOURCE_DIR})
set(OBJDIR ${CMAKE_CURRENT_BINARY_DIR})

include_directories(AFTER ${SRCDIR} ${SRCDIR}/support)

function(add_stem2file VAR pattern stem)
    set(TMPV ${${VAR}})
    foreach(IND ${stem})
        string(REGEX REPLACE "^[ ]*([0-9a-zA-Z_%]+)[ ]*$" \\1 IND ${IND})
        string(REPLACE "%STEM%" ${IND} NEXT ${pattern})
        set(TMPV "${TMPV};${NEXT}")
    endforeach()
    set(${VAR} "${TMPV}" PARENT_SCOPE)
endfunction()

set(LIB_SRC "")
add_stem2file(LIB_SRC "${SRCDIR}/preflate_%STEM%.cpp" 
              "block_decoder;block_reencoder;block_trees;\
               complevel_estimator;constants;decoder;hash_chain;info;\
               parameter_estimator;parser_config;predictor_state;reencoder;\
               seq_chain;statistical_codec;statistical_model;token;\
               token_predictor;tree_predictor")
add_stem2file(LIB_SRC "${SRCDIR}/support/%STEM%.cpp"
              "arithmetic_coder;array_helper;bit_helper;bitstream;const_division;\
               filestream;huffman_decoder;huffman_encoder;huffman_helper;memstream;\
               outputcachestream;task_pool")

set(LIB_HDR "")
add_stem2file(LIB_HDR "${SRCDIR}/preflate_%STEM%.h" 
              "block_decoder;block_reencoder;block_trees;checker;\
               complevel_estimator;constants;decoder;hash_chain;info;\
               parameter_estimator;parser_config;predictor_state;reencoder;\
               seq_chain;statistical_codec;statistical_model;token;\
               token_predictor;tree_predictor")
add_stem2file(LIB_HDR "${SRCDIR}/support/%STEM%.h" 
              "arithmetic_coder;array_helper;bitstream;bit_helper;const_division;\
               filestream;huffman_decoder;huffman_encoder;huffman_helper;\
               memstream;outputcachestream;task_pool")

add_definitions(-DZ_SOLO -DNO_GZIP -DNOMINMAX)

add_library(preflate_dll SHARED dllmain.cpp ${LIB_SRC} ${LIB_HDR})
set_target_properties(preflate_dll PROPERTIES
    OUTPUT_NAME "preflate_dll"
    PREFIX ""
)

if(MSVC)
    target_compile_options(preflate_dll PRIVATE /MT /EHsc /O2 /Ob3 /Oi /Ot /Gy /GL /W3 /arch:AVX2 /GS- /Gw)
    target_link_options(preflate_dll PRIVATE /LTCG /OPT:REF /OPT:ICF)
endif()
