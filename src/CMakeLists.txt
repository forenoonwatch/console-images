target_sources(${PROJECT_NAME} PRIVATE
	"${CMAKE_CURRENT_SOURCE_DIR}/main.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/stb_image.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/win_common.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/console.cpp"
	"${CMAKE_CURRENT_SOURCE_DIR}/graphic.cpp"
)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
