/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
*  _TwyliteMud_ by Rv.                          Based on CircleMud3.0bpl9 *
*    				                                          *
*  OasisOLC - redit.c 		                                          *
*    				                                          *
*  Copyright 1996 Harvey Gilpin.                                          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */
/*. Original author: Levork .*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "structs.h"
#include "comm.h"
#include "interpreter.h"
#include "utils.h"
#include "db.h"
#include "boards.h"
#include "olc.h"

/*------------------------------------------------------------------------*/
/*. External data .*/

extern int      top_of_world;
extern struct room_data *world;
extern struct obj_data *obj_proto;
extern struct char_data *mob_proto;
extern char    *room_bits[];
extern char    *sector_types[];
extern char    *exit_bits[];
extern struct zone_data *zone_table;
extern sh_int r_mortal_start_room;
extern sh_int r_immort_start_room;
extern sh_int r_frozen_start_room;
extern sh_int r_race_start_room[NUM_RACES];
extern sh_int r_lowbie_start_room;
extern sh_int r_clan_start_room[NUM_CLANS];
extern sh_int mortal_start_room;
extern sh_int immort_start_room;
extern sh_int frozen_start_room;
extern sh_int race_start_room[NUM_RACES];
extern sh_int lowbie_start_room;
extern int top_of_zone_table;
extern int rev_dir[];

/*------------------------------------------------------------------------*/
/* function protos */

void redit_disp_extradesc_menu(struct descriptor_data * d);
void redit_disp_exit_menu(struct descriptor_data * d);
void redit_disp_exit_flag_menu(struct descriptor_data * d);
void redit_disp_flag_menu(struct descriptor_data * d);
void redit_disp_sector_menu(struct descriptor_data * d);
void redit_disp_menu(struct descriptor_data * d);
void redit_parse(struct descriptor_data * d, char *arg);
void redit_setup_new(struct descriptor_data *d);
void redit_setup_existing(struct descriptor_data *d, int real_num);
void redit_save_to_disk(struct descriptor_data *d);
void redit_save_internally(struct descriptor_data *d);
void free_room(struct room_data *room);

/*------------------------------------------------------------------------*/

#define  W_EXIT(room, num) (world[(room)].dir_option[(num)])

/*------------------------------------------------------------------------*\
  Utils and exported functions.
\*------------------------------------------------------------------------*/

void redit_setup_new(struct descriptor_data *d)
{ CREATE(OLC_ROOM(d), struct room_data, 1);
  OLC_ROOM(d)->name = strdup("An unfinished room");
  OLC_ROOM(d)->description = strdup("You are in an unfinished room.\r\n");
  redit_disp_menu(d);
  OLC_VAL(d) = 0;
}

/*------------------------------------------------------------------------*/

void redit_setup_existing(struct descriptor_data *d, int real_num)
{ struct room_data *room;
  int counter;
  
  /*. Build a copy of the room .*/
  CREATE (room, struct room_data, 1);
  *room = world[real_num];
  /* allocate space for all strings  */
  if (world[real_num].name)
    room->name = strdup (world[real_num].name);
  if (world[real_num].description)
    room->description = strdup (world[real_num].description);

  /* exits - alloc only if necessary */
  for (counter = 0; counter < NUM_OF_DIRS; counter++)
  { if (world[real_num].dir_option[counter])
    { CREATE(room->dir_option[counter], struct room_direction_data, 1);
      /* copy numbers over */
      *room->dir_option[counter] = *world[real_num].dir_option[counter];
      /* malloc strings */
      if (world[real_num].dir_option[counter]->general_description)
        room->dir_option[counter]->general_description =
          strdup(world[real_num].dir_option[counter]->general_description);
      if (world[real_num].dir_option[counter]->keyword)
        room->dir_option[counter]->keyword =
          strdup(world[real_num].dir_option[counter]->keyword);
    }
  }
  
  /*. Extra descriptions if necessary .*/ 
  if (world[real_num].ex_description) 
  { struct extra_descr_data *this, *temp, *temp2;
    CREATE (temp, struct extra_descr_data, 1);
    room->ex_description = temp;
    for (this = world[real_num].ex_description; this; this = this->next)
    { if (this->keyword)
        temp->keyword = strdup (this->keyword);
      if (this->description)
        temp->description = strdup (this->description);
      if (this->next)
      { CREATE (temp2, struct extra_descr_data, 1);
	temp->next = temp2;
	temp = temp2;
      } else
        temp->next = NULL;
    }
  }
 
  /*. Attatch room copy to players descriptor .*/
  OLC_ROOM(d) = room;
  OLC_VAL(d) = 0;
  redit_disp_menu(d);
}

