all: yellow normal

yellow: jaune.c
	gcc -std=c99 -Wall -g -o yellow jaune.c -lwebsockets

normal: 
	gnome-terminal --tab-with-profile="sheep" -e "./yellow 127.0.0.1 1443"
	gnome-terminal --tab-with-profile="sheep" -e "./yellow 127.0.0.1 1443"
	gnome-terminal --tab-with-profile="sheep" -e "./yellow 127.0.0.1 1443"	
	gnome-terminal --tab-with-profile="sheep" -e "./yellow 127.0.0.1 1443"

