if(CONFIG_ZMK_INPUT_PROCESSOR_THRESHOLD_LAYER)
  target_sources(app PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../src/input_processor_threshold_layer.c
  )
endif()