/*------------------------------------------------------------------------*/
      
#define ZCMD (zone_table[zone].cmd[cmd_no])

void redit_save_internally(struct descriptor_data *d)
{ int i, j, room_num, found = 0, zone, cmd_no;
  struct room_data *new_world;
  struct char_data *temp_ch;
  struct obj_data *temp_obj;
  
  void renum_storage_rooms();
  void renum_restricted_rooms();

  room_num = real_room(OLC_NUM(d));
  if (room_num > 0) 
  { /*. Room exits: move contents over then free and replace it .*/
    OLC_ROOM(d)->contents = world[room_num].contents;
    OLC_ROOM(d)->people = world[room_num].people;
    free_room(world + room_num);
    world[room_num] = *OLC_ROOM(d);
  } else 
  { /*. Room doesn't exist, hafta add it .*/

    CREATE(new_world, struct room_data, top_of_world + 2);

    /* count thru world tables */
    for (i = 0; i <= top_of_world; i++) 
    { if (!found) {
        /*. Is this the place? .*/
        if (world[i].number > OLC_NUM(d)) 
	{ found = 1;

	  new_world[i] = *(OLC_ROOM(d));
	  new_world[i].number = OLC_NUM(d);
	  new_world[i].func = NULL;
          room_num  = i;
	
	  /* copy from world to new_world + 1 */
          new_world[i + 1] = world[i];
          /* people in this room must have their numbers moved */
	  for (temp_ch = world[i].people; temp_ch; temp_ch = temp_ch->next_in_room)
	    if (temp_ch->in_room != -1)
	      temp_ch->in_room = i + 1;

	  /* move objects */
	  for (temp_obj = world[i].contents; temp_obj; temp_obj = temp_obj->next_content)
	    if (temp_obj->in_room != -1)
	      temp_obj->in_room = i + 1;
	  
        } else 
        { /*.   Not yet placed, copy straight over .*/
	  new_world[i] = world[i];
        }
      } else 
      { /*. Already been found  .*/
 
        /* people in this room must have their in_rooms moved */
        for (temp_ch = world[i].people; temp_ch; temp_ch = temp_ch->next_in_room)
	  if (temp_ch->in_room != -1)
	    temp_ch->in_room = i + 1;

        /* move objects */
        for (temp_obj = world[i].contents; temp_obj; temp_obj = temp_obj->next_content)
  	  if (temp_obj->in_room != -1)
	    temp_obj->in_room = i + 1;

        new_world[i + 1] = world[i];
      }
    }
    if (!found)
    { /*. Still not found, insert at top of table .*/
      new_world[i] = *(OLC_ROOM(d));
      new_world[i].number = OLC_NUM(d);
      new_world[i].func = NULL;
      room_num  = i;
    }

    /* copy world table over */
    free(world);
    world = new_world;
    top_of_world++;

    /*. Update zone table .*/
    for (zone = 0; zone <= top_of_zone_table; zone++)
      for (cmd_no = 0; ZCMD.command != 'S'; cmd_no++)
        switch (ZCMD.command)
        {
          case 'M':
          case 'O':
            if (ZCMD.arg3 >= room_num)
              ZCMD.arg3++;
 	    break;
          case 'D':
          case 'R':
            if (ZCMD.arg1 >= room_num)
              ZCMD.arg1++;
          case 'G':
          case 'P':
          case 'E':
          case '*':
            break;
          default:
            mudlog("SYSERR: OLC: redit_save_internally: Unknown comand", BRF, LVL_BUILDER, TRUE);
        }

    /* update load rooms, to fix creeping load room problem */
    if (room_num <= r_mortal_start_room)
      r_mortal_start_room++;
    if (room_num <= r_immort_start_room)
      r_immort_start_room++;
    if (room_num <= r_frozen_start_room)
      r_frozen_start_room++;
    for (i = 0; i < NUM_RACES; i++)
      if (room_num <= r_race_start_room[i])
        r_race_start_room[i]++;
    if (room_num <= r_lowbie_start_room)
      r_lowbie_start_room++;
    for (i = 0; i < NUM_CLANS; i++)
      if (room_num <= r_clan_start_room[i])
        r_clan_start_room[i]++;

    /*. Update world exits .*/
    for (i = 0; i < top_of_world + 1; i++)
      for (j = 0; j < NUM_OF_DIRS; j++)
        if (W_EXIT(i, j))
          if (W_EXIT(i, j)->to_room >= room_num)
	    W_EXIT(i, j)->to_room++;

  }
  olc_add_to_save_list(zone_table[OLC_ZNUM(d)].number, OLC_SAVE_ROOM);
  /* move storage and restricted rooms */
  renum_storage_rooms();
  renum_restricted_rooms();
}


