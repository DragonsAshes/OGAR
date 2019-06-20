#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <sys/time.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include "client.h"
#include "libwebsockets.h"


#define TRUE 1
#define FALSE 0

bool haut = FALSE;
bool bas = FALSE;
bool mouton_en_vue = FALSE;
bool bien_place = FALSE;
bool revenir = FALSE;
bool enX = FALSE;
bool enYbas = FALSE;
bool enYhaut = FALSE;





int sendCommand(struct lws *wsi,unsigned char *buf,unsigned int len);


typedef struct my_infos_cell
{
	unsigned int mon_id;
	unsigned int coord_X,coord_Y;
	unsigned int mapX,mapY;
	unsigned int old_traj_X,old_traj_Y;
	unsigned int bord_left,bord_top,bord_right,bord_bottom;
} my_infos_cell;

typedef struct Cell
{
    unsigned int nodeID;
    unsigned int x, y;
    unsigned short size;
    unsigned char flag;
    unsigned char r,g,b;
    char* name;
}Cell;

typedef struct Pile{
  Cell *cell;
  struct Pile *next;
} Pile;


Pile *liste_mout = NULL;
my_infos_cell *my_cell = NULL;
char* name = "cyan";

bool firstborder = TRUE;
bool init = TRUE;

typedef struct vecteur{
  int x;
  int y;
}vecteur;

vecteur position;

void sighandler(int sig)
{
  forceExit = 1;
}

t_packet *allocatePacket()
{
	t_packet *tmp;

	if ((tmp=malloc(sizeof(t_packet))) == NULL ) return NULL;
	memset(tmp,0,sizeof(t_packet));
	return tmp;
}


int sendCommand(struct lws *wsi,unsigned char *buf,unsigned int len)
{
	t_packet *tmp,*list=packetList;

	if (len > MAXLEN ) return -1;
	if ((tmp=allocatePacket()) == NULL ) return -1;
	memcpy(&(tmp->buf)[LWS_PRE],buf,len);
	tmp->len=len;
	if (packetList == NULL )
		packetList=tmp;
	else {
		while (list && list->next) {
			list=list->next;
		}
		list->next=tmp;
	}
	lws_callback_on_writable(wsi);
	return 1;
}
////////////////////////////////////////////////////////////
static int start_connection(struct lws *wsi)
{
	unsigned char	connexion_start_command[5] = {0xff,0,0,0,0};
	unsigned char	connexion_command[5] = {0xfe,0xd,0,0,0};
	sendCommand(wsi, connexion_start_command,5);
	printf("connexion start ok\n");
	sendCommand(wsi,connexion_command,5);
	printf("connexion com ok\n");
// add -n nickname dans la commande pour la selection du chien


	return 1;
}


void insertion(Pile **pile, Cell *upcell){

    Pile *tmp = *pile;
    while(tmp != NULL)
    {
    	if(tmp->cell->nodeID == upcell->nodeID) // parcours la pile en cherchant un ID identique /si oui alors update des donnees (coords)
    	{																																												// si non alors creation de l element en fin de pile
    		free(tmp->cell);
    		tmp->cell = upcell;
    		return;
    	}
    	tmp = tmp->next;
    }
  	tmp=malloc(sizeof(Pile)); //free upcell

    tmp->next = *pile;
    tmp->cell = upcell;
    *pile = tmp;
}
/****************************************************************************************************************************/
void supressID(Pile **pile, unsigned int removecellsID)
{
	Pile *tmp = *pile;
	Pile *prec = *pile;

	if(tmp->cell->nodeID == removecellsID)
	{
		*pile = tmp->next;
		free(tmp->cell);
		free(tmp);
	}

	while(tmp != NULL)
	{
		if(tmp->cell->nodeID == removecellsID) //parcours la pile entiere a la recherche dun ID souhaité et delet
		{

			prec -> next = tmp -> next;
			free(tmp->cell);
			free(tmp);
			return;

		}
		prec = tmp;
		tmp = tmp->next;
	}
}
/****************************************************************************************************************************/
void printNodeStack(Pile *pile)
{
	Pile *tmp = pile;

	if(tmp == NULL)
		printf("Empty");

	while(tmp != NULL)
	{
		printf("[%d, %d, %s] -> ", tmp->cell->nodeID, tmp->cell->size, tmp->cell->name);
		tmp = tmp->next;
	}
	printf("\n");
}
/****************************************************************************************************************************/
Pile *recherche_pile(Pile *pile, unsigned int search_ID)
{
	Pile *tmp = pile;

	while(tmp != NULL)
	{
		if(tmp->cell->nodeID == search_ID) //parcours la pile entiere a la recherche dun ID souhaité et return celui ci
			return tmp;
		tmp = tmp->next;
	}
	return NULL;
}
/****************************************************************************************************************************/
int distance(int coordxA,int coordyA,int coordxB,int coordyB)
{
	return sqrt(pow((coordxB-coordxA),2)+pow((coordyB-coordyA),2)	);
}


