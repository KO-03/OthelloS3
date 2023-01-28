#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>

#include <signal.h>
#include <sys/signalfd.h>

#include <gtk/gtk.h>

#include <stdbool.h> //librairie boolean

#define MAXDATASIZE 256

#define TYPE_COULEUR 0
#define TYPE_COORD 1
#define TYPE_IMPOSSIBILITE_COUP 2
#define TYPE_FIN 3


struct coup {
	uint16_t type;				//0 pour la couleur, 1 pour les coordonnees x, y
	uint16_t x;
	uint16_t y;
	uint16_t couleur;
};


/* Variables globales */
int damier[8][8]; //car Plateau de dimension 8*8
int couleur;

int port;

char *addr_j2, *port_j2;

pthread_t thr_id;

int sockfd, newsockfd = -1;
int addr_size;
struct sockaddr *their_addr;

fd_set master, read_fds, write_fds;	
int fdmax;						

int status;
struct addrinfo hints, *servinfo, *p;

/* Variables globales associees a l'interface graphique */
GtkBuilder *p_builder = NULL;
GError *p_err = NULL;

/* -------------------------------------------------- DEBUT DES ENTETES DES FONCTIONS -------------------------------------------------- */  
  
/* Fonction permettant de changer l'image d'une case du damier (indique par sa colonne et sa ligne) */
void change_img_case(int col, int lig, int couleur_j);

/* Fonction permettant changer nom joueur blanc dans cadre Score */
void set_label_J1(char *texte);

/* Fonction permettant de changer nom joueur noir dans cadre Score */
void set_label_J2(char *texte);

/* Fonction permettant de changer score joueur blanc dans cadre Score */
void set_score_J1(int score);

/* Fonction permettant de recuperer score joueur blanc dans cadre Score */
int get_score_J1(void);

/* Fonction permettant de changer score joueur noir dans cadre Score */
void set_score_J2(int score);

/* Fonction permettant de recuperer score joueur noir dans cadre Score */
int get_score_J2(void);

/* Fonction transformant coordonnees du damier graphique en indexes pour matrice du damier */
void coord_to_indexes(const gchar *coord, int *col, int *lig);

/* Fonction appelee lors du clique sur une case du damier */
static void coup_joueur(GtkWidget *p_case);

/* Fonction retournant texte du champs adresse du serveur de l'interface graphique */
char *lecture_addr_serveur(void);

/* Fonction retournant texte du champs port du serveur de l'interface graphique */
char *lecture_port_serveur(void);

/* Fonction retournant texte du champs login de l'interface graphique */
char *lecture_login(void);

/* Fonction retournant texte du champs adresse du cadre Joueurs de l'interface graphique */
char *lecture_addr_adversaire(void);

/* Fonction retournant texte du champs port du cadre Joueurs de l'interface graphique */
char *lecture_port_adversaire(void);

/* Fonction affichant boite de dialogue si partie gagnee */
void affiche_fenetre_gagne(void);

/* Fonction affichant boite de dialogue si partie perdue */
void affiche_fenetre_perdu(void);

/* Fonction appelee lors du clique du bouton Se connecter */
static void clique_connect_serveur(GtkWidget *b);

/* Fonction desactivant bouton demarrer partie */
void disable_button_start(void);

/* Fonction appelee lors du clique du bouton Demarrer partie */
static void clique_connect_adversaire(GtkWidget *b);

/* Fonction desactivant les cases du damier */
void gele_damier(void);

/* Fonction activant les cases du damier */
void degele_damier(void);

/* Fonction permettant d'initialiser le plateau de jeu */
void init_interface_jeu(void);

/* Fonction reinitialisant la liste des joueurs sur l'interface graphique */
void reset_liste_joueurs(void);

/* Fonction permettant d'ajouter un joueur dans la liste des joueurs sur l'interface graphique */
void affich_joueur(char *login, char *adresse, char *port);

/* Fonction executee par le thread gerant les communications a travers la socket */
static void *f_com_socket(void *p_arg);

/* -------------------------------------------------- ENTETES DES FONCTIONS SUPPLEMENTAIRES -------------------------------------------------- */

/* Fonction verifiant si une case est jouable en verifiant qu'on ne depasse pas la taille du damier et que l'on joue en fonction de son voisin */
bool coup_valide(int col, int lig, int couleur_joueur);

/* Fonction permettant de changer la couleur de la case */
int set_case(int colonne, int ligne, int couleur);

/* Fonction qui retourne si on doit changer la ligne */
bool change_ligne(int colonne, int ligne, int chmtx, int chmty, int couleur);

/* Fonction changeant la couleur des cases, voir changement_case(int, int, int, int, int) */
void changements_case(int colonne, int ligne, int couleur);

/* Fonction changeant la couleur des cases dans la direction choisie */
void changement_case(int colonne, int ligne, int chmtx, int chmty, int couleur);

/* Fonction permettant de savoir si on peut encore placer des pieces */
bool fin_partie();

