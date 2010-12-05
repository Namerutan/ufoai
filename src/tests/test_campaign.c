/**
 * @file test_campaign.c
 * @brief Test cases for the campaign code
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

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "CUnit/Basic.h"
#include "test_shared.h"
#include "test_campaign.h"
#include "../client/client.h"
#include "../client/renderer/r_state.h" /* r_state */
#include "../client/ui/ui_main.h"
#include "../client/campaign/cp_campaign.h"
#include "../client/campaign/cp_map.h"
#include "../client/campaign/cp_hospital.h"
#include "../client/campaign/cp_missions.h"
#include "../client/campaign/cp_nation.h"
#include "../client/campaign/cp_overlay.h"
#include "../client/campaign/cp_ufo.h"
#include "../client/campaign/cp_time.h"

static const int TAG_INVENTORY = 1538;

static void FreeInventory (void *data)
{
	Mem_Free(data);
}

static void *AllocInventoryMemory (size_t size)
{
	return Mem_PoolAlloc(size, com_genericPool, TAG_INVENTORY);
}

static void FreeAllInventory (void)
{
	Mem_FreeTag(com_genericPool, TAG_INVENTORY);
}

static const inventoryImport_t inventoryImport = { FreeInventory, FreeAllInventory, AllocInventoryMemory };

static inline void ResetInventoryList (void)
{
	INV_DestroyInventory(&cls.i);
	INV_InitInventory("testCampaign", &cls.i, &csi, &inventoryImport);
}

static campaign_t *GetCampaign (void)
{
	return CL_GetCampaign(cp_campaign->string);
}

static void ResetCampaignData (void)
{
	const campaign_t *campaign;

	CL_ResetSinglePlayerData();
	CL_ReadSinglePlayerData();
	campaign = GetCampaign();
	CL_ReadCampaignData(campaign);

	ResetInventoryList();

	CL_UpdateCredits(MAX_CREDITS);

	MAP_Shutdown();
	MAP_Init(campaign->map);
}

/**
 * The suite initialization function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_InitSuiteCampaign (void)
{
	TEST_Init();

	cl_genericPool = Mem_CreatePool("Client: Generic");
	cp_campaignPool = Mem_CreatePool("Client: Local (per game)");
	cp_campaign = Cvar_Get("cp_campaign", "main", 0, NULL);
	cp_missiontest = Cvar_Get("cp_missiontest", "0", 0, NULL);
	vid_imagePool = Mem_CreatePool("Vid: Image system");

	r_state.active_texunit = &r_state.texunits[0];
	R_FontInit();
	UI_Init();

	memset(&cls, 0, sizeof(cls));
	Com_ParseScripts(qfalse);

	Cmd_AddCommand("msgoptions_set", Cmd_Dummy_f, NULL);

	cl_geoscape_overlay = Cvar_Get("cl_geoscape_overlay", "0", 0, NULL);
	cl_3dmap = Cvar_Get("cl_3dmap", "1", 0, NULL);

	CL_SetClientState(ca_disconnected);
	cls.realtime = Sys_Milliseconds();

	return 0;
}

/**
 * The suite cleanup function.
 * Returns zero on success, non-zero otherwise.
 */
static int UFO_CleanSuiteCampaign (void)
{
	UI_Shutdown();
	CL_ResetSinglePlayerData();
	TEST_Shutdown();
	return 0;
}

static installation_t* CreateInstallation (const char *name, const vec2_t pos)
{
	const installationTemplate_t *installationTemplate = INS_GetInstallationTemplateFromInstallationID("ufoyard");
	installation_t *installation = INS_GetFirstUnfoundedInstallation();

	CU_ASSERT_PTR_NOT_NULL_FATAL(installationTemplate);

	CU_ASSERT_PTR_NOT_NULL_FATAL(installation);

	CU_ASSERT_FALSE(installation->founded);

	INS_SetUpInstallation(installation, installationTemplate, pos, name);
	CU_ASSERT_EQUAL(installation->installationStatus, INSTALLATION_UNDER_CONSTRUCTION);

	/* fake the build time */
	installation->buildStart = ccs.date.day - installation->installationTemplate->buildTime;
	INS_UpdateInstallationData();
	CU_ASSERT_EQUAL(installation->installationStatus, INSTALLATION_WORKING);

	return installation;
}

