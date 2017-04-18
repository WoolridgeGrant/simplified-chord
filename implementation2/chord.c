#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h> 
#include <time.h>
#include <math.h>

#define NB_SITE 6
#define NB_DATA 64 /*On suppose que les id des donnees s'etendent de 0 a 64, K = 6 pour les finger tables*/
#define K 6
#define I 6
#define TAGINIT 0
#define LOOKUP 1 
#define LASTCHANCE 2 
#define ANSWER 3
#define END 4
#define DATA_RECHERCHE 60/*Modifier cette variable pour chercher le responsable d'une donnee a partir de son id (9, 36, 60 sont de bonnes valeurs)*/


int id_chord[NB_SITE] = {0};
int resp[NB_SITE];
/**
 * 3 colonnes : (id_chord + 2^i)% (2^I), j_mpi, j_chord
 * avec j noeud responsable de (id_chord + 2^i)% (2^I)
 */
int finger_table[I][3];

int whois_responsible(int id_data);
int is_responsible(int resp, int id_chord, int id_data_recherche);

/**
* initialisation - Initialise le système en envoyant à chaque noeud :
*
* - Son id chord
* - La première donnée dont le noeud est responsable
* - La clé chord et le rang MPI de chaque pair figurant dans la finger table du pair
*/

void initialisation()
{
	int r = 0, i = 0, j = 0, k = 0;
	int exist = 0;	

	/*
	 * Definition des id des differents 
	 * sites de maniere aleatoire
	 */
	
	srand(time(NULL));

	/*
	 * à la sorite de cette boucle, on aura:
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

	for(i=0; i < NB_SITE; i++){
		//i car le dernier processus est l'initiateur
		MPI_Send(&id_chord[i], 1, MPI_INT, i, TAGINIT, MPI_COMM_WORLD);  		//son propre id  
		MPI_Send(&resp[i], 1, MPI_INT, i, TAGINIT, MPI_COMM_WORLD);			//l'id de la premiere donnee dont il responsable
	}

	/*Initialisation de la finger table*/
	for(i = 0; i < NB_SITE; i++){
		for(j = 0; j < I; j++){
			finger_table[j][0] = (id_chord[i] + (int)pow(2, j)) % (int)pow(2, K); 
			/*Recherche du responsable de la donnee*/
			finger_table[j][1] = whois_responsible(finger_table[j][0]);
			finger_table[j][2] = id_chord[finger_table[j][1]];
		}
		MPI_Send(finger_table, 3*I, MPI_INT, i, TAGINIT, MPI_COMM_WORLD);
	}
}

/** 
 * whois_reponsible - Recherche du responsable d'une donnée à partir de son id
 *
 *  @id_data: id de la donnée recherchée
 *
 *  @return: Retourne le rang MPI du processus responsable de la donnée passée en paramètre 
 */

int whois_responsible(int id_data){
	int i = 0;
	for(i = 0; i < NB_SITE; i++){
		if(is_responsible(resp[i], id_chord[i], id_data))	
			return i;
	}
	return -1; //Should never happen
}


/**
  * is_responsible - Teste si un noeud est responsable d'une donnée ou non
  * (appartenance à un intervalle cyclique)
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
  * find_finger - Recherche du plus grand pair de la finger table tel que k appartient a ]j,i]
  *
  * @id_data: id de la donnée que l'on recherche
  *
  * @ft: finger table du site appelant find_finger
  *
  * @id_chord: id chord du site appelant find_finger
  *
  * @return: Le rang MPI correspondant au finger trouvé
  */

int find_finger(int id_data, int ft[I][3], int id_chord){
	int i;
	for(i = I-1; i >= 0; i--){
		if(is_responsible(ft[i][2], id_chord, id_data))
			return ft[i][1];
	}
	return -1;
}

/**
  * lookup - Recherche d'une donnée à partir de son id
  *
  * @id_data: id de la donnée que l'on recherche
  *
  * @initiateur: rang mpi de l'initiateur de la recherche
  *
  * @id_chord : id chord du site appelant lookup
  *
  */

