#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <time.h>

#define NB_SITE   6
#define NB_DATA   64 /*On suppose que les id des donnees s'etendent de 0 a 64*/
#define TAGINIT 0

struct successor{
	int id_succ;
	int rang_succ;
};

int id_peers[NB_SITE];
struct successor succ[NB_SITE];
int frd[NB_SITE]; //frd[i] est l'id de la premiere donnee dont est responsable le site id_peers[i]

int id_ajout = -1;

//Reste a definir le random pour l'ajout et le frd
int id_generator(){
	int r = 0;
	int i = 0, j = 0;
	int exist = 0;	

	//Definition des id des differents sites de maniere aleatoire
	while(i < NB_SITE){
			r = rand() % NB_DATA; //à revoir
			for(j = 0; j < i; j++)
				if(r == id_peers[j])
					exist = 1;

			if(exist){
				exist = 0;
				continue;
			}

			id_peers[i] = r;
			i++;
	}

	//Definition du successeur de chacun des sites
	for(i = 0; i < NB_SITE; i++){
		//Attribution des premieres donnees dont chaque site est responsable 
		//L'id de la premiere donnee dont chaque site est responsable est egale a la sienne
		frd[i] = id_peers[i];
		
		succ[i].id_succ = NB_DATA+1; //Ils seront tous plus petits que NB_DATA+1

		for(j = 0; j < NB_SITE; j++){
			//Le plus petit parmi tout ceux plus grand que lui
			if ((i != j) && (id_peers[i] < id_peers[j]) && (succ[i].id_succ > id_peers[j])){
				succ[i].id_succ = id_peers[j];
				succ[i].rang_succ = j; 
			}
		}

		//Les deux noeuds qui sont tout au debut du cercle et tout a la fin du cercle
		if(succ[i].id_succ == NB_DATA+1){
			//Recherche du plus petit des id chord
			succ[i].id_succ = id_peers[0];
			succ[i].rang_succ = 0;
			for(j = 1; j < NB_SITE; j++){
				if(succ[i].id_succ > id_peers[j]){
					succ[i].id_succ = id_peers[j];
					succ[i].rang_succ = j;
				}
			}
		}
	}
}

void simulateur(void) {
	int i;
	
	srand(time(NULL));

	id_generator();
		               
	for(i=0; i<NB_SITE; i++){
		//i car le dernier processus est l'initiateur
		MPI_Ssend(&id_peers[i], 1, MPI_INT, i, TAGINIT, MPI_COMM_WORLD);  			//son propre id  
		MPI_Ssend(&succ[i].id_succ, 1, MPI_INT, i, succ[i].rang_succ, MPI_COMM_WORLD);	//son successeur (envoi du rang MPI du successeur via le TAG)
		MPI_Ssend(&frd[i], 1, MPI_INT, i, TAGINIT, MPI_COMM_WORLD);			//l'id de la premiere donnee dont il responsable
	}
}

void test_initialisation(int rang){
	int id_chord;
	int id_succ;
	int rang_succ;
	int frd;
	MPI_Status status;
	MPI_Recv(&id_chord, 1, MPI_INT, NB_SITE, TAGINIT, MPI_COMM_WORLD, &status);
	MPI_Recv(&id_succ, 1, MPI_INT, NB_SITE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	rang_succ = status.MPI_TAG;
	MPI_Recv(&frd, 1, MPI_INT, NB_SITE, TAGINIT, MPI_COMM_WORLD, &status);

	printf("Mon rang : %d, Mon id_chord : %d, Mon successeur : %d, rang succ: %d\n", rang, id_chord, id_succ, rang_succ);
}


int main (int argc, char* argv[]) {
	int nb_proc,rang;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &nb_proc);

	if (nb_proc != NB_SITE+1) {
		printf("Nombre de processus incorrect !\n");
		MPI_Finalize();
		exit(2);
	}

	MPI_Comm_rank(MPI_COMM_WORLD, &rang);

	//L'initiateur va etre le dernier processus pour des raisons pratiques (par rapport a l'indice la boucle
	//De cette maniere les rang MPI des sites iront de 0 a NB_SITE-1 et l'initiateur aura
	//pour rang MPI NB_SITE
	if (rang == NB_SITE)
		simulateur();
	else
		test_initialisation(rang);

	MPI_Finalize();
	return 0;
}
