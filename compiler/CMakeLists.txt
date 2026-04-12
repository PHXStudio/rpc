########################################################################
# rpc compiler
########################################################################

# find bison & flex.
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)
bison_target(RPC_PARSER rpc.y "${CMAKE_CURRENT_BINARY_DIR}/rpc.tab.cpp")
if(MSVC)
	flex_target(RPC_SCANNER rpc.l "${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp" COMPILE_FLAGS "--nounistd")
else()
	flex_target(RPC_SCANNER rpc.l "${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp")
endif()
add_flex_bison_dependency(RPC_SCANNER RPC_PARSER)


# sources.
set(PARSER_FILES
	${BISON_RPC_PARSER_OUTPUTS}
	${FLEX_RPC_SCANNER_OUTPUTS}
	rpc.y
	rpc.l
	)
set(SPEC_FILES
	Definition.h
	Enum.h
	Field.h
	Service.h
	Struct.h
	Enum.cpp
	Field.cpp
	Service.cpp
	Struct.cpp
	
	)
set(GEN_FILES
	CppGenerator.h
	PYGenerator.h
	CSGenerator.h
	GoGenerator.h
#    ASGenerator.h

	CppGenerator.cpp
	PYGenerator.cpp
	CSGenerator.cpp
	GoGenerator.cpp
 #   ASGenerator.cpp
	)
set(MISC_FILES
	CodeFile.h
	CodeFile.cpp
	CodeGenerator.h
    Compiler.h
    Compiler.cpp
	Args.h
	Args.cpp
    Main.cpp
	)

source_group( parser FILES ${PARSER_FILES})
source_group( spec FILES ${SPEC_FILES})
source_group( gen FILES ${GEN_FILES})
source_group( misc FILES ${MISC_FILES})

add_executable(rpc ${PARSER_FILES} ${SPEC_FILES} ${GEN_FILES} ${MISC_FILES})
target_include_directories(rpc PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${PROJECT_BINARY_DIR}
)

# install rpc
#TARGET_LINK_LIBRARIES(rpc ${Boost_LIBRARIES})
install(TARGETS rpc DESTINATION ${CMAKE_INSTALL_BINDIR})