void lookup(int id_data, int initiateur, int ft[I][3], int id_chord){
	int finger = find_finger(id_data, ft, id_chord); //Rang mpi du prochain site à qui transmettre la requête	
	int tab[2] = {id_data, initiateur};
	if(finger == -1)
		MPI_Send(tab, 2, MPI_INT, ft[0][1], LASTCHANCE, MPI_COMM_WORLD);
	else
		MPI_Send(tab, 2, MPI_INT, finger, LOOKUP, MPI_COMM_WORLD);
}


void scenario(int rang){
	//Initialisation
	int id_chord;
	int finger_table[I][3];
	int resp;
	int i;

	int reponse;
	int lookup_ans[2];

	int inexistant = -1;
	int whatever = 0;

	MPI_Request request;
	MPI_Status status;
	MPI_Recv(&id_chord, 1, MPI_INT, NB_SITE, TAGINIT, MPI_COMM_WORLD, &status);
	MPI_Recv(&resp, 1, MPI_INT, NB_SITE, TAGINIT, MPI_COMM_WORLD, &status);
	MPI_Recv(finger_table, I*3, MPI_INT, NB_SITE, TAGINIT, MPI_COMM_WORLD, &status);

	printf("Mon rang : %d, Mon id_chord : %d, Resp: %d\n", rang, id_chord, resp);

	//rang 1 initialise la recherche
	if(rang == 1){

		printf("Finger table du rang 1: \n");
		for(i = 0; i < I; i++){
			printf("i = %d, data mod = %d, resp = %d, rang_resp = %d\n", i, finger_table[i][0], finger_table[i][2], finger_table[i][1]);
		}

		lookup(DATA_RECHERCHE, rang, finger_table, id_chord);
		/*On cherche à connaître le type de message que l'on va recevoir*/
		MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

		/* Cas où il est responsable lui même de la donnée qu'il a demandé
		 * Réception d'un tableau id_data, initiateur
		 */
		if(status.MPI_TAG == LASTCHANCE){
			MPI_Recv(lookup_ans, 2, MPI_INT, MPI_ANY_SOURCE, LASTCHANCE, MPI_COMM_WORLD, &status);	
			if(is_responsible(resp, id_chord, DATA_RECHERCHE))
				printf("Le responsable de la donnée est %d est le site ayant pour id chord %d\n", DATA_RECHERCHE, id_chord);
			else
				printf("La donnée recherchée n'existe pas\n");
		}
		/* Cas où le responsable a été trouvé
		 * Réception d'un entier contenant l'id chord du responsable
		 */
		else if(status.MPI_TAG == ANSWER){
			MPI_Recv(&reponse, 1, MPI_INT, MPI_ANY_SOURCE, ANSWER, MPI_COMM_WORLD, &status);
			if(reponse == -1)
				printf("La donnée recherchée n'existe pas\n");
			else
				printf("Le responsable de la donnée %d est le site ayant pour id chord %d\n", DATA_RECHERCHE, reponse);
		}
		else
			fprintf(stderr, "ERREUR : Réception par l'initiateur d'un message inattendu\n");	

		/* Envoie d'un message à tous les processus pour reveiller ceux 
		 * qui attendent encore un message
		 */
		for(i = 0; i < NB_SITE; i++){
			MPI_Isend(&whatever, 1, MPI_INT, i, END, MPI_COMM_WORLD, &request);
		}
	}
	else{
		MPI_Probe(MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		if(status.MPI_TAG == LASTCHANCE){
			MPI_Recv(lookup_ans, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);	
			if(is_responsible(resp, id_chord, DATA_RECHERCHE))
				MPI_Send(&id_chord, 1, MPI_INT, lookup_ans[1], ANSWER, MPI_COMM_WORLD); //Envoi à l'initiateur donnée non trouvée
			else
				MPI_Send(&inexistant, 1, MPI_INT, lookup_ans[1], ANSWER, MPI_COMM_WORLD); //Envoi à l'initiateur donnée non trouvée
		}
		else if(status.MPI_TAG == LOOKUP){
			MPI_Recv(lookup_ans, 2, MPI_INT, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);	
			lookup(lookup_ans[0], lookup_ans[1], finger_table, id_chord);
		}
		else if(status.MPI_TAG == END){
			return;
		}
		else
			fprintf(stderr, "ERREUR : Réception par un noeud d'un message inattendu\n");
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
		initialisation();
	else
		scenario(rang);

	MPI_Finalize();
	return 0;
}

