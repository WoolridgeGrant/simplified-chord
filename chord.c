#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <time.h>

#define NB_SITE 6
#define NB_DATA 64 /*On suppose que les id des donnees s'etendent de 0 a 64*/
#define TAGINIT 0
#define TAGRESP NB_SITE+1 /*Tags negatifs non authorises*/
#define TAGTERM 2 /*Message de terminaison*/
#define TAGQUIT 3
#define TAGQUIT_SPREAD 4
#define DATA_RECHERCHE 9/*Modifier cette variable pour chercher le responsable d'une donnee a partir de son id (9, 36, 60 sont de bonnes valeurs)*/
#define QUIT_RANK 1

struct successor{
	int id_succ;
	int rang_succ;
};

int id_peers[NB_SITE];
struct successor succ[NB_SITE];
int resp[NB_SITE];

int id_ajout = -1;

//Reste a definir le random pour l'ajout et le resp
void initialisation()
{
	int r = 0, i = 0, j = 0, k = 0;
	int exist = 0;	

	/*
	 * Definition des id des differents 
	 * sites de maniere aleatoire
	 */
	/*TODO: Changer id_peers en id_chord
	 * id_succ en id_chord_succ
	 * rang_succ en rang_mpi_succ
	 */

	while(i < NB_SITE){
		r = rand() % NB_DATA; //à revoir
		for(j = 0; j < i; j++)
			if(r == id_peers[j])
				exist = 1;

		if(exist){
			exist = 0;
			continue;
		}

		k = 0;
		while((id_peers[k] != 0) && (r > id_peers[k]))
			k+=1;
			
		while(j > k){
			id_peers[j] = id_peers[j-1];
			j -= 1;
		}

		id_peers[k] = r;
		i++;
	}

	resp[0] = (id_peers[NB_SITE - 1] + 1) % NB_DATA;
	for(i = 1; i < NB_SITE; i++)
		resp[i] = id_peers[i-1] + 1;

	for(i = 0; i < NB_SITE; i++)
		printf("[  initialisation ]  id_peers[%d] = %d\n", i, id_peers[i]);

	for(i = 0; i < NB_SITE; i++)
		printf("[  initialisation ]  resp[%d] = %d\n", i, resp[i]);

	//Definition du successeur de chacun des sites
	succ[NB_SITE-1].id_succ = id_peers[0];
	succ[NB_SITE-1].rang_succ = 0;
	for(i=0; i < NB_SITE-1; i++){
		succ[i].id_succ = id_peers[i+1];
		succ[i].rang_succ = i+1;
	}
}

void simulateur(void) {
	int i;
	
	srand(time(NULL));
	initialisation();
			       
	for(i=0; i<NB_SITE; i++){
		//i car le dernier processus est l'initiateur
		MPI_Ssend(&id_peers[i], 1, MPI_INT, i, TAGINIT, MPI_COMM_WORLD);  		//son propre id  
		MPI_Ssend(&succ[i].id_succ, 1, MPI_INT, i, succ[i].rang_succ, MPI_COMM_WORLD);	//son successeur (envoi du rang MPI du successeur via le TAG)
		MPI_Ssend(&resp[i], 1, MPI_INT, i, TAGINIT, MPI_COMM_WORLD);			//l'id de la premiere donnee dont il responsable
	}
}

/**
  * is_responsible - Teste si un noeud est responsable d'une donnée ou non
  *
  * @resp: id de la première donnée dont le noeud est responsable
  *
  * @id_chord: id chord du noeud
  * 
  * @id_data_recherche : id de la donnée recherchée
  *
  * @return: retourne 1 si la donnée recherchée est gérée par le noeud appelant is_responsible, sinon retourne 0 
  */

int is_responsible(int resp, int id_chord, int id_data_recherche){
	printf("is_responsible\n");
	if(id_chord < resp)
		return ((id_data_recherche >= resp && id_data_recherche <= NB_DATA) || (id_data_recherche >= 0 && id_data_recherche <= id_chord));
	else
		return (id_data_recherche <= id_chord && id_data_recherche >= resp);
}

