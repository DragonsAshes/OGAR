#include "websockets.h"
#include <signal.h>
#include <math.h>

int sendCommand(struct lws *wsi,unsigned char *buf,unsigned int len);
int monID;
int spawn_id;


typedef enum DOG
{
  INIT,
  WAITING,
  FOLLOWING,
  SEARCHING,
  TRACKING,
} DOG;


DOG yellow = INIT;

typedef struct vecteur{
  int x;
  int y;
}vecteur;

vecteur spawnpoints[4];
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

void swap(int *a, int *b)
{
  int c;
  c = *a;
  *a = *b;
  *b = c;
}

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

int j = 0;

void initialize(struct lws *wsi, Pile* pile)
{
  int calcul1, calcul2, calcul3, calcul4;
  Pile *tmp = pile;
  int old;
  while(tmp != NULL)
  {
  	if( monID == tmp->cell->nodeID )
  		break;
  	tmp = tmp->next;
  }
  if(tmp == NULL)
  	return;

  if(j == 0)
  {
    if(tmp->cell->x <= 4500 && tmp->cell->y <= 3000)
      spawn_id = 0;
    else if(tmp->cell->x > 4500 && tmp->cell->y <= 3000)
      spawn_id = 1;
    else if(tmp->cell->x > 4500 && tmp->cell->y <= 3000)
      spawn_id = 2;
    else if(tmp->cell->x > 4500 && tmp->cell->y > 3000)
      spawn_id = 3;
    j = 1;
  }

  move(wsi, spawnpoints[spawn_id]);

  printf(" Cell->coord : [%d, %d]\n", tmp->cell->x, tmp->cell->y);
  printf("Position : [%d, %d]\n", spawnpoints[spawn_id].x, spawnpoints[spawn_id].y);


  if(tmp->cell->x == spawnpoints[spawn_id].x && tmp->cell->y == spawnpoints[spawn_id].y)
  {
  	yellow = WAITING;
  	printf("\n Le chien est passé en mode WAIT\n");
  }

}


