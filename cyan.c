#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <sys/time.h>
#include <unistd.h>
#include <math.h>
#include "client.h"
#include "libwebsockets.h"

int sendCommand(struct lws *wsi,unsigned char *buf,unsigned int len);
int monID;

typedef enum DOG
{
  INIT,
  BALAYAGE,
  PUSHING,
} DOG;

DOG cyan = INIT;

typedef struct vecteur{
  int x;
  int y;
}vecteur;

vecteur direction;

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

Pile* chaine = NULL;


void sighandler(int sig)
{
  forceExit = 1;
}

t_packet *allocatePacket()
{
  t_packet *tmp;
  if ((tmp = malloc(sizeof(t_packet))) == NULL) return NULL;
  memset(tmp, 0, sizeof(t_packet));
  return tmp;
}

void move(struct lws *wsi, vecteur vect)
{
  unsigned char paquet[13] = {0};
  paquet[0] = 0x10;
  memcpy(paquet+1, &(vect.x), 4);
  memcpy(paquet+5, &(vect.y), 4);
  sendCommand(wsi, paquet, 13);
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

void insertion(Pile **pile, Cell *upcell){

    Pile *tmp = *pile;

  	tmp=malloc(sizeof(Pile));
    tmp->next = *pile;
    tmp->cell = upcell;
    *pile = tmp;

}


void supressall(Pile **pile)
{
	Pile* tmp = *pile;
	while(*pile != NULL)
	{
		tmp = *pile;
		*pile = tmp->next;
		free(tmp->cell->name);
		free(tmp->cell);
		free(tmp);
	}
}

void printNodeStack(Pile* pile)
{
  Pile *tmp = pile;

  if(tmp == NULL)
    printf("Empty\n");

  while (tmp != NULL)
  {
    printf("[%d, %d, %s] -> ", tmp->cell->nodeID, tmp->cell->size, tmp->cell->name);
    tmp = tmp->next;
  }
  printf("\n");
}

void update(unsigned char *paquet)
{
	supressall(&chaine);
	printNodeStack(chaine);
	int i;
	unsigned short deadcellslength;
		memcpy(&deadcellslength, paquet+1, sizeof(unsigned short));
		i = deadcellslength*8 + 3;
		while(paquet[i] != 0)
		{
			Cell *node = malloc(sizeof(Cell));
			memcpy(node, paquet+i, 18); // 18 = sizeof(Cell) - sizeof(char*)
			i+=18;

			if (node->flag & 0x8)
			{
				node->name = malloc(strlen(paquet+i)+1);
				strcpy(node->name, paquet+i);
				i += strlen(node->name)+1;
			}
			printf("Contenu de la cellule : ID : %d x : %d y : %d \n",node->nodeID, node->x, node->y );

			insertion(&chaine, node);

		}
		i++;
		unsigned short removecellslength;
		memcpy(&removecellslength, paquet+i, sizeof(unsigned short) );
		unsigned int removecellsID;
		i+=2;

		//printNodeStack(chaine);
}

void initialize(struct lws *wsi, Pile* pile)
{
	vecteur position;
	Pile *tmp = pile;
	position.x = 8650;
	position.y = 2600;
	move(wsi, position);
	while(tmp != NULL && tmp->cell->nodeID != monID)
		tmp = tmp->next;
	if(tmp == NULL)
		return;
	printf("POSITION : %d / %d et ID = %d\n", position.x, position.y, monID);
	if(tmp->cell->x == position.x && tmp->cell->y == position.y)
	{
		cyan = BALAYAGE;
	}

}

void balaye(struct lws *wsi, Pile* pile)
{
	static int mode = 0; //Si mode == 0 alors on descend, si mode == 1 alors on monte
	Pile *tmp = pile;
	vecteur destination;
	if(mode == 0)
	{
		destination.x = 8500;
		destination.y = 3400;
	}
	else if (mode == 1)
	{
		destination.x = 8500;
		destination.y = 2600;
	}
	while(tmp != NULL && tmp->cell->nodeID != monID)
		tmp = tmp->next;
	if(tmp == NULL)
		return;

	if(tmp->cell->x == 8500 && tmp->cell->y == 3400)
		mode = 0;
	else if(tmp->cell->x == 8500 && tmp->cell->y == 2600)
		mode = 1;

	tmp = chaine;
	while (tmp != NULL && strncmp("bot", tmp->cell->name, 3) != 0)
		tmp = tmp->next;
	if(tmp == NULL)
	{
		move(wsi, destination);
		return;
	}
	else
	{
		direction.x = tmp->cell->x;
		direction.y = tmp->cell->y;
		move(wsi, destination);
		cyan = PUSHING;
		return;
	}

}

void push(struct lws *wsi, Pile *pile)
{
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

	teta = atanf( (float) (position.x - 7500) / ((float) (5000 - position.y)));
	x = sinf(teta) * 50;
	y = sinf(teta) *50;
	direction.x = (int) (position.x + x);
	direction.y = (int) (position.y +y);
	if (direction.x < 0+32 )
		direction.x = 32;
	if (direction.x > 9000-32)
		direction.x = 9000-32;
	if(direction.y < 0+32)
		direction.y = 32;
	if (direction.y > 6000-32)
		direction.y = 6000-32;
	move(wsi, direction);
	if(position.x < 7500 && position.y > 5000 )
		cyan = INIT;

}



int first_ID = 0;
int recv_packet(unsigned char *paquet, struct lws *wsi)
{
	switch (paquet[0])
	{
		case 0x10 :
			if( cyan == INIT)
			{
				printf("INIT\n");
				update(paquet);
				Pile *tmp = chaine;
				/*while(tmp ->next != NULL)
				{
					if( first_ID == 0 && strcmp(tmp->cell->name, "cyan") == 0)
					{
						monID = tmp->cell->nodeID;
						first_ID = 1;
					}
					tmp = tmp->next;
				}*/

				initialize(wsi, chaine);
			}
			else if( cyan == BALAYAGE)
			{
				printf("BALAYAGE\n");
				update(paquet);
				balaye(wsi, chaine);
			}
			else if( cyan == PUSHING)
			{
				printf("PUSHING\n");
				update(paquet);
				push(wsi, chaine);
			}
			break;
		case 0x20 :
			monID = paquet[1];
	}
}








static int callbackOgar(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
	static unsigned int offset=0;
	static unsigned char rbuf[MAXLEN];

	unsigned char connection[5] = {0xff, 0, 0, 0, 0};
	unsigned char start[5] = {0xfe, 0x0d, 0, 0, 0};

	switch (reason) {
	case LWS_CALLBACK_CLIENT_ESTABLISHED:

		sendCommand(wsi, connection, 5);
		sendCommand(wsi, start, 5);

    char* nom = "cyan";
    unsigned char* paquet = calloc(strlen(nom)+2, 1);
    memcpy(paquet+1, nom, strlen(nom));
    sendCommand(wsi, paquet, strlen(nom)+2);

		fprintf(stderr, "ogar: LWS_CALLBACK_CLIENT_ESTABLISHED\n");
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
				// call recv function here
				recv_packet(rbuf, wsi);

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

int main(int argc, char const *argv[])
{
	if(argc < 3)
	{
		printf("Use ./prog ip port\n");
		return 1;
	}


	struct lws_context_creation_info info;
	struct lws_client_connect_info i;

	struct lws_context *context;
	const char *protocol,*temp;

	memset(&info, 0, sizeof info);
	memset(&i, 0, sizeof(i));

	signal(SIGINT, sighandler);

	i.origin = "agar.io";
	i.port = atoi(argv[2]);

	if (lws_parse_uri(argv[1], &protocol, &i.address, &i.port, &temp))
		;

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
}