/* Fonction permettant de savoir quand aucun joueur ne peut jouer */
bool fin_partie_prematuree();

/* Fonction permettant de savoir quand il n'y a pas de coup possible */
bool coup_impossible();

/* Fonction permettant de retourner la couleur de la case */
int return_couleur_case(int colonne, int ligne);

/* Fonction retournant la couleur du joueur courant */
int get_couleur();

/* Fonction initialisant la couleur du joueur courant */
void set_couleur(int couleur_joueur);

/* Fonction retournant la couleur de l'adversaire */
int get_couleur_adversaire();

/* Fonction retournant la couleur adverse */
int get_couleur_adverse(int couleur);

/* Fonction permettant de poser un jeton */
void poser_jeton(int col, int lig, int couleur_j);

/* Fonction indiquant les cases jouables */
void indic_jouable(int couleur_j);

/* Fonction pour remettre les cases par défaut pendant le gele du damier*/
void reset_case_jouable();

/* -------------------------------------------------- FIN DES ENTETES DES FONCTIONS -------------------------------------------------- */

/* ------------------------------------------------------- DEBUT DES FONCTIONS ------------------------------------------------------- */

/* Fonction transformant les coordonnees du damier graphique en indexes pour matrice du damier */
void coord_to_indexes(const gchar * coord, int *col, int *lig) {
	char *c;
	c = malloc(3 * sizeof(char));

	c = strncpy(c, coord, 1);
	c[1] = '\0';

	if (strcmp(c, "A") == 0) {
		*col = 0; }
	else if (strcmp(c, "B") == 0) {
		*col = 1; }
	else if (strcmp(c, "C") == 0) {
		*col = 2; }
	else if (strcmp(c, "D") == 0) {
		*col = 3; }
	else if (strcmp(c, "E") == 0) {
		*col = 4; }
	else if (strcmp(c, "F") == 0) {
		*col = 5; }
	else if (strcmp(c, "G") == 0) {
		*col = 6; }
	else if (strcmp(c, "H") == 0) {
		*col = 7; }

	*lig = atoi(coord + 1) - 1;
}

/* Fonction transformant les indexes pour matrice du damier en coordonnees du damier graphique */
void indexes_to_coord(int col, int lig, char *coord) {
	char c;

	if (col == 0) {
		c = 'A'; }
	else if (col == 1) {
		c = 'B'; }
	else if (col == 2) {
		c = 'C'; }
	else if (col == 3) {
		c = 'D'; }
	else if (col == 4) {
		c = 'E'; }
	else if (col == 5) {
		c = 'F'; }
	else if (col == 6) {
		c = 'G'; }
	else if (col == 7) {
		c = 'H'; }

	sprintf(coord, "%c%d\0", c, lig + 1);
}

/* Fonction permettant de changer l'image d'une case du damier (indique par sa colonne et sa ligne) */
void change_img_case(int col, int lig, int couleur_j) {
	char *coord;

	coord = malloc(3 * sizeof(char));

	indexes_to_coord(col, lig, coord);

	if (couleur_j == 1)
	{
		// image pion blanc
		gtk_image_set_from_file(GTK_IMAGE
								(gtk_builder_get_object(p_builder, coord)),
								"UI_Glade/case_blanc.png");
	}
	else
	{
		// image pion noir
		gtk_image_set_from_file(GTK_IMAGE
								(gtk_builder_get_object(p_builder, coord)),
								"UI_Glade/case_noir.png");
	}
}

/* Fonction permettant changer nom joueur blanc dans cadre Score */
void set_label_J1(char *texte) {
	gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(p_builder, "label_J1")), texte);
}

/* Fonction permettant changer nom joueur noir dans cadre Score */
void set_label_J2(char *texte) {
	gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(p_builder, "label_J2")), texte);
}

/* Fonction permettant de changer score joueur blanc dans cadre Score */
void set_score_J1(int score) {
	char *s;

	s = malloc(5 * sizeof(char));
	sprintf(s, "%d\0", score);

	gtk_label_set_text(GTK_LABEL(gtk_builder_get_object(p_builder, "label_ScoreJ1")), s);
}

/* Fonction permettant de recuperer score joueur blanc dans cadre Score */
int get_score_J1(void) {
	const gchar *c;

	c = gtk_label_get_text(GTK_LABEL
						   (gtk_builder_get_object
							(p_builder, "label_ScoreJ1")));

	return atoi(c);
}

/* Fonction permettant de changer score joueur noir dans cadre Score */
void set_score_J2(int score) {
	char *s;

	s = malloc(5 * sizeof(char));
	sprintf(s, "%d\0", score);

	gtk_label_set_text(GTK_LABEL
					   (gtk_builder_get_object(p_builder, "label_ScoreJ2")), s);
}

/* Fonction permettant de recuperer score joueur noir dans cadre Score */
int get_score_J2(void) {
	const gchar *c;

	c = gtk_label_get_text(GTK_LABEL
						   (gtk_builder_get_object
							(p_builder, "label_ScoreJ2")));

	return atoi(c);
}