/*------------------------------------------------------------------------*/

void redit_save_to_disk(struct descriptor_data *d)
{
  int counter, counter2, realcounter;
  FILE *fp;
  struct room_data *room;
  struct extra_descr_data *ex_desc;

  sprintf(buf, "%s/%.3d.wld", WLD_PREFIX, zone_table[OLC_ZNUM(d)].number);
  if (!(fp = fopen(buf, "w+")))
  { mudlog("SYSERR: OLC: Cannot open room file!", BRF, LVL_BUILDER, TRUE);
    return;
  }

  for (counter = zone_table[OLC_ZNUM(d)].number * 100;
       counter <= zone_table[OLC_ZNUM(d)].top;
       counter++) 
  { realcounter = real_room(counter);
    if (realcounter >= 0) 
    { 
      room = (world + realcounter);

      /*. Remove the '\r\n' sequences from description .*/
      strcpy(buf1, room->description ? room->description : "Empty");
      strip_string(buf1);

      /*. Build a buffer ready to save .*/
      sprintf(buf, "#%d\n%s~\n%s~\n%d %d %d\n",
      		counter, room->name ? room->name : "undefined", buf1,
      		zone_table[room->zone].number,
      		room->room_flags, room->sector_type
      );
      /*. Save this section .*/
      fputs(buf, fp);

      /*. Handle exits .*/
      for (counter2 = 0; counter2 < NUM_OF_DIRS; counter2++) 
      { if (room->dir_option[counter2]) 
        { int temp_door_flag;

          /*. Again, strip out the crap .*/
          if (room->dir_option[counter2]->general_description)
          { strcpy(buf1, room->dir_option[counter2]->general_description);
            strip_string(buf1);
          } else
          *buf1 = 0;

          /*. Figure out door flag .*/
          if (IS_SET(room->dir_option[counter2]->exit_info, EX_ISDOOR)) 
          { if (IS_SET(room->dir_option[counter2]->exit_info, EX_PICKPROOF))
	      temp_door_flag = 2;
	    else
	      temp_door_flag = 1;
	  } else 
	    if (IS_SET(room->dir_option[counter2]->exit_info, EX_BREAKABLE))
	      temp_door_flag = 5;  /* Don't even ask */
	    else
	      temp_door_flag = 0;

          /*. Check for keywords .*/
          if(room->dir_option[counter2]->keyword)
            strcpy(buf2, room->dir_option[counter2]->keyword);
          else
            *buf2 = 0;
               
          /*. Ok, now build a buffer to output to file .*/
          sprintf(buf, "D%d\n%s~\n%s~\n%d %d %d\n",
                        counter2, buf1, buf2, temp_door_flag,
			room->dir_option[counter2]->key,
			world[room->dir_option[counter2]->to_room].number
          );
          /*. Save this door .*/
	  fputs(buf, fp);
        }
      }
      if (room->ex_description) 
      { for (ex_desc = room->ex_description; ex_desc; ex_desc = ex_desc->next) 
        { /*. Home straight, just deal with extras descriptions..*/
          strcpy(buf1, ex_desc->description);
          strip_string(buf1);
          sprintf(buf, "E\n%s~\n%s~\n", ex_desc->keyword,buf1);
          fputs(buf, fp);
	}
      }
      fprintf(fp, "S\n");
    }
  }
  /* write final line and close */
  fprintf(fp, "#99999\n$~\n");
  fclose(fp);
  olc_remove_from_save_list(zone_table[OLC_ZNUM(d)].number, OLC_SAVE_ROOM);
}

/*------------------------------------------------------------------------*/

void free_room(struct room_data *room)
{ int i;
  struct extra_descr_data *this, *next;

  if (room->name)
    free(room->name);
  if (room->description)
    free(room->description);

  /*. Free exits .*/
  for (i = 0; i < NUM_OF_DIRS; i++)
  { if (room->dir_option[i])
    { if (room->dir_option[i]->general_description)
        free(room->dir_option[i]->general_description);
      if (room->dir_option[i]->keyword)
        free(room->dir_option[i]->keyword);
    }
    free(room->dir_option[i]);
  }

  /*. Free extra descriptions .*/
  for (this = room->ex_description; this; this = next)
  { next = this->next;
    if (this->keyword)
      free(this->keyword);
    if (this->description)
      free(this->description);
    free(this);
  }
}

