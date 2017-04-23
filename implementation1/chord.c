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
#define INSERT_NOTIFY 0

void remove_notify(int, int);
void remove_node(int, int, int *, int *, int *);
void insert_notify(int, int, int *, int *);
void insert_node(int, int, int *, int *, int *);

struct successor{
	int id_chord_succ;
	int rang_mpi_succ;
};

int id_chord[NB_SITE] = {0};
struct successor succ[NB_SITE];
int resp[NB_SITE];

int id_ajout = -1;

/**
  * initialisation - Détermine l'id chord, le successeur de chaque site ainsi que 
  * 					l'id de la première donnée dont chaque site est responsable 
  */

void initialisation(){
	int r = 0, i = 0, j = 0, k = 0;
	int exist = 0;	

	/*
	 * Definition des id des differents 
	 * sites de maniere aleatoire
	 */

	/*
	 * à la sortie de cette boucle, on aura:
	 * pour tout n appartenant à [0, NB_SITE-2]
	 * id_chord[n + 1] > id_chord[n]
	 */
	while(i < NB_SITE){
		r = rand() % NB_DATA;

		/* Teste si la clef existe déjà */
		for(j = 0; j < i; j++)
			if(r == id_chord[j])
				exist = 1;

		if(exist){
			exist = 0;
			continue;
		}

		k = 0;
		while(r > id_chord[k]){
			k+=1;

			if(!id_chord[k])
				break;
		}

		while(j > k){
			id_chord[j] = id_chord[j-1];
			j -= 1;
		}

		id_chord[k] = r;
		i++;
	}

	resp[0] = (id_chord[NB_SITE - 1] + 1) % NB_DATA;
	for(i = 1; i < NB_SITE; i++)
		resp[i] = id_chord[i-1] + 1;

	for(i = 0; i < NB_SITE; i++)
		printf("[  initialisation ]  id_chord[%d] = %d\n", i, id_chord[i]);

	for(i = 0; i < NB_SITE; i++)
		printf("[  initialisation ]  resp[%d] = %d\n", i, resp[i]);

	//Definition du successeur de chacun des sites
	succ[NB_SITE-1].id_chord_succ = id_chord[0];
	succ[NB_SITE-1].rang_mpi_succ = 0;
	for(i=0; i < NB_SITE-1; i++){
		succ[i].id_chord_succ = id_chord[i+1];
		succ[i].rang_mpi_succ = i+1;
	}
}

/**
  * simulateur - Envoie l'id chord, le successeur et la première donnée dont un site est reponsable
  * 				à chaque site.
  */