static base_t* CreateBase (const char *name, const vec2_t pos)
{
	const campaign_t *campaign = GetCampaign();
	base_t *base = B_GetFirstUnfoundedBase();

	CU_ASSERT_PTR_NOT_NULL_FATAL(campaign);
	CU_ASSERT_PTR_NOT_NULL_FATAL(base);

	CU_ASSERT_FALSE(base->founded);

	RS_InitTree(campaign, qfalse);
	B_SetUpBase(campaign, base, pos, name);

	return base;
}

static void testAircraftHandling (void)
{
	const vec2_t destination = { 10, 10 };
	campaign_t *campaign;
	base_t *base;
	aircraft_t *aircraft;
	aircraft_t *newAircraft;
	aircraft_t *aircraftTemplate;
	int firstIdx;
	int initialCount;
	int count;
	int newFound;

	ResetCampaignData();

	campaign = GetCampaign();

	base = CreateBase("unittestaircraft", destination);
	CU_ASSERT_PTR_NOT_NULL_FATAL(base);

	/** @todo we should not assume that initial base has aircraft. It's a campaign parameter */
	aircraft = AIR_GetFirstFromBase(base);
	CU_ASSERT_PTR_NOT_NULL_FATAL(aircraft);

	/* aircraft should have a template */
	aircraftTemplate = aircraft->tpl;
	CU_ASSERT_PTR_NOT_NULL_FATAL(aircraftTemplate);

	firstIdx = aircraft->idx;
	initialCount = AIR_BaseCountAircraft(base);

	/* test deletion (part 1) */
	AIR_DeleteAircraft(aircraft);
	count = AIR_BaseCountAircraft(base);
	CU_ASSERT_EQUAL(count, initialCount - 1);

	/* test addition (part 1) */
	newAircraft = AIR_NewAircraft(base, aircraftTemplate);
	CU_ASSERT_PTR_NOT_NULL_FATAL(newAircraft);
	count = AIR_BaseCountAircraft(base);
	CU_ASSERT_EQUAL(count, initialCount);

	/* new aircraft assigned to the right base */
	CU_ASSERT_EQUAL(newAircraft->homebase, base);

	newFound = 0;
	AIR_Foreach(aircraft) {
		/* test deletion (part 2) */
		CU_ASSERT_NOT_EQUAL(firstIdx, aircraft->idx);
		/* for test addition (part 2) */
		if (aircraft->idx == newAircraft->idx)
			newFound++;
	}
	/* test addition (part 2) */
	CU_ASSERT_EQUAL(newFound, 1);

	/* check if AIR_Foreach iterates through all aircraft */
	AIR_Foreach(aircraft) {
		AIR_DeleteAircraft(aircraft);
	}
	aircraft = AIR_GetFirstFromBase(base);
	CU_ASSERT_PTR_NULL_FATAL(aircraft);
	count = AIR_BaseCountAircraft(base);
	CU_ASSERT_EQUAL(count, 0);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testEmployeeHandling (void)
{
	employeeType_t type;

	ResetCampaignData();

	for (type = 0; type < MAX_EMPL; type++) {
		if (type != EMPL_ROBOT) {
			int cnt;
			employee_t *e = E_CreateEmployee(type, NULL, NULL);
			CU_ASSERT_PTR_NOT_NULL(e);

			cnt = E_CountUnhired(type);
			CU_ASSERT_EQUAL(cnt, 1);

			E_DeleteEmployee(e);

			cnt = E_CountUnhired(type);
			CU_ASSERT_EQUAL(cnt, 0);
		}
	}

	{
		int i;
		const int amount = 3;
		for (i = 0; i < amount; i++) {
			employee_t *e = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
			CU_ASSERT_PTR_NOT_NULL(e);
		}
		{
			employee_t *e;
			int cnt = 0;
			E_Foreach(EMPL_SOLDIER, e) {
				cnt++;
			}

			CU_ASSERT_EQUAL(cnt, amount);

			E_Foreach(EMPL_SOLDIER, e) {
				CU_ASSERT_TRUE(E_DeleteEmployee(e));
			}

			cnt = E_CountUnhired(EMPL_SOLDIER);
			CU_ASSERT_EQUAL(cnt, 0)
		}
	}

	{
		employee_t *e = E_CreateEmployee(EMPL_PILOT, NULL, NULL);
		int cnt;
		CU_ASSERT_PTR_NOT_NULL(e);
		cnt = E_RefreshUnhiredEmployeeGlobalList(EMPL_PILOT, qfalse);
		CU_ASSERT_EQUAL(cnt, 1);
		CU_ASSERT_TRUE(E_DeleteEmployee(e));
	}

	{
		const ugv_t *ugvType = Com_GetUGVByID("ugv_ares_w");
		employee_t *e = E_CreateEmployee(EMPL_ROBOT, NULL, ugvType);
		CU_ASSERT_PTR_NOT_NULL(e);
		CU_ASSERT_TRUE(E_DeleteEmployee(e));
	}

	{
		int i, cnt;
		employee_t *e;

		for (i = 0; i < MAX_EMPLOYEES; i++) {
			e = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
			CU_ASSERT_PTR_NOT_NULL(e);

			cnt = E_CountUnhired(EMPL_SOLDIER);
			CU_ASSERT_EQUAL(cnt, i + 1);
		}
		E_DeleteAllEmployees(NULL);

		cnt = E_CountUnhired(EMPL_SOLDIER);
		CU_ASSERT_EQUAL(cnt, 0);
	}
}

static void testBaseBuilding (void)
{
	vec2_t pos = {0, 0};
	base_t *base;
	campaign_t *campaign;
	employeeType_t type;

	ResetCampaignData();

	campaign = GetCampaign();

	ccs.credits = 10000000;

	base = CreateBase("unittestcreatebase", pos);

	CU_ASSERT_EQUAL(B_GetInstallationLimit(), MAX_INSTALLATIONS_PER_BASE);

	B_Destroy(base);

	for (type = 0; type < MAX_EMPL; type++)
		CU_ASSERT_EQUAL(E_CountHired(base, type), 0);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testAutoMissions (void)
{
	missionResults_t result;
	battleParam_t battleParameters;
	aircraft_t* aircraft = ccs.aircraftTemplates;
	mission_t *mission;
	campaign_t *campaign;
	employee_t *pilot, *e1, *e2;

	ResetCampaignData();

	memset(&result, 0, sizeof(result));
	memset(&battleParameters, 0, sizeof(battleParameters));

	mission = CP_CreateNewMission(INTERESTCATEGORY_RECON, qfalse);
	campaign = GetCampaign();
	CU_ASSERT_TRUE_FATAL(campaign != NULL);

	CU_ASSERT_PTR_NOT_NULL(aircraft);
	CU_ASSERT_PTR_NOT_NULL(mission);

	pilot = E_CreateEmployee(EMPL_PILOT, NULL, NULL);
	CU_ASSERT_PTR_NOT_NULL(pilot);
	AIR_SetPilot(aircraft, pilot);

	e1 = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
	CU_ASSERT_TRUE(AIR_AddToAircraftTeam(aircraft, e1));
	e2 = E_CreateEmployee(EMPL_SOLDIER, NULL, NULL);
	CU_ASSERT_TRUE(AIR_AddToAircraftTeam(aircraft, e2));

	CU_ASSERT_EQUAL(AIR_GetTeamSize(aircraft), 2);

	battleParameters.probability = -1.0;

	CL_GameAutoGo(mission, aircraft, campaign, &battleParameters, &result);

	CU_ASSERT_EQUAL(result.won, battleParameters.probability < result.winProbability);

	CU_ASSERT_TRUE(AIR_SetPilot(aircraft, NULL));
	CU_ASSERT_PTR_NULL(AIR_GetPilot(aircraft));

	CU_ASSERT_TRUE(AIR_RemoveEmployee(e1, aircraft));
	CU_ASSERT_EQUAL(AIR_GetTeamSize(aircraft), 1);

	CU_ASSERT_TRUE(AIR_RemoveEmployee(e2, aircraft));
	CU_ASSERT_EQUAL(AIR_GetTeamSize(aircraft), 0);

	CU_ASSERT_TRUE(E_DeleteEmployee(e2));
	CU_ASSERT_TRUE(E_DeleteEmployee(pilot));
	CU_ASSERT_TRUE(E_DeleteEmployee(e1));

	CU_ASSERT_EQUAL(E_CountUnhired(EMPL_SOLDIER), 0);
	CU_ASSERT_EQUAL(E_CountUnhired(EMPL_PILOT), 0);
}

static void testTransferItem (void)
{
	const vec2_t pos = {0, 0};
	const vec2_t posTarget = {51, 0};
	base_t *base, *targetBase;
	transferData_t td;
	const objDef_t *od;
	transfer_t *transfer;

	ResetCampaignData();

	base = CreateBase("unittesttransferitem", pos);
	CU_ASSERT_PTR_NOT_NULL_FATAL(base);
	/* make sure that we get all buildings in our second base, too.
	 * This is needed for starting a transfer */
	ccs.campaignStats.basesBuilt = 0;
	targetBase = CreateBase("unittesttransferitemtargetbase", posTarget);
	CU_ASSERT_PTR_NOT_NULL_FATAL(targetBase);

	od = INVSH_GetItemByID("assault");
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);

	memset(&td, 0, sizeof(td));
	td.trItemsTmp[od->idx] += 1;
	td.transferBase = targetBase;
	TR_AddData(&td, CARGO_TYPE_ITEM, od);

	transfer = TR_TransferStart(base, &td);
	CU_ASSERT_PTR_NOT_NULL_FATAL(transfer);

	CU_ASSERT_EQUAL(LIST_Count(ccs.transfers), 1);

	/* to ensure that the transfer is finished with the first think call */
	transfer->event = ccs.date;

	TR_TransferRun();
	CU_ASSERT_TRUE(LIST_IsEmpty(ccs.transfers));

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testTransferAircraft (void)
{
	ResetCampaignData();
}

static void testTransferEmployee (void)
{
	ResetCampaignData();

	/** @todo test the transfer of employees and make sure that they can't be used for
	 * anything in the base while they are transfered */
}

static void testUFORecovery (void)
{
	const vec2_t pos = {0, 0};
	const aircraft_t *ufo;
	storedUFO_t *storedUFO;
	installation_t *installation;
	date_t date = ccs.date;

	ResetCampaignData();

	ufo = AIR_GetAircraft("craft_ufo_fighter");
	CU_ASSERT_PTR_NOT_NULL_FATAL(ufo);

	CreateBase("unittestproduction", pos);

	installation = CreateInstallation("unittestuforecovery", pos);

	date.day++;
	storedUFO = US_StoreUFO(ufo, installation, date, 1.0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(storedUFO);
	CU_ASSERT_EQUAL(storedUFO->status, SUFO_RECOVERED);

	UR_ProcessActive();

	CU_ASSERT_EQUAL(storedUFO->status, SUFO_RECOVERED);

	ccs.date.day++;

	UR_ProcessActive();

	CU_ASSERT_EQUAL(storedUFO->status, SUFO_STORED);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);
}

static void testResearch (void)
{
	int n;
	int i;
	const vec2_t pos = {0, 0};
	technology_t *laserTech;
	technology_t *heavyLaserTech;
	base_t *base;
	campaign_t *campaign;
	employee_t *employee;

	ResetCampaignData();

	laserTech = RS_GetTechByID("rs_laser");
	CU_ASSERT_PTR_NOT_NULL_FATAL(laserTech);
	heavyLaserTech = RS_GetTechByID("rs_weapon_heavylaser");
	CU_ASSERT_PTR_NOT_NULL_FATAL(heavyLaserTech);
	campaign = GetCampaign();

	base = CreateBase("unittestbase", pos);

	CU_ASSERT_TRUE(laserTech->statusResearchable);

	employee = E_GetUnassignedEmployee(base, EMPL_SCIENTIST);
	CU_ASSERT_PTR_NOT_NULL(employee);
	RS_AssignScientist(laserTech, base, employee);

	CU_ASSERT_EQUAL(laserTech->base, base);
	CU_ASSERT_EQUAL(laserTech->scientists, 1);
	CU_ASSERT_EQUAL(laserTech->statusResearch, RS_RUNNING);

	CU_ASSERT_EQUAL(heavyLaserTech->statusResearchable, qfalse);

	n = laserTech->time * 1.25;
	for (i = 0; i < n; i++) {
		RS_ResearchRun();
	}

	CU_ASSERT_EQUAL(laserTech->statusResearch, RS_RUNNING);
	CU_ASSERT_EQUAL(RS_ResearchRun(), 1);
	CU_ASSERT_EQUAL(laserTech->statusResearch, RS_FINISH);

	CU_ASSERT_EQUAL(heavyLaserTech->statusResearchable, qtrue);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testProductionItem (void)
{
	const vec2_t pos = {0, 0};
	base_t *base;
	const objDef_t *od;
	const technology_t *tech;
	int old;
	int i, n;
	productionData_t data;
	production_t *prod;

	ResetCampaignData();

	base = CreateBase("unittestproduction", pos);

	CU_ASSERT_TRUE(B_AtLeastOneExists());
	CU_ASSERT_TRUE(B_GetBuildingStatus(base, B_WORKSHOP));
	CU_ASSERT_TRUE(E_CountHired(base, EMPL_WORKER) > 0);
	CU_ASSERT_TRUE(PR_ProductionAllowed(base));

	od = INVSH_GetItemByID("assault");
	CU_ASSERT_PTR_NOT_NULL_FATAL(od);

	PR_SetData(&data, PRODUCTION_TYPE_ITEM, od);
	old = base->storage.numItems[od->idx];
	prod = PR_QueueNew(base, &data, 1);
	CU_ASSERT_PTR_NOT_NULL(prod);
	tech = RS_GetTechForItem(od);
	n = PR_GetRemainingMinutes(base, prod, 0.0);
	i = tech->produceTime;
	CU_ASSERT_EQUAL(i, PR_GetRemainingHours(base, prod, 0.0));
	for (i = 0; i < n; i++) {
		PR_ProductionRun();
	}

	CU_ASSERT_EQUAL(old, base->storage.numItems[od->idx]);
	PR_ProductionRun();
	CU_ASSERT_EQUAL(old + 1, base->storage.numItems[od->idx]);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testProductionAircraft (void)
{
	const vec2_t pos = {0, 0};
	base_t *base;
	const aircraft_t *aircraft;
	int old;
	int i, n;
	const building_t *buildingTemplate;
	building_t *building;
	productionData_t data;
	production_t *prod;

	ResetCampaignData();

	base = CreateBase("unittestproduction", pos);

	CU_ASSERT_TRUE(B_AtLeastOneExists());
	CU_ASSERT_TRUE(B_GetBuildingStatus(base, B_WORKSHOP));
	CU_ASSERT_TRUE(E_CountHired(base, EMPL_WORKER) > 0);
	CU_ASSERT_TRUE(PR_ProductionAllowed(base));

	aircraft = AIR_GetAircraft("craft_drop_firebird");
	CU_ASSERT_PTR_NOT_NULL_FATAL(aircraft);

	PR_SetData(&data, PRODUCTION_TYPE_AIRCRAFT, aircraft);
	old = base->capacities[CAP_AIRCRAFT_SMALL].cur;
	/* not enough space in hangars */
	CU_ASSERT_PTR_NULL(PR_QueueNew(base, &data, 1));

	buildingTemplate = B_GetBuildingTemplate("building_intercept");
	CU_ASSERT_PTR_NOT_NULL_FATAL(buildingTemplate);
	building = B_SetBuildingByClick(base, buildingTemplate, 1, 1);
	CU_ASSERT_PTR_NOT_NULL_FATAL(building);
	/* hangar is not yet done */
	CU_ASSERT_PTR_NULL(PR_QueueNew(base, &data, 1));

	/** @todo make this a function once base stuff is refactored */
	building->buildingStatus = B_STATUS_WORKING;
	B_UpdateBaseCapacities(B_GetCapacityFromBuildingType(building->buildingType), base);

	aircraft = AIR_GetAircraft("craft_inter_stingray");
	CU_ASSERT_PTR_NOT_NULL_FATAL(aircraft);
	PR_SetData(&data, PRODUCTION_TYPE_AIRCRAFT, aircraft);
	/* no antimatter */
	CU_ASSERT_PTR_NULL(PR_QueueNew(base, &data, 1));

	aircraft = AIR_GetAircraft("craft_inter_stiletto");
	CU_ASSERT_PTR_NOT_NULL_FATAL(aircraft);
	PR_SetData(&data, PRODUCTION_TYPE_AIRCRAFT, aircraft);
	prod = PR_QueueNew(base, &data, 1);
	CU_ASSERT_PTR_NOT_NULL(prod);

	n = PR_GetRemainingMinutes(base, prod, 0.0);
	i = aircraft->tech->produceTime;
	CU_ASSERT_EQUAL(i, PR_GetRemainingHours(base, prod, 0.0));
	for (i = 0; i < n; i++) {
		PR_ProductionRun();
	}

	CU_ASSERT_EQUAL(old, base->capacities[CAP_AIRCRAFT_SMALL].cur);
	PR_ProductionRun();
	CU_ASSERT_EQUAL(old + 1, base->capacities[CAP_AIRCRAFT_SMALL].cur);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testDisassembly (void)
{
	const vec2_t pos = {0, 0};
	base_t *base;
	const aircraft_t *ufo;
	int old;
	int i, n;
	storedUFO_t *storedUFO;
	productionData_t data;
	installation_t *installation;
	production_t *prod;

	ResetCampaignData();

	base = CreateBase("unittestproduction", pos);

	CU_ASSERT_TRUE(B_AtLeastOneExists());
	CU_ASSERT_TRUE(B_GetBuildingStatus(base, B_WORKSHOP));
	CU_ASSERT_TRUE(E_CountHired(base, EMPL_WORKER) > 0);
	CU_ASSERT_TRUE(PR_ProductionAllowed(base));

	ufo = AIR_GetAircraft("craft_ufo_fighter");
	CU_ASSERT_PTR_NOT_NULL_FATAL(ufo);

	installation = CreateInstallation("unittestproduction", pos);

	storedUFO = US_StoreUFO(ufo, installation, ccs.date, 1.0);
	CU_ASSERT_PTR_NOT_NULL_FATAL(storedUFO);
	CU_ASSERT_EQUAL(storedUFO->status, SUFO_RECOVERED);
	PR_SetData(&data, PRODUCTION_TYPE_DISASSEMBLY, storedUFO);
	prod = PR_QueueNew(base, &data, 1);
	CU_ASSERT_PTR_NOT_NULL(prod);

	old = base->capacities[CAP_ITEMS].cur;
	n = PR_GetRemainingMinutes(base, prod, 0.0);
	i = storedUFO->comp->time;
	CU_ASSERT_EQUAL(i, PR_GetRemainingHours(base, prod, 0.0));
	for (i = 0; i < n; i++) {
		PR_ProductionRun();
	}

	CU_ASSERT_EQUAL(old, base->capacities[CAP_ITEMS].cur);
	PR_ProductionRun();
	CU_ASSERT_NOT_EQUAL(old, base->capacities[CAP_ITEMS].cur);

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testMap (void)
{
	vec2_t pos;

	ResetCampaignData();

	Vector2Set(pos, -51, 0);
	CU_ASSERT_TRUE(MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN)));

	Vector2Set(pos, 51, 0);
	CU_ASSERT_TRUE(!MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN)));

	Vector2Set(pos, 20, 20);
	CU_ASSERT_TRUE(MapIsWater(MAP_GetColor(pos, MAPTYPE_TERRAIN)));
}

static void testAirFight (void)
{
	const vec2_t destination = { 10, 10 };
	mission_t *mission;
	aircraft_t *aircraft;
	base_t *base;
	int i, cnt;
	/* just some random delta time value that is high enough
	 * to ensure that all the weapons are reloaded */
	const int deltaTime = 1000;
	campaign_t *campaign;

	ResetCampaignData();

	campaign = GetCampaign();

	base = CreateBase("unittestairfight", destination);
	CU_ASSERT_PTR_NOT_NULL_FATAL(base);

	cnt = AIR_BaseCountAircraft(base);
	i = 0;
	AIR_ForeachFromBase(aircraft, base)
		i++;
	CU_ASSERT_EQUAL(i, cnt);

	aircraft = AIR_GetFirstFromBase(base);
	CU_ASSERT_PTR_NOT_NULL_FATAL(aircraft);
	aircraft->status = AIR_IDLE;
	CU_ASSERT_TRUE(AIR_IsAircraftOnGeoscape(aircraft));

	/* prepare the mission */
	mission = CP_CreateNewMission(INTERESTCATEGORY_INTERCEPT, qtrue);
	CU_ASSERT_PTR_NOT_NULL_FATAL(mission);
	CU_ASSERT_EQUAL(mission->stage, STAGE_NOT_ACTIVE);
	CP_InterceptNextStage(mission);
	CU_ASSERT_EQUAL(mission->stage, STAGE_COME_FROM_ORBIT);
	CP_InterceptNextStage(mission);
	CU_ASSERT_EQUAL(mission->stage, STAGE_INTERCEPT);
	mission->ufo->ufotype = UFO_FIGHTER;
	/* we have to update the routing data here to be sure that the ufo is
	 * not spawned on the other side of the globe */
	Vector2Copy(destination, mission->ufo->pos);
	UFO_SendToDestination(mission->ufo, destination);
	CU_ASSERT_TRUE(VectorEqual(mission->ufo->pos, base->pos));
	CU_ASSERT_TRUE(VectorEqual(mission->ufo->pos, aircraft->pos));

	CU_ASSERT_TRUE(aircraft->maxWeapons > 0);
	for (i = 0; i < aircraft->maxWeapons; i++)
		CU_ASSERT_TRUE(aircraft->weapons[i].delayNextShot == 0);

	/* search a target */
	UFO_CampaignRunUFOs(campaign, deltaTime);
	CU_ASSERT_PTR_NOT_NULL_FATAL(mission->ufo->aircraftTarget);

	/* ensure that one hit will destroy the craft */
	mission->ufo->aircraftTarget->damage = 1;
	srand(1);
	UFO_CheckShootBack(campaign, mission->ufo, mission->ufo->aircraftTarget);

	/* one projectile should be spawned */
	CU_ASSERT_EQUAL(ccs.numProjectiles, 1);
	AIRFIGHT_CampaignRunProjectiles(campaign, deltaTime);

	/* target is destroyed */
	CU_ASSERT_PTR_NULL(mission->ufo->aircraftTarget);

	/* one aircraft less */
	CU_ASSERT_EQUAL(cnt - 1, AIR_BaseCountAircraft(base));

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testGeoscape (void)
{
	ResetCampaignData();
}

static void testRadar (void)
{
	base_t *base;
	const vec2_t destination = { 10, 10 };
	aircraft_t *ufo;
	mission_t *mission;
	ufoType_t ufoType = UFO_FIGHTER;

	ResetCampaignData();

	base = CreateBase("unittestradar", destination);

	mission = CP_CreateNewMission(INTERESTCATEGORY_INTERCEPT, qtrue);
	ufo = UFO_AddToGeoscape(ufoType, destination, mission);
	Vector2Copy(destination, ufo->pos);
	UFO_SendToDestination(ufo, destination);
	CU_ASSERT_TRUE(VectorEqual(ufo->pos, base->pos));
	CU_ASSERT_TRUE(VectorEqual(ufo->pos, ufo->pos));
	/* to ensure that the UFOs are really detected when they are in range */
	base->radar.ufoDetectionProbability = 1.0;
	CU_ASSERT_TRUE(RADAR_CheckUFOSensored(&base->radar, base->pos, ufo, qfalse));

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testNation (void)
{
	const nation_t *nation;
	campaign_t *campaign;

	ResetCampaignData();

	nation = NAT_GetNationByID("europe");
	CU_ASSERT_PTR_NOT_NULL(nation);

	campaign = GetCampaign();

	NAT_HandleBudget(campaign);
	/** @todo implement a check here */
}

static void testMarket (void)
{
	campaign_t *campaign;

	ResetCampaignData();

	campaign = GetCampaign();

	RS_InitTree(campaign, qfalse);

	BS_InitMarket(campaign);

	CL_CampaignRunMarket(campaign);
	/** @todo implement a check here */
}

static void testXVI (void)
{
	ResetCampaignData();
}

static void testSaveLoad (void)
{
	const vec2_t pos = {0, 0};
	base_t *base;
	campaign_t *campaign;

	ResetCampaignData();

	campaign = GetCampaign();

	SAV_Init();

	ccs.curCampaign = campaign;

	Cvar_Set("save_compressed", "0");

	base = CreateBase("unittestbase", pos);

	{
		CU_ASSERT(base->founded);
		CU_ASSERT_EQUAL(base->baseStatus, BASE_WORKING);

		Cmd_ExecuteString("game_quicksave");
	}
	{
		B_Destroy(base);

		CU_ASSERT_EQUAL(base->baseStatus, BASE_DESTROYED);

		E_DeleteAllEmployees(NULL);

		B_SetName(base, "unittestbase2");
	}
	{
		ResetCampaignData();

		Cmd_ExecuteString("game_quickload");

		/** @todo check that the savegame was successfully loaded */

		CU_ASSERT_EQUAL(base->baseStatus, BASE_WORKING);
		CU_ASSERT_STRING_EQUAL(base->name, "unittestbase");

		B_Destroy(base);

		E_DeleteAllEmployees(NULL);
	}

	base->founded = qfalse;
}

static void testCampaignRun (void)
{
	int i;
	int deltaDays;
	campaign_t *campaign;
	base_t *base;
	const vec2_t destination = { 10, 10 };

	ResetCampaignData();

	base = CreateBase("unittestcampaignrun", destination);

	campaign = GetCampaign();

	RS_InitTree(campaign, qfalse);

	BS_InitMarket(campaign);

	cls.frametime = 1;

	deltaDays = ccs.date.day;

	while (deltaDays < 10) {
		if (++i > 10000000)
			UFO_CU_FAIL_MSG_FATAL(va("Time did not advance for 10 days, only %i", deltaDays));
		ccs.gameTimeScale = 1;
		CL_CampaignRun(campaign);
		deltaDays = ccs.date.day - deltaDays;
	}

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

static void testLoad (void)
{
	int i;
	aircraft_t *ufo;
	const char *error;

	ResetCampaignData();

	CP_InitOverlay();

	ccs.curCampaign = NULL;
	CU_ASSERT_TRUE(SAV_GameLoad("unittest1", &error));
	CU_ASSERT_PTR_NOT_NULL(ccs.curCampaign);

	i = 0;
	ufo = NULL;
	while ((ufo = UFO_GetNextOnGeoscape(ufo)) != NULL)
		i++;

	/* there should be one ufo on the geoscape */
	CU_ASSERT_EQUAL(i, 1);
}

static void testDateHandling (void)
{
	date_t date;
	date.day = 300;
	date.sec = 300;

	ccs.date = date;

	CU_ASSERT_TRUE(Date_IsDue(&date));
	CU_ASSERT_FALSE(Date_LaterThan(&ccs.date, &date));

	date.day = 299;
	date.sec = 310;

	CU_ASSERT_FALSE(Date_IsDue(&date));
	CU_ASSERT_TRUE(Date_LaterThan(&ccs.date, &date));

	date.day = 301;
	date.sec = 0;

	CU_ASSERT_TRUE(Date_IsDue(&date));
	CU_ASSERT_FALSE(Date_LaterThan(&ccs.date, &date));

	date.day = 300;
	date.sec = 299;

	CU_ASSERT_FALSE(Date_IsDue(&date));
	CU_ASSERT_TRUE(Date_LaterThan(&ccs.date, &date));

	date.day = 300;
	date.sec = 301;

	CU_ASSERT_TRUE(Date_IsDue(&date));
	CU_ASSERT_FALSE(Date_LaterThan(&ccs.date, &date));
}

static void testHospital (void)
{
	base_t *base;
	const vec2_t destination = { 10, 10 };
	employeeType_t type;
	int i;

	ResetCampaignData();

	base = CreateBase("unittesthospital", destination);

	i = 0;
	/* hurt all employees */
	for (type = 0; type < MAX_EMPL; type++) {
		employee_t *employee;
		E_Foreach(type, employee) {
			if (!E_IsHired(employee))
				continue;
			employee->chr.HP = employee->chr.maxHP - 10;
			i++;
		}
	}
	CU_ASSERT_NOT_EQUAL(i, 0);

	HOS_HospitalRun();

	for (type = 0; type < MAX_EMPL; type++) {
		employee_t *employee;
		E_Foreach(type, employee) {
			if (!E_IsHired(employee))
				continue;
			if (employee->chr.HP == employee->chr.maxHP - 10)
				UFO_CU_FAIL_MSG_FATAL(va("%i/%i", employee->chr.HP, employee->chr.maxHP));
		}
	}

	/* cleanup for the following tests */
	E_DeleteAllEmployees(NULL);

	base->founded = qfalse;
}

/* https://sourceforge.net/tracker/index.php?func=detail&aid=3090011&group_id=157793&atid=805242 */
static void test3090011 (void)
{
	const char *error = NULL;
	qboolean success;

	ResetCampaignData();

	CP_InitOverlay();

	success = SAV_GameLoad("3090011", &error);
	UFO_CU_ASSERT_TRUE_MSG(success, error);
}

int UFO_AddCampaignTests (void)
{
	/* add a suite to the registry */
	CU_pSuite campaignSuite = CU_add_suite("CampaignTests", UFO_InitSuiteCampaign, UFO_CleanSuiteCampaign);

	if (campaignSuite == NULL)
		return CU_get_error();

	/* add the tests to the suite */
	if (CU_ADD_TEST(campaignSuite, testAutoMissions) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testBaseBuilding) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testAircraftHandling) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testEmployeeHandling) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testTransferItem) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testTransferAircraft) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testTransferEmployee) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testUFORecovery) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testResearch) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testProductionItem) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testProductionAircraft) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testDisassembly) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testMap) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testAirFight) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testGeoscape) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testRadar) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testNation) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testMarket) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testXVI) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testSaveLoad) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testCampaignRun) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testLoad) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testDateHandling) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, testHospital) == NULL)
		return CU_get_error();

	if (CU_ADD_TEST(campaignSuite, test3090011) == NULL)
		return CU_get_error();

	return CUE_SUCCESS;
}