/**************************************************************************
 Menu functions 
 **************************************************************************/

/* For extra descriptions */
void redit_disp_extradesc_menu(struct descriptor_data * d)
{
  struct extra_descr_data *extra_desc = OLC_DESC(d);
  
  sprintf(buf, "[H[J"
  	"%s1%s) Keyword: %s%s\r\n"
  	"%s2%s) Description:\r\n%s%s\r\n"
        "%s3%s) Goto next description: ",

	grn, nrm, yel, extra_desc->keyword ? extra_desc->keyword : "<NONE>",
	grn, nrm, yel, extra_desc->description ?  extra_desc->description : "<NONE>",
        grn, nrm
  );
  
  if (!extra_desc->next)
    strcat(buf, "<NOT SET>\r\n");
  else
    strcat(buf, "Set.\r\n");
  strcat(buf, "Enter choice (0 to quit) : ");
  send_to_char(buf, d->character);
  OLC_MODE(d) = REDIT_EXTRADESC_MENU;
}

/* For exits */
void redit_disp_exit_menu(struct descriptor_data * d)
{
  /* if exit doesn't exist, alloc/create it */
  if(!OLC_EXIT(d))
    CREATE(OLC_EXIT(d), struct room_direction_data, 1);

  /* weird door handling! */
  if (IS_SET(OLC_EXIT(d)->exit_info, EX_ISDOOR)) {
    if (IS_SET(OLC_EXIT(d)->exit_info, EX_PICKPROOF))
      strcpy(buf2, "Pickproof");
    else
      strcpy(buf2, "Is a door");
  } else {
    if (IS_SET(OLC_EXIT(d)->exit_info, EX_BREAKABLE)) {
      strcpy(buf2, "Breakable floor");
    } else {
      strcpy(buf2, "No door");
    }
  }

  get_char_cols(d->character);
  sprintf(buf, "[H[J"
	"%s1%s) Exit to     : %s%d\r\n"
	"%s2%s) Description :-\r\n%s%s\r\n"
  	"%s3%s) Door name   : %s%s\r\n"
  	"%s4%s) Key         : %s%d\r\n"
  	"%s5%s) Door flags  : %s%s\r\n"
  	"%s6%s) Purge exit.\r\n"
	"Enter choice, 0 to quit : ",

	grn, nrm, cyn, world[OLC_EXIT(d)->to_room].number,
        grn, nrm, yel, OLC_EXIT(d)->general_description ? OLC_EXIT(d)->general_description : "<NONE>",
	grn, nrm, yel, OLC_EXIT(d)->keyword ? OLC_EXIT(d)->keyword : "<NONE>",
	grn, nrm, cyn, OLC_EXIT(d)->key,
	grn, nrm, cyn, buf2, grn, nrm
  );

  send_to_char(buf, d->character);
  OLC_MODE(d) = REDIT_EXIT_MENU;
}

/* For exit flags */
void redit_disp_exit_flag_menu(struct descriptor_data * d)
{
  get_char_cols(d->character);
  sprintf(buf,  "%s0%s) No door\r\n"
  		"%s1%s) Closeable door\r\n"
		"%s2%s) Pickproof\r\n"
		"%s3%s) Breakable floor\r\n"
  		"Enter choice : ",
		grn, nrm, grn, nrm, grn, nrm, grn, nrm
  );
  send_to_char(buf, d->character);
}

/* For room flags */
void redit_disp_flag_menu(struct descriptor_data * d)
{ int counter, columns = 0;

  get_char_cols(d->character);
  send_to_char("[H[J", d->character);
  for (counter = 0; counter < NUM_ROOM_FLAGS; counter++) 
  { sprintf(buf, "%s%2d%s) %-20.20s ",
	    grn, counter + 1, nrm, room_bits[counter]);
    if(!(++columns % 2))
      strcat(buf, "\r\n");
    send_to_char(buf, d->character);
  }
  sprintbit(OLC_ROOM(d)->room_flags, room_bits, buf1);
  sprintf(buf, 
	"\r\nRoom flags: %s%s%s\r\n"
  	"Enter room flags, 0 to quit : ",
	cyn, buf1, nrm
  );
  send_to_char(buf, d->character);
  OLC_MODE(d) = REDIT_FLAGS;
}

