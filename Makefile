all: cwl_progress

cwl_progress: cwl_progress.c sds.o cJSON.o
	$(CC) -o cwl_progress cwl_progress.c sds.o cJSON.o -Wall -std=c99 -pedantic -O2 -lcurl

clean:
	rm -f cwl_progress