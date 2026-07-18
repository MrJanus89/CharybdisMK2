if(CONFIG_ZMK_INPUT_PROCESSOR_THRESHOLD_LAYER)
  target_sources(app PRIVATE ${CMAKE_CURRENT_LIST_DIR}/../src/threshold_layer_v7.c)
endif()