/* for sector type */
void redit_disp_sector_menu(struct descriptor_data * d)
{ int counter, columns = 0;

  send_to_char("[H[J", d->character);
  for (counter = 0; counter < NUM_ROOM_SECTORS; counter++) {
    sprintf(buf, "%s%2d%s) %-20.20s ",
	    grn, counter, nrm, sector_types[counter]);
    if(!(++columns % 2))
      strcat(buf, "\r\n");
    send_to_char(buf, d->character);
  }
  send_to_char("\r\nEnter sector type : ", d->character);
  OLC_MODE(d) = REDIT_SECTOR;
}

/* the main menu */
void redit_disp_menu(struct descriptor_data * d)
{ struct room_data *room;

  get_char_cols(d->character);
  room = OLC_ROOM(d);

  sprintbit((long) room->room_flags, room_bits, buf1);
  sprinttype(room->sector_type, sector_types, buf2);
  sprintf(buf,
  	"[H[J"
	"-- Room number : [%s%d%s]  	Room zone: [%s%d%s]\r\n"
	"%s1%s) Name           : %s%s\r\n"
	"%s2%s) Description    :\r\n%s%s"
  	"%s3%s) Room flags     : %s%s\r\n"
	"%s4%s) Sector type    : %s%s\r\n"
  	"%s5%s) Exit north     : %s%-5d            "
  	"%s6%s) Exit east      : %s%-5d\r\n"
  	"%s7%s) Exit south     : %s%-5d            "
  	"%s8%s) Exit west      : %s%-5d\r\n"
  	"%s9%s) Exit up        : %s%-5d            "
  	"%sA%s) Exit down      : %s%-5d\r\n"
        "%sB%s) Exit northeast : %s%-5d            "
        "%sC%s) Exit southeast : %s%-5d\r\n"
        "%sD%s) Exit southwest : %s%-5d            "
        "%sE%s) Exit northwest : %s%-5d\r\n"
  	"%sF%s) Extra descriptions menu\r\n"
  	"%sQ%s) Quit\r\n"
  	"Enter choice : ",

	cyn, OLC_NUM(d), nrm,
	cyn, zone_table[OLC_ZNUM(d)].number, nrm,
	grn, nrm, yel, room->name,
	grn, nrm, yel, room->description,
	grn, nrm, cyn, buf1,
	grn, nrm, cyn, buf2,
  	grn, nrm, cyn, room->dir_option[NORTH] ?
          world[room->dir_option[NORTH]->to_room].number : -1,
	grn, nrm, cyn, room->dir_option[EAST] ?
          world[room->dir_option[EAST]->to_room].number : -1,
  	grn, nrm, cyn, room->dir_option[SOUTH] ? 
          world[room->dir_option[SOUTH]->to_room].number : -1,
  	grn, nrm, cyn, room->dir_option[WEST] ? 
          world[room->dir_option[WEST]->to_room].number : -1,
  	grn, nrm, cyn, room->dir_option[UP] ? 
          world[room->dir_option[UP]->to_room].number : -1,
  	grn, nrm, cyn, room->dir_option[DOWN] ? 
          world[room->dir_option[DOWN]->to_room].number : -1,
        grn, nrm, cyn, room->dir_option[NORTHEAST] ?
          world[room->dir_option[NORTHEAST]->to_room].number : -1,
        grn, nrm, cyn, room->dir_option[SOUTHEAST] ?
          world[room->dir_option[SOUTHEAST]->to_room].number : -1,
        grn, nrm, cyn, room->dir_option[SOUTHWEST] ?
          world[room->dir_option[SOUTHWEST]->to_room].number : -1,
        grn, nrm, cyn, room->dir_option[NORTHWEST] ?
          world[room->dir_option[NORTHWEST]->to_room].number : -1,
        grn, nrm, grn, nrm
  );
  send_to_char(buf, d->character);

  OLC_MODE(d) = REDIT_MAIN_MENU;
}



/* ********************************************************
 * Fast building Fast digging commands                    *
 ******************************************************** */
