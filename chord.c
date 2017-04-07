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
#define TAGQUIT_OK 4
#define TAGQUIT_SPREAD 5

#define TAGINSERT 6
#define TAGINSERT_OK 7
#define TAGINSERT_SPREAD 8

#define DATA_RECHERCHE 9/*Modifier cette variable pour chercher le responsable d'une donnee a partir de son id (9, 36, 60 sont de bonnes valeurs)*/

/* Rang MPI du pair à supprimer */
#define QUIT_RANK 5
/* Id_chord du pair à insérer */
#define INSERT_RANK 5
/* Id_chord du pair à notifier*/
#define INSERT_NOTIFY 2

void remove_notify(int, int);
void remove_node(int, int, int *, int *, int *);
void insert_notify(int, int);
void insert_node(int);

struct successor{
	int id_succ;
	int rang_succ;
};

int id_peers[NB_SITE] = {0};
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


	/*
	 * à la sorite de cette boucle, on aura:
	 * pour tout n appartenant à [0, NB_SITE-2]
	 * id_peers[n + 1] > id_peers[n]
	 */
	while(i < NB_SITE){
		r = rand() % NB_DATA;

		/* Teste si la clef existe déjà */
		for(j = 0; j < i; j++)
			if(r == id_peers[j])
				exist = 1;

		if(exist){
			exist = 0;
			continue;
		}

		k = 0;
		while(r > id_peers[k]){
			k+=1;

			if(!id_peers[k])
				break;
		}

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
	//int remove_message;
	//int remove_tag;

	MPI_Status status;
	MPI_Recv(&id_chord, 1, MPI_INT, NB_SITE, TAGINIT, MPI_COMM_WORLD, &status);
	MPI_Recv(&id_chord_succ, 1, MPI_INT, NB_SITE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	rang_mpi_succ = status.MPI_TAG;
	MPI_Recv(&resp, 1, MPI_INT, NB_SITE, TAGINIT, MPI_COMM_WORLD, &status);

	printf("Mon rang : %d, Mon id_chord : %d, Mon successeur : %d, rang succ: %d\n", rang, id_chord, id_chord_succ, rang_mpi_succ);

	/*Recherche de la donnee ayant pour id DATA_RECHERCHE*/
	if(rang == 1){
		/*Teste localement s'il est responsable de la donnee qu'il demande*/
		if(is_responsible(resp, id_chord, id_donnee)){
			printf("Responsable localement\n");
			printf("Le noeud responsable de la donnee %d est a pour id chord : %d\n", id_donnee, id_chord);
			MPI_Ssend(&rang, 1, MPI_INT, rang_mpi_succ, TAGTERM, MPI_COMM_WORLD);
		}
		else{
			MPI_Ssend(&id_donnee, 1, MPI_INT, rang_mpi_succ, rang, MPI_COMM_WORLD);	
			MPI_Recv(&id_chord_resp, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			if(status.MPI_TAG == TAGRESP)
				printf("Le noeud responsable de la donnee %d est a pour id chord : %d\n", id_donnee, id_chord_resp);
			else //Le message fait un tour complet 
				printf("La donnee demandee n'existe pas\n");
		}
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
	 * Gestion de la Dynamicité du Système.
	 */


	if(rang == QUIT_RANK){
		remove_notify(resp, rang_mpi_succ);
	}
	else{
		remove_node(rang, id_chord, &resp, &rang_mpi_succ, &id_chord_succ);
		printf("[ Step 2 ] rang: %d, id_chord: %d,id_chord_succ: %d, rang_mpi_succ: %d.\n",
							rang, id_chord, id_chord_succ, rang_mpi_succ);
	}


	if(rang == INSERT_RANK){
		insert_notify(id_chord, rang);
	}
	else{
		insert_node(rang);
	}




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

void remove_notify(int resp, int rang_mpi_succ)
{
	int remove_message;
	MPI_Status status;

	MPI_Ssend(&resp, 1, MPI_INT, rang_mpi_succ, TAGQUIT, MPI_COMM_WORLD);
	MPI_Recv(&remove_message, 1, MPI_INT, MPI_ANY_SOURCE,TAGQUIT_OK, MPI_COMM_WORLD, &status);
        printf("[  test_initialisation ], Quitting node -- resp = %d\n", resp);
}

void remove_node(int rang, int id_chord, int *resp, int *rang_mpi_succ, int *id_chord_succ)
{
	int remove_tag;
	int remove_message;
	MPI_Status status;


	MPI_Recv(&remove_message, 1, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	remove_tag = status.MPI_TAG;

	/*
	** Le successeur du pair à supprimer envoie:
	** remove_message = <id_chord, TAGQUIT_SPREAD>
	**/
	if(remove_tag == TAGQUIT){
		*resp = remove_message;

		//printf("[  test_initialisation  ]  rang: %d, new_resp: %d\n", rang, resp);
		if(QUIT_RANK != (rang+1) % NB_SITE)
			MPI_Ssend(&id_chord, 1, MPI_INT, *rang_mpi_succ, TAGQUIT_SPREAD, MPI_COMM_WORLD);
	}

	else{
		if(QUIT_RANK != (rang+1) % NB_SITE)
		MPI_Ssend(&remove_message, 1, MPI_INT, *rang_mpi_succ, TAGQUIT_SPREAD, MPI_COMM_WORLD);

		else{
			/*
			** Le prédécesseur du pair à supprimer.
			*/
			MPI_Ssend(&remove_message, 1, MPI_INT, *rang_mpi_succ, TAGQUIT_OK, MPI_COMM_WORLD);
			*rang_mpi_succ = (QUIT_RANK + 1) % NB_SITE;
			*id_chord_succ = remove_message;
		}
	}
}






void insert_notify(int id_chord, int rang){

	int data_insert[1][2] = {{id_chord, rang}};
	MPI_Ssend(&data_insert, 2, MPI_INT, INSERT_NOTIFY, TAGINSERT, MPI_COMM_WORLD);
}



void insert_node(int rang){

	int insert_message[2];
	MPI_Status status;

	if(rang == INSERT_NOTIFY)
		MPI_Recv(insert_message, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	if(rang == INSERT_NOTIFY)
		printf("[  insert_node  ] rank: %d\n", rang);
	return;
}
