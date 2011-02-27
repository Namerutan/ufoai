/**
 * @file cl_game_team.h
 * @brief cgame team management headers.
 */

/*
Copyright (C) 2002-2010 UFO: Alien Invasion.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.m

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef CL_GAME_TEAM_H
#define CL_GAME_TEAM_H

void GAME_SaveTeam_f(void);
void GAME_LoadTeam_f(void);
void GAME_TeamSlotComments_f(void);
void GAME_SaveTeamState_f(void);
void GAME_ToggleActorForTeam_f(void);
void GAME_AutoTeam_f(void);
void GAME_AutoTeam(const char *equipmentDefinitionID, int teamMembers);
void GAME_UpdateTeamMenuParameters_f(void);
void GAME_TeamSelect_f(void);
qboolean GAME_LoadDefaultTeam(void);

qboolean CL_SaveCharacterXML(xmlNode_t *p, const character_t *chr);
qboolean CL_LoadCharacterXML(xmlNode_t *p, character_t *chr);

void CL_SaveInventoryXML(xmlNode_t *p, const inventory_t * i);
void CL_LoadInventoryXML(xmlNode_t *p, inventory_t * i);

#endif