ACMD(do_link)
{
  int iroom = 0, rroom = 0;
  int dir = 0;
/* struct room_data *room; */


  /* buf2 is the direction, buf3 is the room */
  two_arguments(argument, buf2, buf3); 

  if ((!*buf2) || (!*buf3)) {
    send_to_char("Format: link <dir> <room number>\r\n", ch); 
    return;
  } 

  iroom = atoi(buf3);
  rroom = real_room(iroom);

  if (rroom <= 0) {
    sprintf(buf, "There is no room with the number %d", iroom);
    send_to_char(buf, ch);
    return;
  }

  /* Main stuff */
  switch (*buf2) {
    case 'n':
    case 'N':
        dir = NORTH;
        break;
    case 'e':
    case 'E':
        dir = EAST;
        break;
    case 's':
    case 'S':
        dir = SOUTH;
        break;
    case 'w':
    case 'W':
        dir = WEST;
        break;
    case 'u':
    case 'U':
        dir = UP;
        break;
    case 'd':
    case 'D':
        dir = DOWN;
        break;
    default:
        send_to_char("Unknown direction\r\n", ch);
        return;
        break;
  }

  CREATE(world[rroom].dir_option[rev_dir[dir]], struct room_direction_data, 1); 
  world[rroom].dir_option[rev_dir[dir]]->general_description = NULL;
  world[rroom].dir_option[rev_dir[dir]]->keyword = NULL;
  world[rroom].dir_option[rev_dir[dir]]->to_room = ch->in_room; 

  CREATE(world[ch->in_room].dir_option[dir], struct room_direction_data, 1); 
  world[ch->in_room].dir_option[dir]->general_description = NULL;
  world[ch->in_room].dir_option[dir]->keyword = NULL;
  world[ch->in_room].dir_option[dir]->to_room = rroom; 

  /* Only works if you have Oasis OLC */
/*
  olc_add_to_save_list((iroom/100), OLC_SAVE_ROOM);
*/
  olc_add_to_save_list(zone_table[world[rroom].zone].number, OLC_SAVE_ROOM);
  olc_add_to_save_list(zone_table[world[ch->in_room].zone].number, 
      OLC_SAVE_ROOM);

  sprintf(buf, "You make an exit %s to room %d.\r\n", buf2, iroom);
  send_to_char(buf, ch);
}



/**************************************************************************
  The main loop
 **************************************************************************/

