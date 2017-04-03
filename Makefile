.PHONY : all clean check run
.ONESHELL:

CHECK_PATCH=./checkpatch.pl --no-tree

all :	chord run clean

%.o : %.c
	mpicc -Wall -pthread -c $<

chord: chord.c
	mpicc -Wall -pthread -lrt  $^ -o $@

run :
	mpirun -np 7 chord

check : 
	for f in *.c *.h ; do
		$(CHECK_PATCH) -f $$f
	done

clean :
	rm -rf *.o chord
