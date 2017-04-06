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
#define TAGRESP NB_SITE+1 /*Tags negatifs non authorises*/
#define TAGTERM 2 /*Message de terminaison*/
#define TAGQUIT 3
#define TAGQUIT_SPREAD 4
#define DATA_RECHERCHE 9/*Modifier cette variable pour chercher le responsable d'une donnee a partir de son id (9, 36, 60 sont de bonnes valeurs)*/


int id_chord[NB_SITE];
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
	while(i < NB_SITE){
		r = rand() % NB_DATA; //à revoir
		for(j = 0; j < i; j++)
			if(r == id_chord[j])
				exist = 1;

		if(exist){
			exist = 0;
			continue;
		}

		k = 0;
		while((id_chord[k] != 0) && (r > id_chord[k]))
			k+=1;
			
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

void scenario(int rang){
	//Initialisation
	int id_chord;
	int finger_table[I][3];
	int resp;
	int i;

	MPI_Status status;
	MPI_Recv(&id_chord, 1, MPI_INT, NB_SITE, TAGINIT, MPI_COMM_WORLD, &status);
	MPI_Recv(&resp, 1, MPI_INT, NB_SITE, TAGINIT, MPI_COMM_WORLD, &status);
	MPI_Recv(finger_table, I*3, MPI_INT, NB_SITE, TAGINIT, MPI_COMM_WORLD, &status);

	printf("Mon rang : %d, Mon id_chord : %d, Resp: %d\n", rang, id_chord, resp);

	if(rang == 1){
		printf("Finger table du rang 1: \n");
		for(i = 0; i < I; i++){
			printf("i = %d, data mod = %d, resp = %d, rang_resp = %d\n", i, finger_table[i][0], finger_table[i][2], finger_table[i][1]);
		}
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

