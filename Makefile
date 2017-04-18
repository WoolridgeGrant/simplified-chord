.PHONY : all clean check run
.ONESHELL:

CHECK_PATCH=./checkpatch.pl --no-tree

all :	chord1 run clean

%.o : %.c
	mpicc -Wall -pthread -c $<

implem1: chord1 run clean

implem2: chord2 run clean

chord1: ./implementation1/chord.c
	mpicc -Wall -pthread -lrt  $^ -o chord

chord2: ./implementation2/chord.c
	mpicc -Wall -pthread -lm -lrt  $^ -o chord

run :
	mpirun -np 7 chord

check : 
	for f in *.c *.h ; do
		$(CHECK_PATCH) -f $$f
	done

clean :
	rm -rf *.o chord
