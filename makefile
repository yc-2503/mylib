CFLAGS = -g -lpthread -rdynamic
app:main.o Queue.o Console.o log.o trace.o MemPool.o threadTool.o md5.o
	gcc main.o Queue.o MemPool.o Console.o trace.o log.o threadTool.o md5.o -lpthread -rdynamic -g -o app
.PHONY:clean 
clean:
	-rm app
	-rm *.o
#echo "$(shell md5sum app  | awk '{print $1}')">> app