/* Fonction appelee lors du clic sur une case du damier */
static void coup_joueur(GtkWidget * p_case) {
	int col, lig, type_msg, nb_piece, score;
	char buf[MAXDATASIZE];

	//ajout initiasation requete
	struct coup req;

	// Traduction coordonnees damier en indexes matrice damier
	coord_to_indexes(gtk_buildable_get_name(GTK_BUILDABLE(gtk_bin_get_child(GTK_BIN(p_case)))), &col, &lig);

	printf("click joueur : col:%d / lig:%d\n", col, lig);
  	printf("Couleur coup joueur : %u \n", couleur);

	/* Cas où le plateau est rempli
	*  la partie se termine
	*/
	if (fin_partie())
	{
		reset_case_jouable();
		gele_damier();

		//préparation message et envoie sur socket à adversaire
		req.type = htons((uint16_t) TYPE_FIN);

		printf("Requête à envoyer %u\n", req.type);
		bzero(buf, MAXDATASIZE);
		snprintf(buf, MAXDATASIZE, "%u", req.type);

		printf("Envoyer requête %s\n", buf);
		if (send(newsockfd, &buf, strlen(buf), 0) == -1)
		{
			perror("send");
			close(newsockfd);
		}

		affiche_fenetre();
	}

	/* Cas où aucun des joueurs peut jouer
	*  la partie se termine,
	*  les places vident vont au gagnant
	*/
	else if (fin_partie_prematuree())
	{
		int place_vide =
			(8 * 8) - (get_score_J1() + get_score_J2());
		if (get_score_J1() > get_score_J2())
		{
			set_score_J1(get_score_J1() + place_vide);
		}
		else
		{
			set_score_J2(get_score_J2() + place_vide);
		}

		//préparation message et envoie sur socket à adversaire
		req.type = htons((uint16_t) TYPE_FIN);

		printf("Requête à envoyer %u\n", req.type);
		bzero(buf, MAXDATASIZE);
		snprintf(buf, MAXDATASIZE, "%u", req.type);

		printf("Envoyer requête %s\n", buf);
		if (send(newsockfd, &buf, strlen(buf), 0) == -1)
		{
			perror("send");
			close(newsockfd);
		}


		affiche_fenetre();
	}

	/* Cas où un joueur ne peut pas jouer */
	else if (coup_impossible())
	{
		reset_case_jouable();
		gele_damier();

		//préparation message et envoie sur socket à adversaire
		req.type = htons((uint16_t) TYPE_IMPOSSIBILITE_COUP);

		printf("Requête à envoyer %u\n", req.type);
		bzero(buf, MAXDATASIZE);
		snprintf(buf, MAXDATASIZE, "%u", req.type);

		printf("Envoyer requête %s\n", buf);
		if (send(newsockfd, &buf, strlen(buf), 0) == -1)
		{
			perror("send");
			close(newsockfd);
		}

		affiche_fenetre_coup_impossible();
	}

	/* Cas normal de jeu */
	else
	{
		if (coup_valide(col, lig, get_couleur()))
		{
			//change la case
			set_case(col, lig, get_couleur());
			reset_case_jouable();
			gele_damier();
			//déterminer pions dont la couleur change + score
			changements_case(col, lig, get_couleur());

			printf("Info à envoyer : %u %u\n", col, lig);

			//préparation message et envoie sur socket à adversaire
			req.type = htons((uint16_t) TYPE_COORD);
			req.x = htons((uint16_t) col);
			req.y = htons((uint16_t) lig);
			printf("Requête à envoyer %u : %u %u\n", req.type, req.x, req.y);
			bzero(buf, MAXDATASIZE);
			snprintf(buf, MAXDATASIZE, "%u, %u, %u", req.type, req.x, req.y);

			printf("Envoyer requête %s\n", buf);
			if (send(newsockfd, &buf, strlen(buf), 0) == -1)
			{
				perror("send");
				close(newsockfd);
			}
		}
	}
}


/* Fonction retournant texte du champs adresse du serveur de l'interface graphique */
char *lecture_addr_serveur(void) {
	GtkWidget *entry_addr_srv;

	entry_addr_srv = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_adr");

	return (char *) gtk_entry_get_text(GTK_ENTRY(entry_addr_srv));
}

/* Fonction retournant le texte du champs port du serveur de l'interface graphique */
char *lecture_port_serveur(void) {
	GtkWidget *entry_port_srv;

	entry_port_srv = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_port");

	return (char *) gtk_entry_get_text(GTK_ENTRY(entry_port_srv));
}

/* Fonction retournant le texte du champs login de l'interface graphique */
char *lecture_login(void) {
	GtkWidget *entry_login;

	entry_login = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_login");

	return (char *) gtk_entry_get_text(GTK_ENTRY(entry_login));
}

