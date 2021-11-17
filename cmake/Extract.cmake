function(extract_file2 filename extract_dir)
  message(STATUS "Extract to ${extract_dir} ...")

  # cmake -E tar -xf filename.tgz
  set(temp_dir ${CMAKE_BINARY_DIR}/tmp_for_extract.dir)
  if(EXISTS ${temp_dir})
    file(REMOVE_RECURSE ${temp_dir})
  endif()
  file(MAKE_DIRECTORY ${temp_dir})
  execute_process(COMMAND ${CMAKE_COMMAND} -E tar -xf ${filename}
                  WORKING_DIRECTORY ${temp_dir})

  file(GLOB contents "${temp_dir}/*")
  list(LENGTH contents n)
  if(NOT n EQUAL 1 OR NOT IS_DIRECTORY "${contents}")
    set(contents "${temp_dir}")
  endif()

  get_filename_component(contents ${contents} ABSOLUTE)
  file(INSTALL "${contents}/" DESTINATION ${extract_dir})
  
  file(REMOVE_RECURSE ${temp_dir})
endfunction()