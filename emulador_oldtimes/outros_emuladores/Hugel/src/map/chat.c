// Copyright (c) Athena Dev Teams - Licensed under GNU GPL
// For more information, see LICENCE in the main folder

#include "../common/cbasetypes.h"
#include "../common/malloc.h"
#include "../common/nullpo.h"
#include "../common/strlib.h"
#include "atcommand.h" // msg_txt()
#include "battle.h" // struct battle_config
#include "clif.h"
#include "map.h"
#include "npc.h" // npc_event_do()
#include "pc.h"
#include "chat.h"

#include <stdio.h>
#include <string.h>


int chat_triggerevent(struct chat_data *cd); // forward declaration

/// Initializes a chatroom object (common functionality for both pc and npc chatrooms).
/// Returns a chatroom object on success, or NULL on failure.
static struct chat_data* chat_createchat(struct block_list* bl, const char* title, const char* pass, int limit, bool pub, int trigger, const char* ev)
{
	struct chat_data* cd;
	nullpo_retr(NULL, bl);

	cd = (struct chat_data *) aMalloc(sizeof(struct chat_data));

	safestrncpy(cd->title, title, sizeof(cd->title));
	safestrncpy(cd->pass, pass, sizeof(cd->pass));
	cd->pub = pub;
	cd->users = 0;
	cd->limit = limit;
	cd->trigger = trigger;
	memset(cd->usersd, 0, sizeof(cd->usersd));
	cd->owner = bl;
	safestrncpy(cd->npc_event, ev, sizeof(cd->npc_event));

	cd->bl.m    = bl->m;
	cd->bl.x    = bl->x;
	cd->bl.y    = bl->y;
	cd->bl.type = BL_CHAT;
	cd->bl.next = cd->bl.prev = NULL;
	cd->bl.id   = map_addobject(&cd->bl);

	if( cd->bl.id == 0 ) {
		aFree(cd);
		cd = NULL;
	}

	return cd;
}

/*==========================================
 * player chatroom creation
 *------------------------------------------*/
int chat_createpcchat(struct map_session_data* sd, const char* title, const char* pass, int limit, bool pub)
{
	struct chat_data* cd;
	nullpo_retr(0, sd);

	if (sd->chatID)
		return 0; //Prevent people abusing the chat system by creating multiple chats, as pointed out by End of Exam. [Skotlex]

	if (map[sd->bl.m].flag.nochat) {
		clif_displaymessage (sd->fd, msg_txt(281));
		return 0; //Can't create chatrooms on this map.
	}

	if ((battle_config.block_area) && ((sd->bl.m == map_mapname2mapid("prontera.gat") && (sd->bl.x >= 148 && sd->bl.x <= 163 && sd->bl.y >= 130 && sd->bl.y <= 232)) ||
	    (sd->bl.m == map_mapname2mapid("prontera.gat") && (sd->bl.x >= 123 && sd->bl.x <= 196 && sd->bl.y >= 182 && sd->bl.y <= 238)))) {
		 clif_displaymessage (sd->fd,"Vocκ nγo estα autorizado a abrir chats ou criar vendas nesta αrea.");
		 return 0; // can't create chatrooms on prontera square [by  theultramage & JuliosS]
	}
	pc_stop_walking(sd,1);

	cd = chat_createchat(&sd->bl, title, pass, limit, pub, 0, "");
	if (cd) {
		cd->users = 1;
		cd->usersd[0] = sd;
		pc_setchatid(sd,cd->bl.id);
		clif_createchat(sd,0);
		clif_dispchat(cd,0);
	} else
		clif_createchat(sd,1);

	return 0;
}

/*==========================================
 * join an existing chatroom
 *------------------------------------------*/
int chat_joinchat(struct map_session_data* sd, int chatid, const char* pass)
{
	struct chat_data* cd;

	nullpo_retr(0, sd);
	cd = (struct chat_data*)map_id2bl(chatid);

	if (cd == NULL || cd->bl.type != BL_CHAT || cd->bl.m != sd->bl.m || sd->vender_id || sd->chatID || cd->users >= cd->limit) {
		clif_joinchatfail(sd,0);
		return 0;
	}
	if (!cd->pub && strncmp(pass, cd->pass, sizeof(cd->pass)) != 0 && !(battle_config.gm_join_chat && pc_isGM(sd) >= battle_config.gm_join_chat)) {
		clif_joinchatfail(sd,1);
		return 0;
	}

	pc_stop_walking(sd,1);
	cd->usersd[cd->users] = sd;
	cd->users++;

	pc_setchatid(sd,cd->bl.id);

	clif_joinchatok(sd,cd);	// V½ΙQΑ΅½lΙΝSυΜXg
	clif_addchat(cd,sd);	// ωΙΙ½lΙΝΗΑ΅½lΜρ
	clif_dispchat(cd,0);	// όΝΜlΙΝlΟ»ρ

	chat_triggerevent(cd); // Cxg
	
	return 0;
}


/*==========================================
 * leave a chatroom
 *------------------------------------------*/
