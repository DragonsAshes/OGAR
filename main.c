#include "websockets.h"
#include <signal.h>
#include <math.h>

typedef struct position_cursor{
	int posX;
	int posY;
}position_cursor;

position_cursor* cursor_vect = NULL;



typedef struct Cell
{
	unsigned int nodeID;
	unsigned int x,y;
	unsigned short size;
	unsigned char flag;
	unsigned char r,g,b;
	char* name;
} Cell;

//Cell cell;
//memcpy(&cell, tableau + position, sizeof(Cell) - sizeof(char*));

typedef struct pile {
	Cell *cell;
	struct pile *next;
} Pile;

Pile *chaine = NULL;

void initialize(struct lws *wsi, Pile* pile)
{
	int calcul1, calcul2;
	Pile* tmp = pile
	while(strcmp(tmp->cell->name, "blue") != 0)
	{
		tmp = tmp->next;
	}
	calcul1 = abs(tmp->cell->x - 1000) + abs(tmp->cell->y -1000);
	calcul2 = abs(tmp->cell->x - 8000) + abs(tmp->cell->y - 5000);

	if(calcul1 <= calcul2)
	{
		//sendCommand(wsi, )
	}
	else{
		//sendCommand(wsi, )
	}

}


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
    while(tmp != NULL)
    {
    	if(tmp->cell->nodeID == upcell->nodeID)
    	{
    		free(tmp->cell);
    		tmp->cell = upcell;
    		return;
    	}
    	tmp = tmp->next;
    }
  	tmp=malloc(sizeof(Pile));
    tmp->next = *pile;
    tmp->cell = upcell;
    *pile = tmp;

}

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
		if(tmp->cell->nodeID == removecellsID)
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


void update(unsigned char *paquet)
{
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

			insertion(&chaine, node);

		}
		i++;
		unsigned short removecellslength;
		memcpy(&removecellslength, paquet+i, sizeof(unsigned short) );
		unsigned int removecellsID;
		i+=2;
		for(int j = 0; j < removecellslength; j++)
		{
			memcpy(&removecellsID, paquet+i, 2);
			supressID(&chaine, removecellsID);
			i+=4;		

		}
		printNodeStack(chaine);
}


int recv_packet(unsigned char *paquet, struct lws *wsi)
{
	/* Si le paquet commence par 0x40 (ID = 64)
		alors utiliser memcopy pour conserver les coordonnées de la map (si c'est le premier paquet)
		ou alors récupérer les coordonnées correspondants au rectangle de vision du chien

	Si l'ID du paquet est 0x20
		alors récupérer l'ID du chien pour le lier avec la race du chien que l'on vient de faire apparaitre

	Si l'ID du paquet est 0x10
		alors retenir les informations concernant notre chien
	*/


	switch (paquet[0])
	{
		case 0x10 :
			update(paquet);
			break;

		case 0x40 :
			break;

		default :
			break;

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

		//unsigned char buf[1] = {1};
		//sendCommand(wsi, buf, 1);

		unsigned char t[5] = {0};
		t[1] = 't'; t[2] = 'r'; t[3] = 'y';
		sendCommand(wsi, t, 5);

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
				for(int i = 0; i < offset; i++)
					printf(" 0x%x", rbuf[i]);
				printf("\n---------------------------------------\n");
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
	struct lws_context_creation_info info;
	struct lws_client_connect_info i;

	struct lws_context *context;
	const char *protocol,*temp;

	memset(&info, 0, sizeof info);
	memset(&i, 0, sizeof(i));

	signal(SIGINT, sighandler);

	i.origin = "agar.io";
	i.port = 2008;

	if (lws_parse_uri("192.168.130.150", &protocol, &i.address, &i.port, &temp))
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