void redit_parse(struct descriptor_data * d, char *arg)
{ 
  /* extern struct room_data *world; */
  int number;

  switch (OLC_MODE(d)) {
  case REDIT_CONFIRM_SAVESTRING:
    switch (*arg) {
    case 'y':
    case 'Y':
      redit_save_internally(d);
      sprintf(buf, "OLC: %s edits room %d", GET_NAME(d->character), OLC_NUM(d));
      mudlog(buf, CMP, LVL_BUILDER, TRUE);
      /*. Do NOT free strings! just the room structure .*/
      cleanup_olc(d, CLEANUP_STRUCTS);
      send_to_char("Room saved to memory.\r\n", d->character);
      break;
    case 'n':
    case 'N':
      /* free everything up, including strings etc */
      cleanup_olc(d, CLEANUP_ALL);
      break;
    default:
      send_to_char("Invalid choice!\r\n", d->character);
      send_to_char("Do you wish to save this room internally? : ", d->character);
      break;
    }
    return;

  case REDIT_MAIN_MENU:
    switch (*arg) {
    case 'q':
    case 'Q':
      if (OLC_VAL(d))
      { /*. Something has been modified .*/
        send_to_char("Do you wish to save this room internally? : ", d->character);
        OLC_MODE(d) = REDIT_CONFIRM_SAVESTRING;
      } else
        cleanup_olc(d, CLEANUP_ALL);
      return;
    case '1':
      send_to_char("Enter room name:-\r\n| ", d->character);
      OLC_MODE(d) = REDIT_NAME;
      break;
    case '2':
      send_to_char("Enter room description:-\r\n", d->character);
      OLC_MODE(d) = REDIT_DESC;
      d->str = (char **) malloc(sizeof(char *));
      *(d->str) = NULL;
      d->max_str = MAX_ROOM_DESC;
      d->mail_to = 0;
      OLC_VAL(d) = 1;
      break;
    case '3':
      redit_disp_flag_menu(d);
      break;
    case '4':
      redit_disp_sector_menu(d);
      break;
    case '5':
      OLC_VAL(d) = NORTH;
      redit_disp_exit_menu(d);
      break;
    case '6':
      OLC_VAL(d) = EAST;
      redit_disp_exit_menu(d);
      break;
    case '7':
      OLC_VAL(d) = SOUTH;
      redit_disp_exit_menu(d);
      break;
    case '8':
      OLC_VAL(d) = WEST;
      redit_disp_exit_menu(d);
      break;
    case '9':
      OLC_VAL(d) = UP;
      redit_disp_exit_menu(d);
      break;
    case 'a':
    case 'A':
      OLC_VAL(d) = DOWN;
      redit_disp_exit_menu(d);
      break;
    case 'b':
    case 'B':
      OLC_VAL(d) = NORTHEAST;
      redit_disp_exit_menu(d);
      break;
    case 'c':
    case 'C':
      OLC_VAL(d) = SOUTHEAST;
      redit_disp_exit_menu(d);
      break;
    case 'd':
    case 'D':
      OLC_VAL(d) = SOUTHWEST;
      redit_disp_exit_menu(d);
      break;
    case 'e':
    case 'E':
      OLC_VAL(d) = NORTHWEST;
      redit_disp_exit_menu(d);
      break;
    case 'f':
    case 'F':
      /* if extra desc doesn't exist . */
      if (!OLC_ROOM(d)->ex_description) {
	CREATE(OLC_ROOM(d)->ex_description, struct extra_descr_data, 1);
	OLC_ROOM(d)->ex_description->next = NULL;
      }
      OLC_DESC(d) = OLC_ROOM(d)->ex_description;
      redit_disp_extradesc_menu(d);
      break;
    default:
      send_to_char("Invalid choice!", d->character);
      redit_disp_menu(d);
      break;
    }
    return;

  case REDIT_NAME:
    if (OLC_ROOM(d)->name)
      free(OLC_ROOM(d)->name);
    if (strlen(arg) > MAX_ROOM_NAME)
      arg[MAX_ROOM_NAME -1] = 0;
    OLC_ROOM(d)->name = strdup(arg);
    break;
  case REDIT_DESC:
    /* we will NEVER get here */
    mudlog("SYSERR: Reached REDIT_DESC case in parse_redit",BRF,LVL_BUILDER,TRUE);
    break;

  case REDIT_FLAGS:
    number = atoi(arg);
    if ((number < 0) || (number > NUM_ROOM_FLAGS)) {
      send_to_char("That's not a valid choice!\r\n", d->character);
      redit_disp_flag_menu(d);
    } else {
      if (number == 0)
        break;
      else {
	/* toggle bits */
	if (IS_SET(OLC_ROOM(d)->room_flags, 1 << (number - 1)))
	  REMOVE_BIT(OLC_ROOM(d)->room_flags, 1 << (number - 1));
	else
	  SET_BIT(OLC_ROOM(d)->room_flags, 1 << (number - 1));
	redit_disp_flag_menu(d);
      }
    }
    return;

  case REDIT_SECTOR:
    number = atoi(arg);
    if (number < 0 || number >= NUM_ROOM_SECTORS) {
      send_to_char("Invalid choice!", d->character);
      redit_disp_sector_menu(d);
      return;
    } else 
      OLC_ROOM(d)->sector_type = number;
    break;

  case REDIT_EXIT_MENU:
    switch (*arg) {
    case '0':
      break;
    case '1':
      OLC_MODE(d) = REDIT_EXIT_NUMBER;
      send_to_char("Exit to room number : ", d->character);
      return;
    case '2':
      OLC_MODE(d) = REDIT_EXIT_DESCRIPTION;
      d->str = (char **) malloc(sizeof(char *));
      *(d->str) = NULL;
      d->max_str = MAX_EXIT_DESC;
      d->mail_to = 0;
      send_to_char("Enter exit description:-\r\n", d->character);
      return;
    case '3':
      OLC_MODE(d) = REDIT_EXIT_KEYWORD;
      send_to_char("Enter keywords : ", d->character);
      return;
    case '4':
      OLC_MODE(d) = REDIT_EXIT_KEY;
      send_to_char("Enter key number : ", d->character);
      return;
    case '5':
      redit_disp_exit_flag_menu(d);
      OLC_MODE(d) = REDIT_EXIT_DOORFLAGS;
      return;
    case '6':
      /* delete exit */
      if (OLC_EXIT(d)->keyword)
	free(OLC_EXIT(d)->keyword);
      if (OLC_EXIT(d)->general_description)
	free(OLC_EXIT(d)->general_description);
      free(OLC_EXIT(d));
      OLC_EXIT(d) = NULL;
      break;
    default:
      send_to_char("Try again : ", d->character);
      return;
    }
    break;

  case REDIT_EXIT_NUMBER:
    number = (atoi(arg));
    if (number != -1)
    { number = real_room(number);
      if (number < 0)
      { send_to_char("That room does not exist, try again : ", d->character);
        return;
      }
    }
    OLC_EXIT(d)->to_room = number;
    redit_disp_exit_menu(d);
    return;

  case REDIT_EXIT_DESCRIPTION:
    /* we should NEVER get here */
    mudlog("SYSERR: Reached REDIT_EXIT_DESC case in parse_redit",BRF,LVL_BUILDER,TRUE);
    break;

  case REDIT_EXIT_KEYWORD:
    if (OLC_EXIT(d)->keyword)
      free(OLC_EXIT(d)->keyword);
    OLC_EXIT(d)->keyword = strdup(arg);
    redit_disp_exit_menu(d);
    return;

  case REDIT_EXIT_KEY:
    number = atoi(arg);
    OLC_EXIT(d)->key = number;
    redit_disp_exit_menu(d);
    return;

  case REDIT_EXIT_DOORFLAGS:
    number = atoi(arg);
    if ((number < 0) || (number > 3)) {
      send_to_char("That's not a valid choice!\r\n", d->character);
      redit_disp_exit_flag_menu(d);
    } else {
      /* doors are a bit idiotic, don't you think? :) */
      if (number == 0)
	OLC_EXIT(d)->exit_info = 0;
      else if (number == 1)
	OLC_EXIT(d)->exit_info = EX_ISDOOR;
      else if (number == 2)
	OLC_EXIT(d)->exit_info = EX_ISDOOR | EX_PICKPROOF;
      else if (number == 3)
        OLC_EXIT(d)->exit_info = EX_BREAKABLE;
      /* jump out to menu */
      redit_disp_exit_menu(d);
    }
    return;

  case REDIT_EXTRADESC_KEY:
    OLC_DESC(d)->keyword = strdup(arg);
    redit_disp_extradesc_menu(d);
    return;

  case REDIT_EXTRADESC_MENU:
    number = atoi(arg);
    switch (number) {
    case 0:
      {
	/* if something got left out, delete the extra desc
	 when backing out to menu */
	if (!OLC_DESC(d)->keyword || !OLC_DESC(d)->description) 
        { struct extra_descr_data **tmp_desc;

	  if (OLC_DESC(d)->keyword)
	    free(OLC_DESC(d)->keyword);
	  if (OLC_DESC(d)->description)
	    free(OLC_DESC(d)->description);

	  /*. Clean up pointers .*/
	  for(tmp_desc = &(OLC_ROOM(d)->ex_description); *tmp_desc;
	      tmp_desc = &((*tmp_desc)->next))
          { if(*tmp_desc == OLC_DESC(d))
	    { *tmp_desc = NULL;
              break;
	    }
	  }
	  free(OLC_DESC(d));
	}
      }
      break;
    case 1:
      OLC_MODE(d) = REDIT_EXTRADESC_KEY;
      send_to_char("Enter keywords, separated by spaces : ", d->character);
      return;
    case 2:
      OLC_MODE(d) = REDIT_EXTRADESC_DESCRIPTION;
      send_to_char("Enter extra description:-\r\n", d->character);
      /* send out to modify.c */
      d->str = (char **) malloc(sizeof(char *));
      *(d->str) = NULL;
      d->max_str = MAX_EXTRA_DESC;
      d->mail_to = 0;
      return;
    case 3:
      if (!OLC_DESC(d)->keyword || !OLC_DESC(d)->description) {
	send_to_char("You can't edit the next extra desc without completing this one.\r\n", d->character);
	redit_disp_extradesc_menu(d);
      } else {
	struct extra_descr_data *new_extra;

	if (OLC_DESC(d)->next)
	  OLC_DESC(d) = OLC_DESC(d)->next;
	else {
	  /* make new extra, attach at end */
	  CREATE(new_extra, struct extra_descr_data, 1);
	  OLC_DESC(d)->next = new_extra;
	  OLC_DESC(d) = new_extra;
	}
	redit_disp_extradesc_menu(d);
      }
      return;
    }
    break;

  default:
    /* we should never get here */
    mudlog("SYSERR: Reached default case in parse_redit",BRF,LVL_BUILDER,TRUE);
    break;
  }
  /*. If we get this far, something has be changed .*/
  OLC_VAL(d) = 1;
  redit_disp_menu(d);
}