/* Fonction retournant le texte du champs adresse du cadre Joueurs de l'interface graphique */
char *lecture_addr_adversaire(void) {
	GtkWidget *entry_addr_j2;

	entry_addr_j2 = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_addr_j2");

	return (char *) gtk_entry_get_text(GTK_ENTRY(entry_addr_j2));
}

/* Fonction retournant le texte du champs port du cadre Joueurs de l'interface graphique */
char *lecture_port_adversaire(void) {
	GtkWidget *entry_port_j2;

	entry_port_j2 = (GtkWidget *) gtk_builder_get_object(p_builder, "entry_port_j2");

	return (char *) gtk_entry_get_text(GTK_ENTRY(entry_port_j2));
}

/* Fonction permettant d'afficher une des boites de dialogue */
void affiche_fenetre(void) {
	if (get_couleur() == 1)
	{
		if (get_score_J1() > get_score_J2())
		{
			affiche_fenetre_gagne();
		}
		else
		{
			affiche_fenetre_perdu();
		}
	}
	else if (get_couleur() == 0)
	{
		if (get_score_J2() > get_score_J1())
		{
			affiche_fenetre_gagne();
		}
		else
		{
			affiche_fenetre_perdu();
		}
	}
}

/* Fonction affichant boite de dialogue si partie gagnée */
void affiche_fenetre_gagne(void) {
	GtkWidget *dialog;

	GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

	dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "Fin de la partie.\n\n Vous avez gagné!!!", NULL);
	gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);
}

/* Fonction affichant boite de dialogue si partie perdue */
void affiche_fenetre_perdu(void) {
	GtkWidget *dialog;

	GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

	dialog = gtk_message_dialog_new(GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "Fin de la partie.\n\n Vous avez perdu!", NULL);
	gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);
}

/* Fonction affichant boite de dialogue si joueur ne peut pas jouer */
void affiche_fenetre_coup_impossible(void) {
	GtkWidget *dialog;

	GtkDialogFlags flags = GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT;

	dialog =
		gtk_message_dialog_new(GTK_WINDOW(gtk_builder_get_object(p_builder, "window1")), flags, GTK_MESSAGE_INFO, GTK_BUTTONS_CLOSE, "Impossible de jouer.\n\n Dommage !", NULL);
	gtk_dialog_run(GTK_DIALOG(dialog));

	gtk_widget_destroy(dialog);
}

/* Fonction appelee lors du clic du bouton Se connecter */
static void clique_connect_serveur(GtkWidget * b) {
	/*
	 * PAS A FAIRE POUR NOUS
	 */

}

/* Fonction desactivant bouton demarrer partie */
void disable_button_start(void) {
	gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "button_start"), FALSE);
}

/* Fonction appelee lors du clic du bouton Demarrer partie */
static void clique_connect_adversaire(GtkWidget * b) {
	if (newsockfd == -1)
	{
		// Desactivation bouton demarrer partie
		gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "button_start"), FALSE);

		// Recuperation  adresse et port adversaire au format chaines caracteres
		addr_j2 = lecture_addr_adversaire();
		port_j2 = lecture_port_adversaire();

		printf("[Port joueur : %d] Adresse j2 lue : %s\n", port, addr_j2);
		printf("[Port joueur : %d] Port j2 lu : %s\n", port, port_j2);

		pthread_kill(thr_id, SIGUSR1);
	}
}

/* Fonction simplifiee desactivant les cases du damier */
void gele_damier(void)  {
    char name[20];
    for (int i = 1; i <= 8; i++) {
        for (char j = 'A'; j <= 'H'; j++) {
            sprintf(name, "eventbox%c%d", j, i);
            gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, name), FALSE);
        }
    }
}

/* Fonction simplifiee activant les cases du damier */
void degele_damier(void) {
    char name[20];
    for (int i = 1; i <= 8; i++) {
        for (char j = 'A'; j <= 'H'; j++) {
            sprintf(name, "eventbox%c%d", j, i);
            gtk_widget_set_sensitive((GtkWidget *)gtk_builder_get_object(p_builder, name), TRUE);
        }
    }
}

/* Fonction permettant d'initialiser le plateau de jeu */
void init_interface_jeu(void) {
	/* Initilisation du damier (D4=blanc, E4=noir, D5=noir, E5=blanc) */
	set_case(3, 3, 1);
	set_case(4, 3, 0);
	set_case(3, 4, 0);
	set_case(4, 4, 1);

	/* Initialisation des scores et des joueurs */
	if (get_couleur() == 1)
	{
		set_label_J1("Vous");
		set_label_J2("Adversaire");
	}
	else
	{							
		set_label_J1("Adversaire");
		set_label_J2("Vous");
	}

	set_score_J1(2);
	set_score_J2(2);
}

/* Fonction reinitialisant la liste des joueurs sur l'interface graphique */
void reset_liste_joueurs(void) {
	GtkTextIter start, end;

	gtk_text_buffer_get_start_iter(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), &start);
	gtk_text_buffer_get_end_iter(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), &end);
	gtk_text_buffer_delete(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_builder_get_object(p_builder, "textview_joueurs")))), &start, &end);
}