////////////////////////////////////////////////
int writePacket(struct lws *wsi)
{
	t_packet *tmp=packetList;
	int ret;

	if (packetList == NULL ) return 0;

	packetList=tmp->next;
	ret=lws_write(wsi,&(tmp->buf)[LWS_PRE],tmp->len,LWS_WRITE_BINARY);
	free(tmp);
	lws_callback_on_writable(wsi);
	return(ret);
}




void move(struct lws *wsi, vecteur vect)
{
  unsigned char paquet[13] = {0};
  paquet[0] = 0x10;
  memcpy(paquet+1, &(vect.x), 4);
  memcpy(paquet+5, &(vect.y), 4);
  sendCommand(wsi, paquet, 13);
}



void recv_func(unsigned char *rbuf, struct lws *wsi)
{

	Pile* yellow_find;
	Pile* blue_find;
	unsigned char buffer[6] = {0x00,0x63,0x79,0x61,0x6e,0x00}; //cyan{0x00,0x63,0x79,0x61,0x6e,0x00}; blue{0x00,0x62,0x6c,0x75,0x65,0x00}
	int coord_x_yellow, coord_y_yellow,coord_x_blue,coord_y_blue;
	int curs_tab=0;
	double mapXdouble,mapYdouble;
	switch (rbuf[curs_tab])
	{
		case 0x12 :
			sendCommand(wsi,buffer,6);
			printf("chien added\n");
			break;

		case 0x20 ://id 32 correspond a mon id
			my_cell->mon_id=rbuf[1];
			break;

		case 0x40 :// id 64 correspond a border
			memcpy(&my_cell->bord_left,&rbuf[curs_tab+1],8); //prends les coord des borders
			memcpy(&my_cell->bord_top,&rbuf[curs_tab+9],8);
			memcpy(&my_cell->bord_right,&rbuf[curs_tab+17],8);
			memcpy(&my_cell->bord_bottom,&rbuf[curs_tab+25],8);
			if ((int)my_cell->bord_left==0 && (int)my_cell->bord_top==0 && firstborder)
			{
				memcpy(&mapXdouble,&rbuf[curs_tab+17],8);
				memcpy(&mapYdouble,&rbuf[curs_tab+25],8);
				my_cell->mapX=mapXdouble;
				my_cell->mapY=mapYdouble;
				firstborder = FALSE;
			}
			break;

		case 0x10 : // id 16 correspond a l'update node (quels sont les chiens/ mout dans la vision)
			curs_tab=curs_tab+3;
			char* name_test=malloc(4);
			name_test[3]=0;
			while (rbuf[curs_tab]!=0)
			{
				memcpy(name_test,&rbuf[curs_tab+18],3);
				if( strcmp(name_test,"bot")==0 )//test si bot
				{
					Cell *upcell =malloc(sizeof(Cell));
					upcell->nodeID=rbuf[curs_tab];
					memcpy(&upcell->x,&rbuf[curs_tab+4],4);
					memcpy(&upcell->y,&rbuf[curs_tab+8],4);
					insertion(&liste_mout,upcell);
				}
				if (rbuf[curs_tab]==my_cell->mon_id)
				{
					memcpy(&my_cell->coord_X,&rbuf[curs_tab+4],4); //prends les coord X et Y du blue
					memcpy(&my_cell->coord_Y,&rbuf[curs_tab+8],4);
				//	curs_tab=curs_tab+18+strlen(rbuf+curs_tab+18)+1;
				}
				curs_tab=curs_tab+18+strlen(rbuf+curs_tab+18)+1; // passage au bot ou mout suivant
			}
			break;

		default :
			break;
	}

}