void simulateur(void){
	int i;
	
	srand(time(NULL));
	initialisation();
			       
	for(i=0; i<NB_SITE; i++){
		//i car le dernier processus est l'initiateur
		MPI_Ssend(&id_chord[i], 1, MPI_INT, i, TAGINIT, MPI_COMM_WORLD);  		//son propre id  
		MPI_Ssend(&succ[i].id_chord_succ, 1, MPI_INT, i, succ[i].rang_mpi_succ, MPI_COMM_WORLD);	//son successeur (envoi du rang MPI du successeur via le TAG)
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

/**
  * execution - Execute le scénario demandé dans l'énoncé 
  *
  * @rang: le rang MPI du processus exécutant la fonction
  */

void execution(int rang){
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
	 * Supprimer un noeud 
	 */

	if(rang == QUIT_RANK){
		remove_notify(resp, rang_mpi_succ);
	}
	else{
		remove_node(rang, id_chord, &resp, &rang_mpi_succ, &id_chord_succ);
		printf("[ Step 2 ] rang: %d, id_chord: %d,id_chord_succ: %d, rang_mpi_succ: %d.\n",
							rang, id_chord, id_chord_succ, rang_mpi_succ);
	}

	/*
	 * Insérer un noeud
	 */

	if(rang == INSERT_RANK)
		insert_notify(rang, id_chord, &rang_mpi_succ, &id_chord_succ);
	else
		insert_node(rang, id_chord, &rang_mpi_succ, &id_chord_succ, &resp);

	printf("[ Step 3 ] rang: %d, id_chord: %d,id_chord_succ: %d, rang_mpi_succ: %d.\n",
		rang, id_chord, id_chord_succ, rang_mpi_succ);
}

/**
  * remove_notify - Le noeud quittant l'anneau exécute cette fonction.
  * 				Elle envoie un message de type TAGQUIT à son successeur qui fait
  * 				suivre le message. Une fois que le message est arrivé au prédecesseur
  * 				du noeud qui quitte, ce prédecesseur envoie un message TAGQUIT_OK à
  * 				son successeur pour lui dire qu'il peut quitter. 
  * 				Nous avons fait ce choix d'implémentation pour synchroniser les messages.
  * 				Sans cette méthode nous avions des messages entre l'étape de la suppression
  * 				et l'étape de l'insertion qui se chevauchaient.
  *
  * @resp: La première donnée dont est responsable le noeud quittant l'anneau
  *
  * @rang_mpi_succ: Rang MPI du successeur du noeud qui quitte
  *
  */

void remove_notify(int resp, int rang_mpi_succ){
	int remove_message;
	MPI_Status status;

	MPI_Ssend(&resp, 1, MPI_INT, rang_mpi_succ, TAGQUIT, MPI_COMM_WORLD);
	MPI_Recv(&remove_message, 1, MPI_INT, MPI_ANY_SOURCE,TAGQUIT_OK, MPI_COMM_WORLD, &status);
        printf("[  execution ], Quitting node -- resp = %d\n", resp);
}

/**
  * remove_node - Méthode exécutée par tous les noeuds sauf celui quittant l'anneau.
  * 			  Chaque noeud transmet le message TAGQUIT afin que le prédécesseur
  * 			  et successeur du noeud quittant l'anneau puissent se réorganiser.
  *
  * @rang: rang MPI du noeud exécutant la fonction
  *
  * @id_chord: id_chord du noeud exécutant la fonction
  *
  * @resp: id de la première donnée dont le noeud exécutant la fonction est responsable
  *
  * @rang_mpi_succ: rang MPI du successeur
  *
  * @id_chord_succ: id chord du successeur
  *
  */

void remove_node(int rang, int id_chord, int *resp, int *rang_mpi_succ, int *id_chord_succ){
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

/** 
  * is_predecessor - Renvoie 1 si le pair d'id 'id_chord' est le prédécesseur d'
  *                  	- un pair d'id 'request_data' ou,
  *			- un pair responsable d'une donnée d'id 'request_data'
  * 
  */

int is_predecessor(int request_data, int id_chord, int successor_chord)
{
	if (successor_chord > id_chord)
		return (request_data > id_chord && request_data <= successor_chord);

	return (!(request_data > successor_chord && request_data <= id_chord));
}


/** 
  * insert_notify - Exécutée par le processus qui va se réinsérer dans l'anneau 
  *
  * Voir remove_notify pour les explications sur les arguments
  * 
  */

void insert_notify(int rang, int id_chord, int *rang_mpi_succ, int *id_chord_succ){

	int insert_message[3];
	int data_insert[1][3] = {{rang, id_chord, 0}};
	MPI_Status status;

	MPI_Ssend(&data_insert, 3, MPI_INT, INSERT_NOTIFY, TAGINSERT, MPI_COMM_WORLD);
	MPI_Recv(insert_message, 3, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

	*id_chord_succ = insert_message[0];
	*rang_mpi_succ = insert_message[1];

	if(*rang_mpi_succ != insert_message[2])
		MPI_Ssend(insert_message, 3, MPI_INT, *rang_mpi_succ, TAGINSERT_SPREAD, MPI_COMM_WORLD);
}

/** 
  * insert_node - Exécutée par les processus qui sont déjà dans l'anneau
  *
  * Voir remove_node pour les explications sur les arguments
  * 
  */

void insert_node(int rang, int id_chord, int *rang_mpi_succ, int *id_chord_succ, int *resp){

	int insert_tag;
	int insert_rang;
	int insert_id_chord;
	int insert_message[3];

	int tmp_id_chord_succ;
	int tmp_rang_mpi_succ;
	MPI_Status status;

	MPI_Recv(insert_message, 3, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
	insert_tag = status.MPI_TAG;
	insert_rang = insert_message[0];
	insert_id_chord = insert_message[1];

	/*
	 * Le peer notifié par l'insertion peut etre:
	 * futur successeur, futur predecesseur, ou
	 * ou ni l'un ni l'autre.
	 */

	printf("[  insert_node  ] Insert notification: %d\n", rang);
	if(insert_tag == TAGINSERT)
		insert_message[2] = rang;

	if(is_responsible(*resp, id_chord, insert_id_chord))
		*resp = insert_id_chord + 1;
	else
		if( is_predecessor(id_chord, *id_chord_succ, insert_id_chord)){
			tmp_rang_mpi_succ = insert_rang;
			tmp_id_chord_succ = insert_id_chord;

			insert_message[0] = *id_chord_succ;
			insert_message[1] = *rang_mpi_succ;

			*id_chord_succ = tmp_id_chord_succ;
			*rang_mpi_succ = tmp_rang_mpi_succ;
		}

	if(insert_tag == TAGINSERT || (insert_tag != TAGINSERT  &&  *rang_mpi_succ != insert_message[2]))
		MPI_Ssend(insert_message, 3, MPI_INT, *rang_mpi_succ, TAGINSERT_SPREAD, MPI_COMM_WORLD);
}

int main (int argc, char* argv[]) {
	int nb_proc,rang;
	MPI_Init(&argc, &argv);
	MPI_Comm_size(MPI_COMM_WORLD, &nb_proc);

	if (nb_proc != NB_SITE+1) {
		printf("Nombre de processus incorrect !\n");
		exit(2);
	}

	MPI_Comm_rank(MPI_COMM_WORLD, &rang);

	//L'initiateur va etre le dernier processus pour des raisons pratiques (par rapport a l'indice la boucle
	//De cette maniere les rang MPI des sites iront de 0 a NB_SITE-1 et l'initiateur aura
	//pour rang MPI NB_SITE

	if (rang == NB_SITE)
		simulateur();
	else
		execution(rang);

	MPI_Finalize();
	return 0;
}