/* Fonction permettant d'ajouter un joueur dans la liste des joueurs sur l'interface graphique */
void affich_joueur(char *login, char *adresse, char *port) {
	const gchar *joueur;

	joueur = g_strconcat(login, " - ", adresse, " : ", port, "\n", NULL);

	gtk_text_buffer_insert_at_cursor(GTK_TEXT_BUFFER(gtk_text_view_get_buffer(GTK_TEXT_VIEW(gtk_builder_get_object (p_builder, "textview_joueurs")))), joueur, strlen(joueur));
}

/* Fonction exécutée par le thread gérant les communications à travers la socket */
static void *f_com_socket(void *p_arg) {

	int i, nbytes, col, lig;

	char buf[MAXDATASIZE], *tmp, *p_parse;
	int len, bytes_sent, t_msg_recu;

	sigset_t signal_mask;
	int fd_signal, rv;

	uint16_t type_msg, col_j2;
	uint16_t ucol, ulig;

	char *token;
	char *saveptr;

	//ajout initiasation requete
	struct coup req;

	/* Association descripteur au signal SIGUSR1 */
	sigemptyset(&signal_mask);
	sigaddset(&signal_mask, SIGUSR1);

	if (sigprocmask(SIG_BLOCK, &signal_mask, NULL) == -1)
	{
		printf("[Port joueur %d] Erreur sigprocmask\n", port);
		return 0;
	}

	fd_signal = signalfd(-1, &signal_mask, 0);

	if (fd_signal == -1)
	{
		printf("[Port joueur %d] Erreur signalfd\n", port);
		return 0;
	}

	/* Ajout descripteur du signal dans ensemble de descripteur utilisé avec fonction select */
	FD_SET(fd_signal, &master);

	if (fd_signal > fdmax)
	{
		fdmax = fd_signal;
	}


	while (1)
	{
		read_fds = master;		// copie des ensembles

		if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1)
		{
			perror("Problème avec select");
			exit(4);
		}

		printf("[Port joueur %d] Entrée dans boucle for\n", port);
		for (i = 0; i <= fdmax; ++i)
		{
		   	/*printf("[Port joueur %d] newsockfd=%d, iteration %d boucle for\n", port, newsockfd, i); */

			if (FD_ISSET(i, &read_fds))
			{
				if (i == fd_signal)
				{
					printf("i = fd_signal\n");
					/* Cas où de l'envoie du signal par l'interface graphique pour connexion au joueur adverse */
					if (newsockfd == -1)
					{
						memset(&hints, 0, sizeof(hints));	
						hints.ai_family = AF_UNSPEC;
						hints.ai_socktype = SOCK_STREAM;

						rv = getaddrinfo(addr_j2, port_j2, &hints, &servinfo);
						if (rv != 0)
						{
							fprintf(stderr, "getaddrinfo:%s\n",
									gai_strerror(rv));
						}
						//création socket et attachement
						for (p = servinfo; p != NULL; p = p->ai_next)
						{
							if ((newsockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)	//sockfd est la socket du client
							{
								perror("client: socket");
								continue;
							}
							if (connect(newsockfd, p->ai_addr, p->ai_addrlen) == -1)	//attachement de la socket - client
							{
								close(newsockfd);
								//FD_CLR(newsockfd, &master);
								perror("client: connect");
								continue;
							}
							break;
						}
						if (p == NULL)
						{
							fprintf(stderr, "server : failed to bind\n");
							return 2;
						}

						freeaddrinfo(servinfo);	//libère la structure

						/* Ajout descripteur du signal dans ensemble de descripteur utilisé avec fonction select */
						FD_SET(newsockfd, &master);

						if (newsockfd > fdmax)
						{
							fdmax = newsockfd;
						}

						close(sockfd);
						FD_CLR(sockfd, &master);

						//fermeture et suppression de fd_signal de l'ensemble master
						close(fd_signal);
						FD_CLR(fd_signal, &master);

						//Choix de couleur à compléter

						set_couleur(0);
						req.type = htons((uint16_t) TYPE_COULEUR);
						req.couleur =
							htons((uint16_t) get_couleur_adversaire());

						bzero(buf, MAXDATASIZE);

						printf("Requête à envoyer %u : %u\n", req.type,
							   req.couleur);
						snprintf(buf, MAXDATASIZE, "%u, %u", req.type,
								 req.couleur);

						printf("Envoyer requête %s\n", buf);
						if (send(newsockfd, &buf, strlen(buf), 0) == -1)
						{
							perror("send");
							close(newsockfd);
						}

						//initialisation interface graphique
						init_interface_jeu();
						indic_jouable(get_couleur());
					}
				}

				if (i == sockfd)
				{				//acceptation connexion adversaire

					printf("i == sockfd\n");

					if (newsockfd == -1)
					{
						addr_size = sizeof(their_addr);
						/*
						 * Accepte une connexion client
						 * et retourne nouveau descripteur de socket de service
						 * pour traiter la requête du client
						 * Fonction bloquante
						 */
						newsockfd =
							accept(sockfd, their_addr,
								   (socklen_t *) & addr_size);


						if (newsockfd == -1)
						{
							perror("problème avec accept");
						}
						else
						{
							/*
							 * Ajout du nouveau descripteur dans ensemble
							 * de descripteur utilisé avec fonction select
							 */
							FD_SET(newsockfd, &master);

							if (newsockfd > fdmax)
							{
								fdmax = newsockfd;
							}

							//


							//fermeture socket écoute car connexion déjà établie avec adversaire
							printf
								("[Port joueur : %d] Bloc if du serveur, fermeture socket écoute\n",
								 port);
							FD_CLR(sockfd, &master);
							close(sockfd);
						}

						//fermeture et suppression de FD_SIGNAL de l'ensemble master
						close(fd_signal);
						FD_CLR(fd_signal, &master);

						// Réception et traitement des messages du joueur adverse
						bzero(buf, MAXDATASIZE);
						recv(newsockfd, buf, MAXDATASIZE, 0);

						token = strtok_r(buf, ",", &saveptr);
						sscanf(token, "%u", &(req.type));
						req.type = ntohs(req.type);
						printf("type message %u\n", req.type);

						if (req.type == 0)
						{
							token = strtok_r(NULL, ",", &saveptr);
							sscanf(token, "%u", &(req.couleur));
							req.couleur = ntohs(req.couleur);
							printf("couleur %u\n", req.couleur);
							set_couleur(req.couleur);
						}

						init_interface_jeu();
						indic_jouable(get_couleur());

						gtk_widget_set_sensitive((GtkWidget *) gtk_builder_get_object(p_builder, "button_start"), FALSE);
					}
				}
				else
				{
					printf("else\n");

					if (i == newsockfd)
					{
						printf("else suite\n");
						// Réception et traitement des messages du joueur adverse
						bzero(buf, MAXDATASIZE);
						nbytes = recv(newsockfd, buf, MAXDATASIZE, 0);

						printf("reception de %d octets reçus\n", nbytes);

						token = strtok_r(buf, ",", &saveptr);
						sscanf(token, "%u", &(req.type));
						req.type = ntohs(req.type);
						printf("type message %u\n", req.type);

						if (req.type == TYPE_COORD)
						{
							token = strtok_r(NULL, ",", &saveptr);
							sscanf(token, "%u", &(req.x));
							req.x = ntohs(req.x);
							printf("x %u\n", req.x);

							token = strtok_r(NULL, ",", &saveptr);
							sscanf(token, "%u", &(req.y));
							req.y = ntohs(req.y);
							printf("y %u\n", req.y);
							set_case(req.x, req.y, get_couleur_adversaire());
							changements_case(req.x, req.y,
											 get_couleur_adversaire());
							degele_damier();
							indic_jouable(get_couleur());
						}
						else if (req.type == TYPE_IMPOSSIBILITE_COUP)
						{
							printf("adversaire n'a pas plus jouer\n");
							degele_damier();
							indic_jouable(get_couleur());
						}
						else if (req.type == TYPE_FIN)
						{
							printf("fin du jeu\n");
							degele_damier();
						}
					}
				}
			}
		}
	}

	return (NULL);
}