void initialize(struct lws *wsi)
{
	printf("on est dans INITIALIZE\n");
	vecteur position;
	int rayon = ((my_cell->mapX)/10.0);
	int pos_init_Y = (my_cell->mapY / 2) - rayon + 500; //moitié
	int pos_init_X = my_cell->mapX - 350;

	position.x = pos_init_X;
	position.y = pos_init_Y;
	move(wsi, position);
	printf("position X : %d\nposition Y : %d\n", position.x, position.y);
	printf("coordX : %d\ncoordY : %d\n", my_cell->coord_X, my_cell->coord_Y);
	if ((my_cell->coord_X == pos_init_X) && (my_cell->coord_Y == pos_init_Y))
	{
		init = FALSE; //mettre qd on est bien placé
		haut = TRUE;
		printf("INITIALIZE done\n");
	}
	liste_mout = NULL;
}



void trajet(struct lws *wsi) // faire un balayage dans la zone adv
{

//	printf("bas = %d\nhaut = %d\n", bas, haut);
	vecteur position;
	int rayon = ((my_cell->mapX)/10.0);

	if(mouton_en_vue == FALSE){
		printf("on est dans TRAJET\n");
		if (haut == TRUE) //si on est en haut
		{
			printf("descente\n");
			int pos_bas_X = my_cell->mapX - 350;
			int pos_bas_Y = (my_cell->mapY / 2) + rayon - 500;
			position.x = pos_bas_X;
			position.y = pos_bas_Y;
			move(wsi, position);
		//	printf("coordX = %d\ncoordY = %d\n", my_cell->coord_X, my_cell->coord_Y);
			if ((my_cell->coord_X == pos_bas_X) && (my_cell->coord_Y == pos_bas_Y))
			{
				haut = FALSE;
				bas = TRUE;
				printf("descente DONE\n");
			}

		}

		if (bas == TRUE) //si on est en bas
		{
			printf("montée\n");
			int pos_haut_X = my_cell->mapX - 350;
			int pos_haut_Y = (my_cell->mapY / 2) - rayon + 500;
			position.x = pos_haut_X;
			position.y = pos_haut_Y;
			move(wsi, position);
			if ((my_cell->coord_X == pos_haut_X) && (my_cell->coord_Y == pos_haut_Y))
			{
				haut = TRUE;
				bas = FALSE;
				printf("montée DONE\n");
			}
		}
	}
}


