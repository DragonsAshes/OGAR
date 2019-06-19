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

// compile with gcc -Wall -g -o sock ./test-client.c -lwebsockets
// to spectate ./spectacle -s -p 2008 192.168.130.150

typedef struct Cell
{
	unsigned int nodeID;
	unsigned int x,y;
	unsigned short size;//pas utile
	unsigned char flag;
	unsigned char r,g,b;
	char* name;
} Cell;

typedef struct my_infos_cell
{
	unsigned int mon_id;
	unsigned int coord_X,coord_Y;
	unsigned int mapX,mapY;
	unsigned int old_traj_X,old_traj_Y;
	unsigned int bord_left,bord_top,bord_right,bord_bottom;
} my_infos_cell;


typedef struct pile {
	Cell *cell;
	struct pile *next;
} Pile;

Pile *liste_mout = NULL;
Pile *liste_yellow = NULL;
Pile *liste_blue = NULL;
my_infos_cell *my_cell = NULL;

char* name = "blue";
int compteur_frame1=0;
int compteur_packet=0;
int rang=0;



typedef struct position_cursor{
	int posX;
	int posY;
}position_cursor;

position_cursor cursor_vect;
position_cursor near_pos;
bool trajet1=FALSE;
bool trajet2=FALSE;
bool firstborder = TRUE;
bool notup = TRUE;
bool begin = TRUE;
bool recherche = FALSE;
bool replac_projete = FALSE;
bool follow=FALSE;
bool envoiinfo=TRUE;
//malloc(sizeof(position_cursor));

unsigned char cursor[13]={0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};


bool	block_par_job=FALSE;

// =====================================================================================================================================
//	Start of function definition
// =====================================================================================================================================

// Caught on CTRL C
void sighandler(int sig)
{
	forceExit = 1;
}

/**
\brief Allocate a packet structure and initialise it.
\param none
\return pointer to new allocated packet
*********************************************************stray ‘\302*******************************************************************/
t_packet *allocatePacket()
{
	t_packet *tmp;

	if ((tmp=malloc(sizeof(t_packet))) == NULL ) return NULL;
	memset(tmp,0,sizeof(t_packet));
	return tmp;
}