/* ----------------------------------- FONCTIONS SUPPLEMENTAIRES ----------------------------------- */

/* Fonction verifiant si une case est jouable */
bool coup_valide(int col, int lig, int couleur_joueur) { 
	return (return_couleur_case(col, lig) == -1
			&& (change_ligne(col, lig, -1, -1, couleur_joueur)
				|| change_ligne(col, lig, 0, -1, couleur_joueur)
				|| change_ligne(col, lig, 1, -1, couleur_joueur)
				|| change_ligne(col, lig, -1, 0, couleur_joueur)
				|| change_ligne(col, lig, 1, 0, couleur_joueur)
				|| change_ligne(col, lig, -1, 1, couleur_joueur)
				|| change_ligne(col, lig, 0, 1, couleur_joueur)
				|| change_ligne(col, lig, 1, 1, couleur_joueur)));
}

/* Fonction permettant de changer la couleur de la case */
int set_case(int colonne, int ligne, int couleur_joueur) {
	//on test la couleur de la case
	if (get_couleur_adverse(couleur_joueur) ==
		return_couleur_case(colonne, ligne))
	{
		if (couleur_joueur == 1)
		{
			set_score_J1(get_score_J1() + 1);
			set_score_J2(get_score_J2() - 1);
		}
		else if (couleur_joueur == 0)
		{
			set_score_J1(get_score_J1() - 1);
			set_score_J2(get_score_J2() + 1);
		}

	}
	else
	{
		if (couleur_joueur == 1)
		{
			set_score_J1(get_score_J1() + 1);
		}
		else if (couleur_joueur == 0)
		{
			set_score_J2(get_score_J2() + 1);
		}
	}
	poser_jeton(colonne, ligne, couleur_joueur);
}

