/**
 * @file bspfile.h
 */

/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#ifndef _BSPFILE
#define _BSPFILE

#include "../../../common/qfiles.h"

void LoadBSPFile(const char *filename);
void WriteBSPFile(const char *filename);
void PrintBSPFileSizes(void);

typedef struct epair_s {
	struct epair_s	*next;
	char	*key;
	char	*value;
} epair_t;

typedef struct {
	vec3_t		origin;
	int			firstbrush;
	int			numbrushes;
	epair_t		*epairs;
} entity_t;

extern int num_entities;
extern entity_t entities[MAX_MAP_ENTITIES];

void ParseEntities(void);
const char *UnparseEntities(void);

void SetKeyValue(entity_t *ent, const char *key, const char *value);
/* will return "" if not present */
const char *ValueForKey(const entity_t *ent, const char *key);

vec_t FloatForKey(entity_t *ent, const char *key);
void GetVectorForKey(entity_t *ent, const char *key, vec3_t vec);
epair_t *ParseEpair(void);
byte *CompressRouting(byte *dataStart, byte *destStart, int l);

#endif /* _BSP_FILE */