void test_initialisation(int rang){
	//Initialisation
	int id_chord;
	int id_chord_succ;
	int rang_mpi_succ;
	int resp;

	//Recherche d'une donnee
	int id_chord_resp; //L'id chord du responsable de la donnee que l'on recherche
	int id_data_recherche; //L'id de la donnee que l'on recherche
	int id_donnee = DATA_RECHERCHE; //L'id de la donnee que l'on recherche
	int initiateur;

	//Suppression d'un noeud
	int tmp_resp;
	int tmp_rang = -1;
	int tmp_tag;

	MPI_Status status;
	MPI_Recv(&id_chord, 1, MPI_INT, NB_SITE, TAGINIT, MPI_COMM_WORLD, &status);
	MPI_Recv(&id_chord_succ, 1, MPI_INT, NB_SITE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	rang_mpi_succ = status.MPI_TAG;
	MPI_Recv(&resp, 1, MPI_INT, NB_SITE, TAGINIT, MPI_COMM_WORLD, &status);

	printf("Mon rang : %d, Mon id_chord : %d, Mon successeur : %d, rang succ: %d\n", rang, id_chord, id_chord_succ, rang_mpi_succ);

	/*Recherche de la donnee ayant pour id DATA_RECHERCHE*/
	if(rang == 1){
		/*Tester localement s'il n'est pas lui meme responsable de la donnee qu'il demande + si la donnee existe*/
		MPI_Ssend(&id_donnee, 1, MPI_INT, rang_mpi_succ, rang, MPI_COMM_WORLD);	
		MPI_Recv(&id_chord_resp, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		if(status.MPI_TAG == TAGRESP)
			printf("Le noeud responsable de la donnee %d est a pour id chord : %d\n", id_donnee, id_chord_resp);
		else //Le message fait un tour complet 
			printf("La donnee demandee n'existe pas\n");
	}
	else{
		MPI_Recv(&id_data_recherche, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		if(status.MPI_TAG == TAGTERM){
			initiateur = id_data_recherche;
			if(rang_mpi_succ != initiateur)
 				MPI_Ssend(&initiateur, 1, MPI_INT, rang_mpi_succ, TAGTERM, MPI_COMM_WORLD);
		}
		else if(is_responsible(resp, id_chord, id_data_recherche)){
			initiateur = status.MPI_TAG;
			//Ce noeud est responsable de la donnee recherchee
			printf("Responsable trouve : %d\n", id_chord);
			MPI_Ssend(&id_chord, 1, MPI_INT, initiateur, TAGRESP, MPI_COMM_WORLD);	
			//Envoi du message de terminaison a tout ceux qui suivent
			if(rang_mpi_succ != initiateur)
				MPI_Ssend(&initiateur, 1, MPI_INT, rang_mpi_succ, TAGTERM, MPI_COMM_WORLD); //Comme on utilise TAGTERM on envoie l'id de l'initiateur directement
		}
		else{
			initiateur = status.MPI_TAG;
			//Il envoie au successeur	
			MPI_Send(&id_data_recherche, 1, MPI_INT, rang_mpi_succ, initiateur, MPI_COMM_WORLD);	
		}
	
	}

	/*
	 * Suppressions de pair x
	 */
	if(rang == QUIT_RANK){
		MPI_Ssend(&resp, 1, MPI_INT, rang_mpi_succ, TAGQUIT, MPI_COMM_WORLD);
		printf("[  test_initialisation ], Quitting node -- resp = %d\n", resp);
		return;
	}
	else{
		MPI_Recv(&tmp_resp, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		tmp_tag = status.MPI_TAG;

		if(tmp_tag == TAGQUIT){
			resp = tmp_resp;

			printf("[  test_initialisation  ]  rang: %d, new_resp: %d\n", rang, resp);
			if(tmp_rang != rang+1)
				MPI_Ssend(&id_chord, 1, MPI_INT, rang_mpi_succ, TAGQUIT_SPREAD, MPI_COMM_WORLD);
		}

		else{
			//printf("[  test_initialisation  ]  rang: %d, tmp_resp: %d\n", rang, tmp_resp);
			if(QUIT_RANK != rang+1)
				MPI_Ssend(&tmp_resp, 1, MPI_INT, rang_mpi_succ, TAGQUIT_SPREAD, MPI_COMM_WORLD);


			else{
				id_chord_succ = tmp_resp;
				rang_mpi_succ = QUIT_RANK + 1;
			}
		}

	}

	printf("Mon rang : %d, Mon id_chord : %d, Mon successeur : %d, rang succ: %d\n", rang, id_chord, id_chord_succ, rang_mpi_succ);
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