void pousse (struct lws *wsi) //OU TOUT REFAIRE AVEC LA PUSH DE SEB :'(    (bonhomme triste)
{
//OUI je suis claquée je pointe pas quelque part je me cale juste a 300 du mouton et je vais dans une direction et ca le pousse tout seul

	//ce qui ne marche pas :
	//print cazou alors que ca ne devrait pas car
		// bien_place passe a true pour que ca move a un endroit mais apres aucune condition de move est respectéé pour savoir la direction
		//ca print cazou ( aucune des 3 conditions respectée)
		// mais quand ca print CAZOU normalement ca fait r mais le pb c'est que ca move quelque part jsp ou et apres ca s'arrete

//yo

// et (merci olivier) tout est dans le callbackOgar

//CA ME FAAAAAAAAAAAAAAAAAAAAAAAANE

// et juge pas mon code tout plein de if et de bool

// sur ce CAZOU Cazou

	int rayon = ((my_cell->mapX)/10.0);

	vecteur position;
	if (liste_mout != NULL)
	{
		mouton_en_vue = TRUE;

		int pos_mouton_x = liste_mout->cell->x;
		int pos_mouton_y = liste_mout->cell->y;

// si les x et y sont 300 plus loin que le mouton a 5 pixels pres
		bool condX = ( (abs(my_cell->coord_X - (pos_mouton_x + 300) )) < 5) && ( (abs(my_cell->coord_Y - pos_mouton_y)) <5) ;
		bool condYbas = ( (abs(my_cell->coord_X - pos_mouton_x)) <5 ) && ( (abs(my_cell->coord_Y - (pos_mouton_y - 300))) < 5) ;
		bool condYhaut = ( (abs(my_cell->coord_X - pos_mouton_x)) <5 ) &&  ((abs(my_cell->coord_Y - (pos_mouton_y + 300))) < 5) ;

		printf("condX = %d\ncondYbas = %d\ncondYhaut = %d\n", condX, condYbas, condYhaut);

  /*bool condX = ( (my_cell->coord_X == (pos_mouton_x + 300) ) && ( (my_cell->coord_Y == pos_mouton_y)) );
		bool condYbas = ( (my_cell->coord_X == pos_mouton_x) && (my_cell->coord_Y - (pos_mouton_y - 300)) ) ;
		bool condYhaut = ( (my_cell->coord_X == pos_mouton_x)  &&   (my_cell->coord_Y == (pos_mouton_y + 300) ) ) ;*/

		printf("bien_place = %d\n", bien_place);

		if (bien_place == FALSE && revenir == FALSE)
		{
			if (pos_mouton_x < (my_cell->mapX - 300))
			{
				printf("va se caller en x\n");
				position.x = pos_mouton_x + 300;
				position.y = pos_mouton_y;
				move(wsi, position); // se place a 300 (rayon d'action) en x du premier mouton liste
				if (condX == TRUE)
				{
					bien_place = TRUE;
					printf("bien_place est passé a TRUE\n");
				}

			}
			else {
				if (pos_mouton_y > (my_cell->mapY / 2) ) //si sup a la moitié => va vers le bas
				{
					printf("va se caller en y direction bas\n");
					position.x = pos_mouton_x;
					position.y = pos_mouton_y - 300;
					move(wsi, position);
					if ( condYbas == TRUE )
					{
						bien_place = TRUE;
						printf("bien_place est passé a TRUE\n");
					}

				}
				if (pos_mouton_y < (my_cell->mapY / 2) ) //si inf a la moitié => va vers le haut
				{
					printf("va se caller en y direction haut\n");
					position.x = pos_mouton_x;
					position.y = pos_mouton_y + 300;
					move(wsi, position);
					if ( condYhaut == TRUE )
					{
						printf("bien_place est passé a TRUE\n");
						bien_place = TRUE;
					}
				}
			}
		}

		if(bien_place == TRUE && revenir == FALSE)
		{
			printf("ON POUSSE\n");

			if ( condX == TRUE ) // suivant x
			{
				printf("suivant x\n");
				position.x = (my_cell->mapX) - rayon - 100; //va 100 plus loin des cages
				position.y = pos_mouton_y; //va a hauteur du mouton
				move(wsi, position);
				printf("directionX = %d\ndirectionY = %d\n", position.x, position.y);
				//////////
				if ( ( abs( my_cell->coord_X - ( (my_cell->mapX) - rayon - 100))) < 5)
				{
					printf("arrivé a destination\n");
					bien_place = FALSE;
					revenir = TRUE;
					enX = TRUE;
				}
			}

		//	else
			//	bien_place = FALSE;

			if (condYhaut == TRUE ) // pousse suivant y en haut
			{
				printf("suivant y haut\n");
				position.y = (my_cell->mapY / 2) - rayon - 100 ; //va 100 plus loin des cages en hauut
				position.x = pos_mouton_x; //coord_X
				move(wsi, position);
				printf("directionX = %d\ndirectionY = %d\n", position.x, position.y);
				//////////////
				if ( ( abs(my_cell->coord_Y -( ( my_cell->mapY / 2) - rayon - 100)) ) <5 )
				{
					printf("arrivé a destination\n");
					bien_place = FALSE;
					revenir = TRUE;
					enYhaut = TRUE;
				}
			}

			if (condYbas == TRUE) // pousse suivant y en bas
			{
				printf("suivant y bas\n");
				position.y = (my_cell->mapY / 2) + rayon + 100 ; //va 100 plus loin des cages en bas
				position.x = pos_mouton_x; //coord_X
				move(wsi, position);
				printf("directionX = %d\ndirectionY = %d\n", position.x, position.y);
				/////////////
				if ( ( abs(my_cell->coord_Y - ((my_cell->mapY / 2) + rayon + 100))) < 5)
				{
					printf("arrivé a destination\n");
					bien_place = FALSE;
					revenir = TRUE;
					enYbas = TRUE;

				}
			}
			if ( (condX == FALSE) && (condYbas == FALSE) && (condYhaut == FALSE))
				{
					printf("CAZOU\n");
			//		bien_place = FALSE;
				}
		}


		if (revenir == TRUE)
		{
			printf("on rentre\n");

			if (enYhaut == TRUE) // suivant x
			{
				printf("suivant x\n");
				//retourne la ou on était
				position.x = my_cell->mapX - 350; //reviens a la position x de init
				position.y = pos_mouton_y; //coord_Y
				move(wsi, position);
				if (my_cell->coord_X == my_cell->mapX - 350)
				{
					printf("bien arrivé\n");
					mouton_en_vue = FALSE;
					revenir = FALSE;
					enX = FALSE;
					// + tej le mouton de la liste ?
				}
			}

			if(enYhaut == TRUE) // suivant y haut
			{
				printf("suivant y haut\n");
				//retourne la ou on était
				position.x = pos_mouton_x; //reste sur le meme x //coord_X
				position.y = (my_cell->mapY / 2) - rayon + 500; //reviens a position init du haut
				move(wsi, position);
				if (my_cell->coord_Y == (my_cell->mapY / 2) - rayon + 500)
				{
					printf("bien arrivé\n");
					mouton_en_vue = FALSE;
					revenir = FALSE;
					enYhaut = FALSE;
					// + tej le mouton de la liste ?
				}
			}

			if (enYbas == TRUE) // suivant y bas
			{
				//retourne la ou on était
				position.x = pos_mouton_x; //reste sur le meme x //coord_X
				position.y = (my_cell->mapY / 2) + rayon - 500; //reviens a position init du bas
				move(wsi, position);
				if (my_cell->coord_Y == (my_cell->mapY / 2) + rayon - 500)
				{
					printf("bien arrivé\n");
					mouton_en_vue = FALSE;
					revenir = FALSE;
					enYbas = FALSE;
					// + tej le mouton de la liste ?
				}
			}
		}
		printf("pos mouton x = %d\npos mouton y = %d\n", pos_mouton_x, pos_mouton_y);
		printf("coordX = %d\ncoordY = %d\n", my_cell->coord_X, my_cell->coord_Y);
	}
}