int detect(struct lws *wsi, Pile *chaine)
{
	static Cell old;
	static int compteur = 0;
	Pile *tmp = chaine;
	vecteur actual_pos;
	
	while(tmp != NULL && monID != tmp->cell->nodeID)
		tmp = tmp->next;
	actual_pos.x = tmp->cell->x;
	actual_pos.y = tmp->cell->y;

	tmp = chaine;
	while(tmp != NULL && strcmp(tmp->cell->name, "blue") != 0)
	{
		tmp = tmp->next;
	}
	if(tmp == NULL)
	{
		compteur = 0;
		return 0;
	}

	else
	{
		printf("New cell : [%d, %d]\n", tmp->cell->x, tmp->cell->y);
		printf("Old Cell : [%d, %d]\n", old.x, old.y);
		if(tmp->cell->nodeID == old.nodeID)
		{
			if(tmp->cell->x == actual_pos.x && tmp->cell->y == actual_pos.y)
			{
				compteur++;
				if(compteur == 2)
				{
					compteur = 0;
					return tmp->cell->nodeID;
				}
			}
			else{
				compteur = 0;
			}
		}
		memcpy(&old, tmp->cell, sizeof(Cell));
		return 0;
	}


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

void update(unsigned char *paquet)
{
	supressall(&chaine);
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


int first_dir = 0;
void take_direction(struct lws *wsi, Pile *chaine, int scout_ID)
{
	int distance = 0;
	int hyp = 0;
	int x;
	int y;
	printf("Take direction\n");
	printf("ID : %d\n",scout_ID );
	Pile* tmp = chaine;
	vecteur position;
	
	vecteur actual_pos;
	int delta_x, delta_y;
	while(tmp != NULL && tmp->cell->nodeID != monID)
		tmp = tmp->next;
	actual_pos.x = tmp->cell->x;
	actual_pos.y = tmp->cell->y;
	
	tmp = chaine;

	while(tmp != NULL && tmp->cell->nodeID != scout_ID)
		tmp = tmp->next;

	if(tmp == NULL)
		return;

	if(tmp->cell->x == actual_pos.x && tmp->cell->y == actual_pos.y)
		return;
	printf("delta\n");
	
	
	x = tmp->cell->x;
	y = tmp->cell->y;


	if(x < actual_pos.x-50 || x > actual_pos.x+50 || y < actual_pos.y-50 || y > actual_pos.y+50)
	{
		delta_x = tmp->cell->x - actual_pos.x;
		delta_y = tmp->cell->y - actual_pos.y;

		printf("%d ///// %d \n", delta_x, delta_y);

		
		direction.x = delta_x;
		direction.y = delta_y;
		position.x = actual_pos.x + delta_x;
		position.y = actual_pos.y + delta_y;
		move(wsi, position);
		printf("%d ///// %d \n", position.x, position.y);
		yellow = SEARCHING;	
	}
	
	//return(direction);
}


int mouton_here(struct lws *wsi, Pile *chaine)
{
	Pile *tmp = chaine;
	vecteur position;
	while(tmp->cell->nodeID != monID)
		tmp = tmp->next;

	position.x = tmp->cell->x + direction.x;
	position.y = tmp->cell->y + direction.y;
	printf("direction : %d %d \n", direction.x, direction.y);

	tmp = chaine;



	while(tmp != NULL && strncmp("bot", tmp->cell->name, 3) != 0)
		tmp = tmp->next;

	if(tmp == NULL)
	{
		//position.x += direction.x;
		//position.y += direction.y;
		printf("%d ** Searching mouton null\n",__LINE__);
		move(wsi, position);
		return 0;
	}
	else
	{
		direction.x = tmp->cell->x;
		direction.y = tmp->cell->y;
		move(wsi, direction);
		yellow = TRACKING;
		return 1;
	}
}


void push(struct lws* wsi, Pile *pile)
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
	if ( (position.x < 636) && (position.y > 2364 && position.y < 3636))
	{
		j = 0;
		yellow = INIT;
	}
}

void doublon(struct lws *wsi, Pile *chaine)
{
	Pile* tmp = chaine;
	vecteur destination;

	while(tmp != NULL)
	{
		if( strcmp(tmp->cell->name, "yellow") == 0 && tmp->cell->nodeID != monID)
			break;

		tmp = tmp->next;
	}
	if(tmp == NULL)
		return;

	if(monID < tmp->cell->nodeID)
	{
		destination.x = tmp->cell->x + 100;
		destination.y = tmp->cell->y + 100;
		move(wsi, destination);
		yellow = INIT;
	}
	else
		return;

}





int first = 0;
int first_ID = 0;


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
	vecteur dir;
	static int scout_ID;
	int i = 0;
	switch (paquet[0])
	{
		case 0x10 :
			if(yellow == INIT)
			{
       			printf("Update du paquet : \n");
				update(paquet);
				Pile* tmp = chaine;

		        while(tmp->next != NULL)
		        {
		          tmp = tmp->next;
		        }
		        if(first_ID == 0)
		        {
		        	monID = tmp->cell->nodeID;
		        	first_ID = 1;
				}
				tmp = chaine;
				Pile* pointeur = chaine; 
				while(tmp != NULL)
				{
		        	printf("nodeID: %d // monID : %d\n", tmp->cell->nodeID, monID);
		        	printf("%s\n",tmp->cell->name);
					if(strcmp(tmp->cell->name, "yellow") == 0 && (tmp->cell->nodeID != monID) )
					{
						printf("x : %d / %d // y : %d / %d",tmp->cell->x,spawnpoints[spawn_id].x,tmp->cell->y, spawnpoints[spawn_id].y  );
						if( (tmp->cell->x == spawnpoints[spawn_id].x && tmp->cell->y == spawnpoints[spawn_id].y))
						{
							spawn_id++;
		              		spawn_id = spawn_id % 4;
		              		printf("I = %d", spawn_id);
						}
					}
					tmp = tmp->next;
				}
		      	initialize(wsi, chaine);
			}
			else if (yellow == WAITING)
			{
				update(paquet);
				doublon(wsi, chaine);
				scout_ID = detect(wsi, chaine);
				if(scout_ID)
				{
					printf("Le chien passe en mode FOLLOWING\n");
					yellow = FOLLOWING;
				}

			}
			else if (yellow == FOLLOWING)
			{
				update(paquet);
				printf("entry : %d", scout_ID);
				take_direction(wsi, chaine, scout_ID);
				printf("Cazou\n");

			}
			else if (yellow == SEARCHING)
			{
				printf("Search.......\n");
				update(paquet);
				mouton_here(wsi, chaine);
			}
			else if (yellow == TRACKING)
			{
				printf("\n TRACK!!\n");
				update(paquet);
				push(wsi, chaine);

			}
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

    char* nom = "yellow";
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
	vecteur rdv1; rdv1.x = 3000; rdv1.y = 2000;
	vecteur rdv2; rdv2.x = 6000; rdv2.y = 2000;
	vecteur rdv3; rdv3.x = 3000; rdv3.y = 4000;
	vecteur rdv4; rdv4.x = 6000; rdv4.y = 4000;

	spawnpoints[0] = rdv1;
	spawnpoints[1] = rdv2;
	spawnpoints[2] = rdv3;
	spawnpoints[3] = rdv4;

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