int chat_leavechat(struct map_session_data* sd)
{
	struct chat_data* cd;
	int i;
	int leavechar;

	nullpo_retr(1, sd);

	cd = (struct chat_data*)map_id2bl(sd->chatID);
	if( cd == NULL )
	{
		pc_setchatid(sd, 0);
		return 1;
	}

	ARR_FIND( 0, cd->users, i, cd->usersd[i] == sd );
	if ( i == cd->users )
	{// Not found in the chatroom?
		pc_setchatid(sd, 0);
		return -1;
	}

	leavechar = i;

	clif_leavechat(cd, sd);

	for( i = leavechar; i < cd->users; i++ )
		cd->usersd[i] = cd->usersd[i + 1];

	pc_setchatid(sd, 0);
	cd->users--;

	if( cd->users == 0 && cd->owner->type == BL_PC )
	{// Delete empty chatroom
		clif_clearchat(cd, 0);
		map_delobject(cd->bl.id);
		return 1;
	}

	if( leavechar == 0 && cd->owner->type == BL_PC )
	{	// Set and announce new owner
		cd->owner = (struct block_list*) cd->usersd[0];
		clif_changechatowner(cd, cd->usersd[0]);
		clif_clearchat(cd, 0);

		//Adjust Chat location after owner has been changed.
		map_delblock( &cd->bl );
		cd->bl.x=cd->usersd[0]->bl.x;
		cd->bl.y=cd->usersd[0]->bl.y;
		map_addblock( &cd->bl );

		clif_dispchat(cd,0);
	}
	else
		clif_dispchat(cd,0); // refresh chatroom

	return 0;
}

/*==========================================
 * change a chatroom's owner
 *------------------------------------------*/
int chat_changechatowner(struct map_session_data* sd, const char* nextownername)
{
	struct chat_data* cd;
	struct map_session_data* tmpsd;
	int i, nextowner;

	nullpo_retr(1, sd);

	cd = (struct chat_data*)map_id2bl(sd->chatID);
	if (cd == NULL || (struct block_list*) sd != cd->owner)
		return 1;

	ARR_FIND( 1, cd->users, i, strncmp(cd->usersd[i]->status.name, nextownername, NAME_LENGTH) == 0 );
	if (i == cd->users)
		return -1;  // name not found

	// erase temporarily
	clif_clearchat(cd,0);

	// set new owner
	nextowner = i;
	cd->owner = (struct block_list*) cd->usersd[nextowner];
	clif_changechatowner(cd,cd->usersd[nextowner]);

	// change the owner's position (placing him to 0)
	tmpsd = cd->usersd[nextowner];
	cd->usersd[nextowner] = cd->usersd[0];
	cd->usersd[0] = tmpsd;

	// set the new chatroom position
	map_delblock( &cd->bl );
	cd->bl.x = cd->owner->x;
	cd->bl.y = cd->owner->y;
	map_addblock( &cd->bl );

	// and display again
	clif_dispchat(cd,0);

	return 0;
}

/*==========================================
 * change a chatroom's status (title, etc)
 *------------------------------------------*/
int chat_changechatstatus(struct map_session_data* sd, const char* title, const char* pass, int limit, bool pub)
{
	struct chat_data* cd;

	nullpo_retr(1, sd);

	cd=(struct chat_data*)map_id2bl(sd->chatID);
	if(cd==NULL || (struct block_list *)sd != cd->owner)
		return 1;

	safestrncpy(cd->title, title, CHATROOM_TITLE_SIZE);
	safestrncpy(cd->pass, pass, CHATROOM_PASS_SIZE);
	cd->limit = limit;
	cd->pub = pub;

	clif_changechatstatus(cd);
	clif_dispchat(cd,0);

	return 0;
}

/*==========================================
 * kick an user from a chatroom
 *------------------------------------------*/
int chat_kickchat(struct map_session_data* sd, const char* kickusername)
{
	struct chat_data* cd;
	int i;

	nullpo_retr(1, sd);

	cd = (struct chat_data *)map_id2bl(sd->chatID);
	
	if (!cd)
		return -1;

	ARR_FIND( 0, cd->users, i, strncmp(cd->usersd[i]->status.name, kickusername, NAME_LENGTH) == 0 );
	if (i == cd->users)
		return -1;

	if (battle_config.gm_kick_chat && pc_isGM(cd->usersd[i]) >= battle_config.gm_kick_chat)
		return 0; //gm kick protection [Valaris]

	chat_leavechat(cd->usersd[i]);
	return 0;
}

/// Creates a chat room for the npc.
int chat_createnpcchat(struct npc_data* nd, const char* title, int limit, bool pub, int trigger, const char* ev)
{
	struct chat_data* cd;
	nullpo_retr(0, nd);

	cd = chat_createchat(&nd->bl, title, "", limit, pub, trigger, ev);
	if (cd)
	{
		nd->chat_id = cd->bl.id;
		clif_dispchat(cd,0);
	}

	return 0;
}

/// Removes the chatroom from the npc.
int chat_deletenpcchat(struct npc_data* nd)
{
	struct chat_data *cd;

	nullpo_retr(0, nd);
	nullpo_retr(0, cd=(struct chat_data*)map_id2bl(nd->chat_id));
	
	chat_npckickall(cd);
	clif_clearchat(cd, 0);
	map_delobject(cd->bl.id);	// freeάΕ΅Δ­κι
	nd->chat_id = 0;
	
	return 0;
}

/*==========================================
 * KθlΘγΕCxgͺθ`³κΔιΘηΐs
 *------------------------------------------*/
int chat_triggerevent(struct chat_data *cd)
{
	nullpo_retr(0, cd);

	if( cd->users >= cd->trigger && cd->npc_event[0] )
		npc_event_do(cd->npc_event);
	return 0;
}

/// Enables the event of the chat room.
/// At most, 127 users are needed to trigger the event.
int chat_enableevent(struct chat_data* cd)
{
	nullpo_retr(0, cd);

	cd->trigger &= 0x7f;
	chat_triggerevent(cd);
	return 0;
}

/// Disables the event of the chat room
int chat_disableevent(struct chat_data* cd)
{
	nullpo_retr(0, cd);

	cd->trigger |= 0x80;
	return 0;
}

/// Kicks all the users for the chat room.
int chat_npckickall(struct chat_data* cd)
{
	nullpo_retr(0, cd);

	while( cd->users > 0 )
		chat_leavechat(cd->usersd[cd->users-1]);

	return 0;
}
