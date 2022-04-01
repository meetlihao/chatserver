#定义SRC_LIST变量，它包含该目录下所有源文件
aux_source_directory(. SRC_LIST)
aux_source_directory(./db DB_SRC_LIST)
aux_source_directory(./model MODEL_SRC_LIST)
aux_source_directory(./redis REDIS_SRC_LIST)

#指定生成的可执行文件ChatServer
add_executable(ChatServer ${SRC_LIST} ${DB_SRC_LIST} ${MODEL_SRC_LIST} ${REDIS_SRC_LIST})

#指定该可执行文件需要链接的库文件
target_link_libraries(ChatServer muduo_net muduo_base mysqlclient  hiredis pthread)

#配置编译选项
add_compile_options(-std=c++11 -Wall -g)