/*void push(struct lws* wsi, Pile *pile)
{
	if (liste_mout != NULL){
		mouton_en_vue = TRUE;

		vecteur position;
		vecteur direction;
		int teta;
		int x, y;
		Pile *tmp = pile;
		while(tmp != NULL && strncmp("bot", tmp->cell->name, 3) != 0)
			tmp = tmp->next;
		if(tmp == NULL)
			return;

		position.x = tmp->cell->x;
		position.y = tmp->cell->y;

		teta = atanf ( (float) (position.y - 3000) / ((float) (position.x)));
		x = cosf(teta) *50;
		y = sinf(teta) *50;
		direction.x = (int) (position.x + x);
		direction.y = (int) (position.y + y);

		if (direction.x < 0+32 )
			direction.x = 32;
		if (direction.x > 9000-32)
			direction.x = 9000-32;
		if(direction.y < 0+32)
			direction.y = 32;
		if (direction.y > 6000-32)
			direction.y = 6000-32;
		move(wsi, direction);
		if ( (position.x < 636) && (position.y > 2364 && position.y < 3636)) //changer
		{
			mouton_en_vue == FALSE;
		}
	}
}*/




/****************************************************************************************************************************/
static int callbackOgar(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	static unsigned int offset=0;
	static unsigned char rbuf[MAXLEN];

	switch (reason) {
	case LWS_CALLBACK_CLIENT_ESTABLISHED:
		fprintf(stderr, "ogar: LWS_CALLBACK_CLIENT_ESTABLISHED\n");
		start_connection(wsi);
		break;

 	case LWS_CALLBACK_CLIENT_WRITEABLE:
		if (writePacket(wsi) < 0 ) forceExit=1;
		break;

	case LWS_CALLBACK_CLIENT_RECEIVE:
		// we have receive some data, check if it can be written in static allocated buffer (length)

		if (offset + len < MAXLEN ) {
			memcpy(rbuf+offset,in,len);
			offset+=len;
			// we have receive some data, check with websocket API if this is a final fragment
			if (lws_is_final_fragment(wsi)) {
				recv_func(rbuf,wsi);

				if (init == TRUE){
					initialize(wsi);
				}
				if (init == FALSE){
					trajet(wsi);
					pousse (wsi);
				}

				offset=0;
			}
		} else {	// length is too long... get others but ignore them...
			offset=MAXLEN;
		 	if ( lws_is_final_fragment(wsi) ) {
				offset=0;
			}
		}
		break;
	case LWS_CALLBACK_CLOSED:
		lwsl_notice("ogar: LWS_CALLBACK_CLOSED\n");
		forceExit = 1;
		break;
	case LWS_CALLBACK_CLIENT_CONNECTION_ERROR:
		lwsl_err("ogar: LWS_CALLBACK_CLIENT_CONNECTION_ERROR\n");
		forceExit = 1;
		break;

	case LWS_CALLBACK_COMPLETED_CLIENT_HTTP:
		lwsl_err("ogar: LWS_CALLBACK_COMPLETED_CLIENT_HTTP\n");
		forceExit = 1;
		break;

	default:
		break;
	}

	return 0;
}


