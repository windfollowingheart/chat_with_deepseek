.PHONY:server
server: main.c ./threadpool/threadpool.h ./http/http_conn.cpp \
	./http/http_conn.h ./lock/locker.h \
	./log/log.cpp ./log/log.h ./log/block_queue.h \
	./CGImysql/sql_connection_pool.cpp ./CGImysql/sql_connection_pool.h \
	./CGIredis/redis_connection_pool.cpp ./CGIredis/redis_connection_pool.h \
	./CGIrabbitmq/rabbitmq_connection_pool.cpp ./CGIrabbitmq/rabbitmq_connection_pool.h \
	./CGIuploadfile/uploadfile_connection_pool.cpp ./CGIuploadfile/uploadfile_connection_pool.h \
	./CGIgetparseresult/getparseresult_connection_pool.cpp ./CGIgetparseresult/getparseresult_connection_pool.h \
	./token/tokenpool.h ./token/tokenpool.cpp
	g++ -o server main.c \
	./threadpool/threadpool.h \
	./http/http_conn.cpp ./http/http_conn.h \
	./lock/locker.h ./log/log.cpp \
	./log/log.h \
	./CGImysql/sql_connection_pool.cpp ./CGImysql/sql_connection_pool.h \
	./CGIredis/redis_connection_pool.cpp ./CGIredis/redis_connection_pool.h \
	./CGIrabbitmq/rabbitmq_connection_pool.cpp ./CGIrabbitmq/rabbitmq_connection_pool.h \
	./CGIuploadfile/uploadfile_connection_pool.cpp ./CGIuploadfile/uploadfile_connection_pool.h \
	./CGIgetparseresult/getparseresult_connection_pool.cpp ./CGIgetparseresult/getparseresult_connection_pool.h \
	./token/tokenpool.h ./token/tokenpool.cpp \
	-lpthread -lmysqlclient -lssl -lcrypto -lhiredis -lSimpleAmqpClient \
	-lboost_system -lboost_random -lfmt


clean:
	rm  -r server