/**
\brief Add a packet to the list of packet to be sent
\param wsi websocket descriptor
\param buf buffer to be sent
\param len length of packet
\return pointer to new allocated pastray ‘\302cket
****************************************************************************************************************************/
int sendCommand(struct lws *wsi,unsigned char *buf,unsigned int len)
{
	t_packet *tmp,*list=packetList;

	if (len > MAXLEN ) return -1;
	if ((tmp=allocatePacket()) == NULL ) return -1;
	memcpy(&(tmp->buf)[LWS_PRE],buf,len); //[LWS_PRE]
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

/****************************************************************************************************************************/
//sendCommand((cursor+1), &cursor_vect, sizeof(position_cursor)) // commande envoie
/****************************************************************************************************************************/
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
/****************************************************************************************************************************/

void move(struct lws *wsi, position_cursor cursor_vect)
{
  unsigned char paquet[13] = {0};
  paquet[0] = 0x10;
  memcpy(paquet+1, &(cursor_vect.posX), 4);
  memcpy(paquet+5, &(cursor_vect.posY), 4);
  sendCommand(wsi, paquet, 13);
}

/****************************************************************************************************************************/

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
/****************************************************************************************************************************/
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
void supressentete(Pile **pile)
{
	Pile *tmp = (*pile)->next;
	free(*pile);
	*pile = tmp;
}
/****************************************************************************************************************************/
void printNodeStack(Pile *pile)
{
	Pile *tmp = pile;

	if(tmp == NULL)
		printf("Empty");

	while(tmp != NULL)
	{
		printf("[%d, %d,%d] -> ",tmp->cell->nodeID, tmp->cell->x, tmp->cell->y);
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


/****************************************************************************************************************************/

int min(int a,int b)
{
	if (a<=b)
		return a;
	else
		return b;
}

/****************************************************************************************************************************/
void swap(int *a, int *b)
{
  int c;
  c = *a;
  *a = *b;
  *b = c;
}

/****************************************************************************************************************************/

void trie(int* tab, size_t len)
{
  int i, j;

  for(j = 0; j < len-1; j++)
  {
    for(i = 0; i < len-j-1; i++)
    {
      if( tab[i] > tab[i+1] )
      {
        swap( &tab[i], &tab[i+1]);
      }
    }
  }
}

/****************************************************************************************************************************/
position_cursor find_near_pos(int rang)
{
	int tab[4];
	int calcul1, calcul2, calcul3, calcul4;
	calcul1 = abs(my_cell->coord_X - (my_cell->mapX)/3.0) + abs(my_cell->coord_Y - (my_cell->mapY)/3.0);
  calcul2 = abs(my_cell->coord_X - (my_cell->mapX)/3.0) + abs(my_cell->coord_Y - 2*(my_cell->mapY)/3.0);
  calcul3 = abs(my_cell->coord_X - 2*(my_cell->mapX)/3.0) + abs(my_cell->coord_Y - (my_cell->mapY)/3.0);
  calcul4 = abs(my_cell->coord_X - 2*(my_cell->mapX)/3.0) + abs(my_cell->coord_Y - 2*(my_cell->mapY)/3.0);

  //utiliser un tableau pour ordonner les valeurs

  tab[0]=calcul1;
  tab[1]=calcul2;
  tab[2]=calcul3;
  tab[3]=calcul4;

  trie(tab, 4);

  if( tab[rang] == calcul1 )
  {
    near_pos.posX = (my_cell->mapX)/3.0;
    near_pos.posY = (my_cell->mapY)/3.0;
  }

  else if( tab[rang] == calcul2 )
  {
    near_pos.posX = (my_cell->mapX)/3.0;
    near_pos.posY = 2*(my_cell->mapY)/3.0;
  }

  else if( tab[rang] == calcul3 )
  {
    near_pos.posX = 2*(my_cell->mapX)/3.0;
    near_pos.posY = (my_cell->mapY)/3.0;
  }

  else if( tab[rang] == calcul4 )
  {
    near_pos.posX = 2*(my_cell->mapX)/3.0;
    near_pos.posY = 2*(my_cell->mapY)/3.0;
  }
	return near_pos;
}

/****************************************************************************************************************************/
void parcours_dog_func(struct lws *wsi)
{
	compteur_packet=compteur_packet+1;

	printf("%d paquet(s) \n",compteur_packet);

	if (compteur_packet < 500.0 && trajet2 == FALSE)// si l'on est à 2000 packets, alors on a passé approx. 25 sec que les jaunes soient placés
	{
		cursor_vect.posX=1000.0;
		cursor_vect.posY=1000.0;
		printf("here\n");
	}

	if (compteur_packet > 500.0)
	{

		if (my_cell->coord_X==500.0 && my_cell->coord_Y==1000.0)
		{
			notup=TRUE;
			if (trajet2 == FALSE)
				trajet1=TRUE;
		}
		if (my_cell->coord_X==((my_cell->mapX)-1000) && my_cell->coord_Y==((my_cell->mapY)-1000.0)  && trajet1 == FALSE)
		{
			trajet2=TRUE;
		}
		if (trajet1) //possiblement les trajets des blues se superposent et creeront pb sur les liste_mout
		{
			if (my_cell->coord_Y<=(my_cell->mapY)-1000)
			{
				if ((int)((my_cell->coord_Y)/2000.0)%2==0 && ((my_cell->coord_Y)/1000)%2!=0 && (my_cell->coord_Y)%1000==0 && notup) //ligne paire
				{
					cursor_vect.posX=((my_cell->mapX)-1000.0);
				}
				if ((my_cell->coord_X==(my_cell->mapX)-1000.0) && ((my_cell->coord_Y)%2000==1000))
				{
					if ((int)((my_cell->coord_Y)/2000.0)%2==0) //ligne paire
						cursor_vect.posY=(my_cell->coord_Y)+2000.0;
					if ((int)((my_cell->coord_Y)/2000.0)%2!=0) //ligne impaire
						cursor_vect.posX=1000;
				}
				if ((my_cell->coord_X==1000) && ((int)((my_cell->coord_Y)/2000.0)%2!=0) && ((my_cell->coord_Y)%2000==1000))
				{
					cursor_vect.posY=my_cell->coord_Y+2000;
				}
			}
			else
			{
				cursor_vect.posX=1000;
				cursor_vect.posY=1000;
				notup = FALSE;
			}
		}
		if (trajet2)//possiblement les trajets des blues se superposent et creeront pb sur les liste_mout
		{
			if ((my_cell->coord_X>=1000 && my_cell->coord_Y!=1000) || (my_cell->coord_X!=1000 && my_cell->coord_Y>=1000))
			{
				if ((my_cell->coord_Y)%1000==0)
				{
					if ((my_cell->mapY-1000)%2000==0) //last paire
					{
						if ((int)((my_cell->coord_Y)/2000.0)%2==0)
						{
							if (my_cell->coord_X==((my_cell->mapX)-1000))
								cursor_vect.posY=my_cell->coord_Y-2000;
							if (my_cell->coord_X==1000.0)
									cursor_vect.posX=(my_cell->mapX)-1000;
						}
						if ((int)((my_cell->coord_Y)/2000.0)%2!=0)
						{
							if (my_cell->coord_X==(my_cell->mapX)-1000.0)
								cursor_vect.posX=1000;

							if (my_cell->coord_X==1000.0)
								cursor_vect.posY=my_cell->coord_Y-2000;
						}
					}
					else // last impaire
					{
						if ((int) ((my_cell->coord_Y)/2000.0)%2==0 && (my_cell->coord_Y)%2000!=0)
						{
							if (my_cell->coord_X==(my_cell->mapX)-1000)
							{
								cursor_vect.posX=1000;
							}
							if (my_cell->coord_X==1000)
								cursor_vect.posY=my_cell->coord_Y-2000;
						}
						if ((int)((my_cell->coord_Y)/2000.0)%2!=0  && (my_cell->coord_Y)%2000!=0)
						{
							if (my_cell->coord_X==(my_cell->mapX)-1000)
								cursor_vect.posY=my_cell->coord_Y-2000;
							if (my_cell->coord_X==1000)
								cursor_vect.posX=(my_cell->mapX)-1000;
						}
					}
				}
			}
			else
			{
				cursor_vect.posX=my_cell->mapX-1000;
				cursor_vect.posY=my_cell->mapY-1000;
			}
		}
		my_cell->old_traj_X=cursor_vect.posX;
		my_cell->old_traj_Y=cursor_vect.posY;
	}






	if (liste_mout!=NULL)
	{
		if (envoiinfo)
		{
			position_cursor near_pos=find_near_pos(rang);
			if (recherche==FALSE)
			{
				cursor_vect.posX=near_pos.posX;
				cursor_vect.posY=near_pos.posY;
			}
			if ((my_cell->coord_X==cursor_vect.posX) && (my_cell->coord_Y==cursor_vect.posY))
			{
				cursor_vect.posX=near_pos.posX;
				cursor_vect.posY=near_pos.posY;
				printf("OOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOOO\n" );
				recherche=TRUE;
				rang=rang+1;
				if (rang==4)
				{
					rang=0;
					near_pos=find_near_pos(rang);
					cursor_vect.posX=near_pos.posX;
					cursor_vect.posY=near_pos.posY;
				}
			}
			block_par_job=TRUE;
		}
	}
	if (liste_mout==NULL && block_par_job)
	{
		cursor_vect.posX = my_cell->old_traj_X;
		cursor_vect.posY=my_cell->old_traj_Y;
		if ((my_cell->coord_X==cursor_vect.posX) && (my_cell->coord_Y==cursor_vect.posY))
		{
			block_par_job=FALSE;
		}
	}
	move(wsi,cursor_vect);
	printf("cursor\n");
	printf("%d\n",cursor_vect.posX);
	printf("%d\n",cursor_vect.posY);
	if (compteur_packet>20)
	{
		printf("position\n");
		printf("%d\n",my_cell->coord_X);
		printf("%d\n",my_cell->coord_Y);
		printf("/////////////////////////////////////\n");
	}
}




/****************************************************************************************************************************/

void recv_func(unsigned char *rbuf, struct lws *wsi)
{
	Pile* yellow_find;
	Pile* blue_find;
	Pile* mout_find;
	unsigned char buffer[6] ={0x00,0x62,0x6c,0x75,0x65,0x00}; //blue
	int coord_x_yellow, coord_y_yellow,coord_x_blue,coord_y_blue;
	int curs_tab=0;
	double mapXdouble,mapYdouble;
	printNodeStack(liste_mout);

	switch (rbuf[curs_tab])
	{
		case 0x12 :
			sendCommand(wsi,buffer,6);
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
			while (rbuf[curs_tab]!=0)
			{
				char* name_test=malloc(4);
				name_test[3]=0;
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
				}
				else
				{
					memcpy(name_test,&rbuf[curs_tab+18],3);
					if (strcmp(name_test,"yel")==0)
					{ //test si fist letter name is y donc pour yellow
						yellow_find=recherche_pile(liste_yellow, rbuf[curs_tab]);
						if (yellow_find!=NULL)
						{	// recherche dans la liste_yellow si cet ID est present
							memcpy(&coord_x_yellow,&rbuf[curs_tab+4],4); //prends les coord X et Y du yellow
							memcpy(&coord_y_yellow,&rbuf[curs_tab+8],4);
							bool test_pos1=(yellow_find->cell->x == (my_cell->mapX)/3 && yellow_find->cell->y == (my_cell->mapY)/3);
							bool test_pos2=(yellow_find->cell->x == (my_cell->mapX)/3 && yellow_find->cell->y == 2*(my_cell->mapY)/3);
							bool test_pos3=(yellow_find->cell->x == 2*(my_cell->mapX)/3 && yellow_find->cell->y == (my_cell->mapY)/3);
							bool test_pos4=(yellow_find->cell->x == 2*(my_cell->mapX)/3 && yellow_find->cell->y == 2*(my_cell->mapY)/3);
							bool arret_jaune=((yellow_find->cell->x==coord_x_yellow) && (yellow_find->cell->y==coord_y_yellow));
							if ((test_pos1 || test_pos2 || test_pos3 || test_pos4) && arret_jaune && recherche) // test si sur l'une des positions et à l'arret et blue en mode de give infos
							{
								printf("Test\n");
								if (distance(my_cell->coord_X,my_cell->coord_Y, coord_x_yellow ,coord_y_yellow)==0 || follow)
								{
									printf("ARRET\n");
									cursor_vect.posX=my_cell->coord_X;
									cursor_vect.posY=my_cell->coord_Y;
									envoiinfo=FALSE;
									compteur_frame1++;
									printf("compteur de frame : %d\n", compteur_frame1);
									if (compteur_frame1>=30) // compteur de frame --> wait give info to yellow
									{
										printf("DEPLACEMENT\n");
										cursor_vect.posX=liste_mout->cell->x;
										cursor_vect.posY=liste_mout->cell->y;
										follow=TRUE;
										if (compteur_frame1>=50)
										{
											printf("SUPPR MOUT\n");
											supressentete(&liste_mout);//CI
											compteur_frame1=0;
											recherche=FALSE;
											follow=FALSE;
										}
									}
								}
							}
						}
						Cell *upcell_yellow =malloc(sizeof(Cell));
						upcell_yellow->nodeID=rbuf[curs_tab];
						memcpy(&upcell_yellow->x,&rbuf[curs_tab+4],4);
						memcpy(&upcell_yellow->y,&rbuf[curs_tab+8],4);
						insertion(&liste_yellow,upcell_yellow);
					}
					if (strcmp(name_test,"blu")==0)
					{ // test si blue (autre que moi)
						blue_find=recherche_pile(liste_blue, rbuf[curs_tab]);
						if (blue_find !=NULL)
						{	// recherche dans la liste_yellow si cet ID est present
							memcpy(&coord_x_blue,&rbuf[curs_tab+4],4); //prends les coord X et Y du blue
							memcpy(&coord_y_blue,&rbuf[curs_tab+8],4);
							bool pos_coin=(coord_x_blue == 1000 && coord_y_blue == 1000);
							if (pos_coin && begin)
							{ // coin sup gauche occupe
								cursor_vect.posX=(my_cell->mapX)-1000; // envoie les coords du coin inf droit
								cursor_vect.posY=(my_cell->mapY)-1000;
								trajet2=TRUE;
								begin=FALSE;
							}
						}
						else
						{
							Cell *upcell_blue =malloc(sizeof(Cell)); // add new blue a la liste_blue
							upcell_blue->nodeID=rbuf[curs_tab];
							upcell_blue->x=rbuf[curs_tab+4];
							upcell_blue->y=rbuf[curs_tab+8];
							insertion(&liste_blue,upcell_blue);
						}
					}
				}
				curs_tab=curs_tab+18+strlen(rbuf+curs_tab+18)+1;
			}
			break;

		default :
			break;
	}
	/*printNodeStack(liste_mout);*/
}
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
				parcours_dog_func(wsi);
				recv_func(rbuf,wsi);




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

/****************************************************************************************************************************/
int main(int argc, char **argv)
{
	my_cell = malloc(sizeof(my_infos_cell));

	int n = 0;

	struct lws_context_creation_info info;
	struct lws_client_connect_info i;

	struct lws_context *context;
	const char *protocol,*temp;

	memset(&info, 0, sizeof info);
	memset(&i, 0, sizeof(i));

	if (argc < 2);
		//goto usage;

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
			//goto usage;
			break;
		}
	}

	srandom(time(NULL));

	if (optind >= argc);
		//goto usage;

	signal(SIGINT, sighandler);

	if (lws_parse_uri(argv[optind], &protocol, &i.address, &i.port, &temp));
		//goto usage;

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
/*
usage:
	fprintf(stderr, "Usage: ogar-client -h -s -p <port> -P <proxy> -o <origin> <server adress> \n");
	return 1;*/
}
