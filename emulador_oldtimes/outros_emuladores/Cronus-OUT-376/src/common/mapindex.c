#include "mmo.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include "showmsg.h"

#define MAX_MAPINDEX 2000

//Leave an extra char of space to hold the terminator, in case for the strncpy(mapindex_id2name()) calls.
struct {
	char name[MAP_NAME_LENGTH+1]; //Stores map name
	int length; //Stores string length WITHOUT the extension for quick lookup.
} indexes[MAX_MAPINDEX];

static unsigned short max_index = 0;

char mapindex_cfgfile[80] = "db/map_index.txt";

unsigned short mapindex_name2id(char* name) {
	//TODO: Perhaps use a db to speed this up? [Skotlex]
	int i;
	int length = strlen(name);
	char *ext = strstr(name, ".");
	if (ext)
		length = ext-name; //Base map-name length without the extension.
	for (i = 1; i < max_index; i++)
	{
		if (indexes[i].length == length && strncmp(indexes[i].name,name,length)==0)
			return i;
	}
#ifdef MAPINDEX_AUTOADD
	if (i < MAX_MAPINDEX) {
		char map_name[MAP_NAME_LENGTH+5];
		length = strlen(name);
		if (length > MAP_NAME_LENGTH)
			return;
		memcpy(map_name, name, length+1);
		if ((ext = strstr(map_name, ".")) != NULL) {
			length = ext-map_name;
			sprintf(ext, ".gat");
		} else { //No extension?
			length = strlen(map_name);
			strcat(map_name, ".gat");
		}
		if (length > MAP_NAME_LENGTH - 4)
			return 0; //Can't be added.
		strncpy(indexes[i].name, map_name, MAP_NAME_LENGTH);
		indexes[i].length = strlen(map_name);
		ShowDebug("mapindex_name2id: Added map \"%s\" to position %d\n", indexes[i], i);
		return i;
	}
#endif
	ShowDebug("mapindex_name2id: Map \"%s\" not found in index list!\n", name);
	return 0;
}

char* mapindex_id2name(unsigned short id) {
	if (id > MAX_MAPINDEX || !indexes[id].length) {
		ShowDebug("mapindex_id2name: Requested name for non-existant map index [%d] in cache.\n", id);
		return indexes[0].name; //Theorically this should never happen, hence we return this string to prevent null pointer crashes.
	}
	return indexes[id].name;
}

void mapindex_init(void) {
	FILE *fp;
	char line[1024];
	char *ext;
	int last_index = -1;
	int index, length;
	char map_name[1024];
	
	memset (&indexes, 0, sizeof (indexes));
	fp=fopen(mapindex_cfgfile,"r");
	if(fp==NULL){
		ShowFatalError("Unable to read mapindex config file %s!\n", mapindex_cfgfile);
		exit(1); //Server can't really run without this file.
	}
	while(fgets(line,1020,fp)){
		if(line[0] == '/' && line[1] == '/')
			continue;

		switch (sscanf(line,"%1000s\t%d",map_name,&index)) {
			case 1: //Map with no ID given, auto-assign
				index = last_index+1;
			case 2: //Map with ID given
				if (index < 0 || index >= MAX_MAPINDEX) {
					ShowError("(mapindex_init) Map index (%d) for \"%s\" out of range (max is %d)\n", index, map_name, MAX_MAPINDEX);
					continue;
				}
				length = strlen(map_name);
				if (length > MAP_NAME_LENGTH) {
					ShowError("(mapindex_init) Map name %s is too long. Maps are limited to %d characters.\n", map_name, MAP_NAME_LENGTH);
					continue;
				}
				if ((ext = strstr(map_name, ".gat")) != NULL) { //Gat map
					length = ext-map_name;
				} else if ((ext = strstr(map_name, ".afm")) != NULL || (ext = strstr(map_name, ".af2")) != NULL) { //afm map
					length = ext-map_name;
					sprintf(ext, ".gat"); //Change the extension to gat
				} else if ((ext = strstr(map_name, ".")) != NULL) { //Generic extension?
					length = ext-map_name;
					sprintf(ext, ".gat");
				} else { //No extension?
					length = strlen(map_name);
					strcat(map_name, ".gat");
				}
				if (length > MAP_NAME_LENGTH - 4) {
					ShowError("(mapindex_init) Adjusted Map name %s is too long. Maps are limited to %d characters.\n", map_name, MAP_NAME_LENGTH);
					continue;
				}
						
				if (indexes[index].length)
					ShowWarning("(mapindex_init) Overriding index %d: map \"%s\" -> \"%s\"\n", indexes[index].name, map_name);
				
				strncpy(indexes[index].name, map_name, MAP_NAME_LENGTH);
				indexes[index].length = length;
				if (max_index <= index)
					max_index = index+1;
				break;
			default:
				continue;
		}
		last_index = index;
	}
	fclose(fp);
}

void mapindex_final(void) {
}

