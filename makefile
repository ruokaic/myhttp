all: myhttp

myhttp: myhttp.c
	gcc -W -Wall -o myhttp myhttp.c 