/* Fonction verifiant si on doit changer la ligne */
bool change_ligne(int colonne, int ligne, int chmtx, int chmty, int couleur) {
	int compteur = 0;
	colonne += chmtx;
	ligne += chmty;
	/* Tant que la couleur de la selection de direction est différente */
	while (return_couleur_case(colonne, ligne) == get_couleur_adverse(couleur))
	{
		compteur++;
		colonne += chmtx;
		ligne += chmty;
	}
	/* Sortie de boucle, la couleur est la même 
	* que la couleur du joueur qui vient de jouer.
	* Si le compteur est supéreur à 1 alors */
	return ((return_couleur_case(colonne, ligne) == couleur) && (compteur > 0));
}

/* Fonction changeant la couleur des cases */
void changements_case(int colonne, int ligne, int couleur) {
	if (change_ligne(colonne, ligne, -1, -1, couleur)) {
		changement_case(colonne, ligne, -1, -1, couleur);
	}
	if (change_ligne(colonne, ligne, 0, -1, couleur)) {
		changement_case(colonne, ligne, 0, -1, couleur);
	}
	if (change_ligne(colonne, ligne, 1, -1, couleur)) {
		changement_case(colonne, ligne, 1, -1, couleur);
	}
	if (change_ligne(colonne, ligne, -1, 0, couleur)) {
		changement_case(colonne, ligne, -1, 0, couleur);
	}
	if (change_ligne(colonne, ligne, 1, 0, couleur)) {
		changement_case(colonne, ligne, 1, 0, couleur);
	}
	if (change_ligne(colonne, ligne, -1, 1, couleur)) {
		changement_case(colonne, ligne, -1, 1, couleur);
	}
	if (change_ligne(colonne, ligne, 0, 1, couleur)) {
		changement_case(colonne, ligne, 0, 1, couleur);
	}
	if (change_ligne(colonne, ligne, 1, 1, couleur)) {
		changement_case(colonne, ligne, 1, 1, couleur);
	}
}

/* Fonction changeant la couleur des cases dans la bonne direction */
void changement_case(int colonne, int ligne, int chmtx, int chmty, int couleur) {

	colonne += chmtx;
	ligne += chmty;
	while (return_couleur_case(colonne, ligne) == get_couleur_adverse(couleur)) {
		set_case(colonne, ligne, couleur);
		colonne += chmtx;
		ligne += chmty;
	}
}

/* Fonction verifiant si le plateau est plein */
bool fin_partie() {
	int j1, j2, total;
	j1 = get_score_J1();
	j2 = get_score_J2();
	// la limite est le nb de points total pour savoir si le jeu est fini
	total = 8 * 8;
	return ((j1 + j2) == total);
}

/* Fonction permettant de savoir si aucun joueur ne peut jouer */
bool fin_partie_prematuree() {
	int i, j;
	for (i = 0; i < 8; ++i)
	{
		for (j = 0; j < 8; ++j)
		{
			if ((return_couleur_case(i, j) == -1)
				&& (coup_valide(i, j, 1) || coup_valide(i, j, 0)))
				return false;
		}
	}
	return true;
}

/* Fonction permettant de savoir quand le joueur ne peut pas jouer */
bool coup_impossible() {
	int i, j;
	for (i = 0; i < 8; ++i)
	{
		for (j = 0; j < 8; ++j)
		{
			if ((return_couleur_case(i, j) == -1)
				&& (coup_valide(i, j, get_couleur())))
				return false;
		}
	}
	return true;
}

/* Fonction donnant la couleur de la case */
int return_couleur_case(int colonne, int ligne) {
	if ((colonne < 0) || (ligne < 0) || (colonne >= 8)
		|| (ligne >= 8))
	{
		return -2;
	}
	else
	{
		return (damier[colonne][ligne]);
	}
}

int get_couleur() {
	return couleur;
}

void set_couleur(int couleur_joueur) {
	couleur = couleur_joueur;
}

int get_couleur_adversaire() {
	if (get_couleur() == 1) {
		return 0; }
	if (get_couleur() == 0) {
		return 1; }
}

int get_couleur_adverse(int couleur) {
	if (couleur == 1) {
		return 0; }
	if (couleur == 0) {
		return 1; }
}

/* Fonction permettant de poser un jeton sur le plateau */
void poser_jeton(int col, int lig, int couleur_j) {
  damier[col][lig] = couleur_j;
  change_img_case(col, lig, couleur_j);
}