int main(int argc, char **argv)
{
	int n = 0;
	my_cell = malloc(sizeof(my_infos_cell));

	struct lws_context_creation_info info;
	struct lws_client_connect_info i;

	struct lws_context *context;
	const char *protocol,*temp;

	memset(&info, 0, sizeof info);
	memset(&i, 0, sizeof(i));

	if (argc < 2)
		goto usage;


	while (n >= 0) {
		n = getopt(argc, argv, "hsp:P:o:");
		if (n < 0)
			continue;
		switch (n) {
		case 's':
			i.ssl_connection = 2;
			break;
		case 'p':
			i.port = atoi(optarg);
			break;
		case 'o':
			i.origin = optarg;
			break;
		case 'P':
			info.http_proxy_address = optarg;
			break;
		case 'h':
			goto usage;
		}
	}

	srandom(time(NULL));

	if (optind >= argc)
		goto usage;

	signal(SIGINT, sighandler);

	if (lws_parse_uri(argv[optind], &protocol, &i.address, &i.port, &temp))
		goto usage;

	if (!strcmp(protocol, "http") || !strcmp(protocol, "ws"))
		i.ssl_connection = 0;
	if (!strcmp(protocol, "https") || !strcmp(protocol, "wss"))
		i.ssl_connection = 1;

	i.host = i.address;
	i.ietf_version_or_minus_one = -1;
	i.client_exts = NULL;
	i.path="/";

	info.port = CONTEXT_PORT_NO_LISTEN;
	info.protocols = protocols;
	info.gid = -1;
	info.uid = -1;

	context = lws_create_context(&info);
	if (context == NULL) {
		fprintf(stderr, "Creating libwebsocket context failed\n");
		return 1;
	}

	i.context = context;

	if (lws_client_connect_via_info(&i)); // just to prevent warning !!

	forceExit=0;
	// the main magic here !!
	while (!forceExit) {
		lws_service(context, 1000);
	}
	// if there is some errors, we just quit
	lwsl_err("Exiting\n");
	lws_context_destroy(context);

	return 0;

usage:
	fprintf(stderr, "Usage: ogar-client -h -s -p <port> -P <proxy> -o <origin>  <server address> \n");
	return 1;
}