/* Fonction indiquants les cases jouables */
void indic_jouable(int couleur_j) {
	char *coord;
	int i, j;

	coord = malloc(3* sizeof(char));
	
	for (i = 0; i < 8; ++i)
	{
		for (j = 0; j < 8; ++j)
		{
			if ((return_couleur_case(i, j) == -1)
				&& (coup_valide(i, j, couleur_j))
				)
			{
				indexes_to_coord(i, j, coord);
				gtk_image_set_from_file(GTK_IMAGE (gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_jouable.png");
			}
			if ((return_couleur_case(i, j) == -1)
				&& !(coup_valide(i, j, couleur_j))
				)
			{
				indexes_to_coord(i, j, coord);
				gtk_image_set_from_file(GTK_IMAGE (gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_def.png");
			}
		}
	}
}

void reset_case_jouable() {
	char *coord;
	int i, j;

	coord = malloc(3* sizeof(char));
	
	for (i = 0; i < 8; ++i)
	{
		for (j = 0; j < 8; ++j)
		{
			if (return_couleur_case(i, j) == -1)
			{
				indexes_to_coord(i, j, coord);
				gtk_image_set_from_file(GTK_IMAGE (gtk_builder_get_object(p_builder, coord)), "UI_Glade/case_def.png");
			}
		}
	}	
}

/* ---------------------------------------------- FIN DES FONCTIONS ---------------------------------------------- */

/* ----------------------------------------------------- MAIN ---------------------------------------------------- */

int main(int argc, char **argv) {
	int i, j, ret;

	if (argc != 2)
	{
		printf("\nPrototype : ./othello num_port\n\n");
		exit(1);
	}

	/* Initialisation de GTK+ */
	gtk_init(&argc, &argv);

	/* Creation d'un nouveau GtkBuilder */
	p_builder = gtk_builder_new();

	if (p_builder != NULL)
	{
		/* Chargement du XML dans p_builder */
		gtk_builder_add_from_file(p_builder, "UI_Glade/Othello.glade", &p_err);

		if (p_err == NULL)
		{
			/* Recuperation d'un pointeur sur la fenetre. */
			GtkWidget *p_win = (GtkWidget *) gtk_builder_get_object(p_builder, "window1");

			/* En plus, ajout du port dans le titre */
			char titre[80];
			strcpy(titre, "Projet Othello - ");
			strcat(titre, argv[1]);
			gtk_window_set_title(GTK_WINDOW(p_win), titre);

			/* Gestion simplifiee evenement clic pour chacune des cases du damier */
			char nom_objet[20];
			for (char l = 'A'; l <= 'H'; l++) {
				for (int i = 1; i <= 8; i++) {
					sprintf(nom_objet, "eventbox%c%d", l, i);
					g_signal_connect(gtk_builder_get_object(p_builder, nom_objet), "button_press_event", G_CALLBACK(coup_joueur), NULL);
				}
			}

			/* Gestion clic boutons interface */
			g_signal_connect(gtk_builder_get_object
							 (p_builder, "button_connect"), "clicked",
							 G_CALLBACK(clique_connect_serveur), NULL);
			g_signal_connect(gtk_builder_get_object(p_builder, "button_start"),
							 "clicked", G_CALLBACK(clique_connect_adversaire),
							 NULL);

			/* Gestion clic bouton fermeture fenetre */
			g_signal_connect_swapped(G_OBJECT(p_win), "destroy",
									 G_CALLBACK(gtk_main_quit), NULL);

			/* Recuperation numero port donne en parametre */
			port = atoi(argv[1]);

			/* Initialisation du damier de jeu */
			for (i = 0; i < 8; ++i)
			{
				for (j = 0; j < 8; ++j)
				{
					damier[i][j] = -1;
				}
			}

			memset(&hints, 0, sizeof(hints));
			hints.ai_family = AF_UNSPEC;
			hints.ai_socktype = SOCK_STREAM;
			hints.ai_flags = AI_PASSIVE;
			status = getaddrinfo(NULL, argv[1], &hints, &servinfo);
			if (status != 0)
			{
				fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(status));
				return 1;
			}
			// Création socket et attachement
			for (p = servinfo; p != NULL; p = p->ai_next)
			{
				if ((sockfd =
					 socket(p->ai_family, p->ai_socktype,
							p->ai_protocol)) == -1)
				{
					perror("server: socket");
					continue;
				}
				if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1)	//attachement de la socket - serveur
				{
					close(sockfd);
					//FD_CLR(sockfd, &master);
					perror("server: bind");
					continue;
				}
				printf("[Port joueur : %s] Ok socket correct\n", argv[1]);
				break;
			}
			if (p == NULL)
			{
				fprintf(stderr, "server: failed to bind\n");
				return 2;
			}
			freeaddrinfo(servinfo);
			
			listen(sockfd, 1);

			FD_ZERO(&master);
			FD_ZERO(&read_fds);
			FD_SET(sockfd, &master);
			read_fds = master;
			fdmax = sockfd;

			ret = pthread_create(&thr_id, NULL, f_com_socket, NULL);
			if (ret != 0)
			{
				sprintf(stderr, "%s", strerror(ret));
				exit(1);
			}

			gtk_widget_show_all(p_win);
			gtk_main();
		}
		else
		{
			/* Affichage du message d'erreur de GTK+ */
			g_error("%s", p_err->message);
			g_error_free(p_err);
		}
	}


	return EXIT_SUCCESS;
}