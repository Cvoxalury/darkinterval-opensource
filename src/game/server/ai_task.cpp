//========================================================================//
//
// Purpose: Sets up the tasks for default AI.
//
// $NoKeywords: $
//=============================================================================//

#include "cbase.h"
#include "stringregistry.h"
#include "ai_basenpc.h"
#include "ai_task.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

const char * g_ppszTaskFailureText[] =
{
	"No failure",                                    // NO_TASK_FAILURE
	"No Target",                                     // FAIL_NO_TARGET
	"Weapon owned by someone else",                  // FAIL_WEAPON_OWNED
	"Weapon/Item doesn't exist",                     // FAIL_ITEM_NO_FIND
	"No hint node",                                  // FAIL_NO_HINT_NODE
	"Schedule not found",                            // FAIL_SCHEDULE_NOT_FOUND
	"Don't have an enemy",                           // FAIL_NO_ENEMY
	"Found no backaway node",                        // FAIL_NO_BACKAWAY_NODE
	"Couldn't find cover",                           // FAIL_NO_COVER
	"Couldn't find flank",                           // FAIL_NO_FLANK
	"Couldn't find shoot position",                  // FAIL_NO_SHOOT
	"Don't have a route",                            // FAIL_NO_ROUTE
	"Don't have a route: no goal",                   // FAIL_NO_ROUTE_GOAL
	"Don't have a route: blocked",                   // FAIL_NO_ROUTE_BLOCKED
	"Don't have a route: illegal move",              // FAIL_NO_ROUTE_ILLEGAL
	"Couldn't walk to target",                       // FAIL_NO_WALK
	"Node already locked",                           // FAIL_ALREADY_LOCKED
	"No sound present",                              // FAIL_NO_SOUND
	"No scent present",                              // FAIL_NO_SCENT
	"Bad activity",                                  // FAIL_BAD_ACTIVITY
	"No goal entity",                                // FAIL_NO_GOAL
	"No player",                                     // FAIL_NO_PLAYER
	"Can't reach any nodes",                         // FAIL_NO_REACHABLE_NODE
	"No AI Network to Use",                          // FAIL_NO_AI_NETWORK
	"Bad position to Target",                        // FAIL_BAD_POSITION
	"Route Destination No Longer Valid",             // FAIL_BAD_PATH_GOAL
	"Stuck on top of something",                     // FAIL_STUCK_ONTOP
	"Item has been taken",		                     // FAIL_ITEM_TAKEN
};

const char *TaskFailureToString( AI_TaskFailureCode_t code )
{
	const char *pszResult;
	if ( code < 0 || code >= NUM_FAIL_CODES )
		pszResult = (const char *)code;
	else
		pszResult = g_ppszTaskFailureText[code];
	return pszResult;
}

//-----------------------------------------------------------------------------
// Purpose: Given and task name, return the task ID
//-----------------------------------------------------------------------------
int CAI_BaseNPC::GetTaskID(const char* taskName)
{
	return GetSchedulingSymbols()->TaskSymbolToId( taskName );
}

//-----------------------------------------------------------------------------
// Purpose: Initialize the task name string registry
// Input  :
// Output :
//-----------------------------------------------------------------------------
void CAI_BaseNPC::InitDefaultTaskSR(void)
{
	#define ADD_DEF_TASK( name ) idSpace.AddTask(#name, name, "CAI_BaseNPC" )

	CAI_ClassScheduleIdSpace &idSpace = CAI_BaseNPC::AccessClassScheduleIdSpaceDirect();

	ADD_DEF_TASK( TASK_INVALID );
	ADD_DEF_TASK( TASK_ANNOUNCE_ATTACK );
	ADD_DEF_TASK( TASK_RESET_ACTIVITY );
	ADD_DEF_TASK( TASK_WAIT );
	ADD_DEF_TASK( TASK_WAIT_FACE_ENEMY );
	ADD_DEF_TASK( TASK_WAIT_FACE_ENEMY_RANDOM );
	ADD_DEF_TASK( TASK_WAIT_PVS );
	ADD_DEF_TASK( TASK_SUGGEST_STATE );
	ADD_DEF_TASK( TASK_TARGET_PLAYER );
	ADD_DEF_TASK( TASK_SCRIPT_WALK_TO_TARGET );
	ADD_DEF_TASK( TASK_SCRIPT_RUN_TO_TARGET );
	ADD_DEF_TASK( TASK_SCRIPT_CUSTOM_MOVE_TO_TARGET );
	ADD_DEF_TASK( TASK_MOVE_TO_TARGET_RANGE );
	ADD_DEF_TASK( TASK_MOVE_TO_GOAL_RANGE );
	ADD_DEF_TASK( TASK_MOVE_AWAY_PATH );
	ADD_DEF_TASK( TASK_GET_PATH_AWAY_FROM_BEST_SOUND );
	ADD_DEF_TASK( TASK_SET_GOAL );
	ADD_DEF_TASK( TASK_GET_PATH_TO_GOAL );
	ADD_DEF_TASK( TASK_GET_PATH_TO_ENEMY );
	ADD_DEF_TASK( TASK_GET_PATH_TO_ENEMY_LKP );
	ADD_DEF_TASK( TASK_GET_CHASE_PATH_TO_ENEMY );
	ADD_DEF_TASK( TASK_GET_PATH_TO_ENEMY_LKP_LOS );
	ADD_DEF_TASK( TASK_GET_PATH_TO_RANGE_ENEMY_LKP_LOS );
	ADD_DEF_TASK( TASK_GET_PATH_TO_ENEMY_CORPSE );
	ADD_DEF_TASK( TASK_GET_PATH_TO_PLAYER );
	ADD_DEF_TASK( TASK_GET_PATH_TO_ENEMY_LOS );
	ADD_DEF_TASK( TASK_GET_FLANK_ARC_PATH_TO_ENEMY_LOS );
	ADD_DEF_TASK( TASK_GET_FLANK_RADIUS_PATH_TO_ENEMY_LOS );
	ADD_DEF_TASK( TASK_GET_PATH_TO_TARGET );
	ADD_DEF_TASK( TASK_GET_PATH_TO_TARGET_WEAPON );
	ADD_DEF_TASK( TASK_CREATE_PENDING_WEAPON );
	ADD_DEF_TASK( TASK_GET_PATH_TO_HINTNODE );
	ADD_DEF_TASK( TASK_STORE_LASTPOSITION );
	ADD_DEF_TASK( TASK_CLEAR_LASTPOSITION );
	ADD_DEF_TASK( TASK_STORE_POSITION_IN_SAVEPOSITION );
	ADD_DEF_TASK( TASK_STORE_BESTSOUND_IN_SAVEPOSITION );
	ADD_DEF_TASK( TASK_STORE_BESTSOUND_REACTORIGIN_IN_SAVEPOSITION );
	ADD_DEF_TASK( TASK_REACT_TO_COMBAT_SOUND );
	ADD_DEF_TASK( TASK_STORE_ENEMY_POSITION_IN_SAVEPOSITION );
	ADD_DEF_TASK( TASK_GET_PATH_TO_COMMAND_GOAL );
	ADD_DEF_TASK( TASK_MARK_COMMAND_GOAL_POS );
	ADD_DEF_TASK( TASK_CLEAR_COMMAND_GOAL );
	ADD_DEF_TASK( TASK_GET_PATH_TO_LASTPOSITION );
	ADD_DEF_TASK( TASK_GET_PATH_TO_SAVEPOSITION );
	ADD_DEF_TASK( TASK_GET_PATH_TO_SAVEPOSITION_LOS );
	ADD_DEF_TASK( TASK_GET_PATH_TO_BESTSOUND );
	ADD_DEF_TASK( TASK_GET_PATH_TO_BESTSCENT );
	ADD_DEF_TASK( TASK_GET_PATH_TO_RANDOM_NODE );
	ADD_DEF_TASK( TASK_RUN_PATH );
	ADD_DEF_TASK( TASK_WALK_PATH );
	ADD_DEF_TASK( TASK_WALK_PATH_TIMED );
	ADD_DEF_TASK( TASK_WALK_PATH_WITHIN_DIST );
	ADD_DEF_TASK( TASK_RUN_PATH_WITHIN_DIST );
	ADD_DEF_TASK( TASK_WALK_PATH_FOR_UNITS );
	ADD_DEF_TASK( TASK_RUN_PATH_FOR_UNITS );
	ADD_DEF_TASK( TASK_RUN_PATH_FLEE );
	ADD_DEF_TASK( TASK_RUN_PATH_TIMED );
	ADD_DEF_TASK( TASK_STRAFE_PATH );
	ADD_DEF_TASK( TASK_CLEAR_MOVE_WAIT );
	ADD_DEF_TASK( TASK_SMALL_FLINCH );
	ADD_DEF_TASK( TASK_BIG_FLINCH );
	ADD_DEF_TASK( TASK_DEFER_DODGE );
	ADD_DEF_TASK( TASK_FACE_IDEAL );
	ADD_DEF_TASK( TASK_FACE_REASONABLE );
	ADD_DEF_TASK( TASK_FACE_PATH );
	ADD_DEF_TASK( TASK_FACE_PLAYER );
	ADD_DEF_TASK( TASK_FACE_ENEMY );
	ADD_DEF_TASK( TASK_FACE_HINTNODE );
	ADD_DEF_TASK( TASK_PLAY_HINT_ACTIVITY );
	ADD_DEF_TASK( TASK_FACE_TARGET );
	ADD_DEF_TASK( TASK_FACE_LASTPOSITION );
	ADD_DEF_TASK( TASK_FACE_SAVEPOSITION );
	ADD_DEF_TASK( TASK_FACE_AWAY_FROM_SAVEPOSITION );
	ADD_DEF_TASK( TASK_SET_IDEAL_YAW_TO_CURRENT );
	ADD_DEF_TASK( TASK_RANGE_ATTACK1 );
	ADD_DEF_TASK( TASK_RANGE_ATTACK2 );
	ADD_DEF_TASK( TASK_MELEE_ATTACK1 );
	ADD_DEF_TASK( TASK_MELEE_ATTACK2 );
	ADD_DEF_TASK( TASK_RELOAD );
	ADD_DEF_TASK( TASK_SPECIAL_ATTACK1 );
	ADD_DEF_TASK( TASK_SPECIAL_ATTACK2 );
	ADD_DEF_TASK( TASK_FIND_HINTNODE );
	ADD_DEF_TASK( TASK_CLEAR_HINTNODE );
	ADD_DEF_TASK( TASK_FIND_LOCK_HINTNODE );
	ADD_DEF_TASK( TASK_LOCK_HINTNODE );
	ADD_DEF_TASK( TASK_SOUND_ANGRY );
	ADD_DEF_TASK( TASK_SOUND_DEATH );
	ADD_DEF_TASK( TASK_SOUND_IDLE );
	ADD_DEF_TASK( TASK_SOUND_WAKE );
	ADD_DEF_TASK( TASK_SOUND_PAIN );
	ADD_DEF_TASK( TASK_SOUND_DIE );
	ADD_DEF_TASK( TASK_SPEAK_SENTENCE );
	ADD_DEF_TASK( TASK_WAIT_FOR_SPEAK_FINISH );
	ADD_DEF_TASK( TASK_SET_ACTIVITY );
	ADD_DEF_TASK( TASK_RANDOMIZE_FRAMERATE );
	ADD_DEF_TASK( TASK_SET_SCHEDULE );
	ADD_DEF_TASK( TASK_SET_FAIL_SCHEDULE );
	ADD_DEF_TASK( TASK_SET_TOLERANCE_DISTANCE );
	ADD_DEF_TASK( TASK_SET_ROUTE_SEARCH_TIME );
	ADD_DEF_TASK( TASK_CLEAR_FAIL_SCHEDULE );
	ADD_DEF_TASK( TASK_PLAY_SEQUENCE );
	ADD_DEF_TASK( TASK_PLAY_PRIVATE_SEQUENCE );
	ADD_DEF_TASK( TASK_PLAY_PRIVATE_SEQUENCE_FACE_ENEMY );
	ADD_DEF_TASK( TASK_PLAY_SEQUENCE_FACE_ENEMY );
	ADD_DEF_TASK( TASK_PLAY_SEQUENCE_FACE_TARGET );
	ADD_DEF_TASK( TASK_FIND_COVER_FROM_BEST_SOUND );
	ADD_DEF_TASK( TASK_FIND_COVER_FROM_ENEMY );
	ADD_DEF_TASK( TASK_FIND_LATERAL_COVER_FROM_ENEMY );
	ADD_DEF_TASK( TASK_FIND_BACKAWAY_FROM_SAVEPOSITION );
	ADD_DEF_TASK( TASK_FIND_NODE_COVER_FROM_ENEMY );
	ADD_DEF_TASK( TASK_FIND_NEAR_NODE_COVER_FROM_ENEMY );
	ADD_DEF_TASK( TASK_FIND_FAR_NODE_COVER_FROM_ENEMY );
	ADD_DEF_TASK( TASK_FIND_COVER_FROM_ORIGIN );
	ADD_DEF_TASK( TASK_DIE );
	ADD_DEF_TASK( TASK_WAIT_FOR_SCRIPT );
	ADD_DEF_TASK( TASK_PUSH_SCRIPT_ARRIVAL_ACTIVITY );
	ADD_DEF_TASK( TASK_PLAY_SCRIPT );
	ADD_DEF_TASK( TASK_PLAY_SCRIPT_POST_IDLE );
	ADD_DEF_TASK( TASK_ENABLE_SCRIPT );
	ADD_DEF_TASK( TASK_PLANT_ON_SCRIPT );
	ADD_DEF_TASK( TASK_FACE_SCRIPT );
	ADD_DEF_TASK( TASK_PLAY_SCENE );
	ADD_DEF_TASK( TASK_WAIT_RANDOM );
	ADD_DEF_TASK( TASK_WAIT_INDEFINITE );
	ADD_DEF_TASK( TASK_STOP_MOVING );
	ADD_DEF_TASK( TASK_TURN_LEFT );
	ADD_DEF_TASK( TASK_TURN_RIGHT );
	ADD_DEF_TASK( TASK_REMEMBER );
	ADD_DEF_TASK( TASK_FORGET );
	ADD_DEF_TASK( TASK_WAIT_FOR_MOVEMENT );
	ADD_DEF_TASK( TASK_WAIT_FOR_MOVEMENT_STEP );
	ADD_DEF_TASK( TASK_WAIT_UNTIL_NO_DANGER_SOUND );
	ADD_DEF_TASK( TASK_WEAPON_FIND );
	ADD_DEF_TASK( TASK_WEAPON_PICKUP );
	ADD_DEF_TASK( TASK_WEAPON_RUN_PATH );
	ADD_DEF_TASK( TASK_WEAPON_CREATE );
	ADD_DEF_TASK( TASK_ITEM_RUN_PATH );
	ADD_DEF_TASK( TASK_ITEM_PICKUP );
	ADD_DEF_TASK( TASK_USE_SMALL_HULL );
	ADD_DEF_TASK( TASK_FALL_TO_GROUND );
	ADD_DEF_TASK( TASK_WANDER );
	ADD_DEF_TASK( TASK_FREEZE );
	ADD_DEF_TASK( TASK_GATHER_CONDITIONS );
	ADD_DEF_TASK( TASK_IGNORE_OLD_ENEMIES );
	ADD_DEF_TASK( TASK_DEBUG_BREAK );
	ADD_DEF_TASK( TASK_ADD_HEALTH );
	ADD_DEF_TASK( TASK_GET_PATH_TO_INTERACTION_PARTNER );
	ADD_DEF_TASK( TASK_PRE_SCRIPT );
	
#ifdef DARKINTERVAL	// DI NEW: Live Ragdolls Experiment (inspired by Dark Messiah)
	ADD_DEF_TASK(TASK_DI_FALL_IN_RAGDOLL_PUSH_BEGIN); // become invisible and intangible
	ADD_DEF_TASK(TASK_DI_FALL_IN_RAGDOLL_PUSH); // create ragdoll copy
	ADD_DEF_TASK(TASK_DI_WAIT_UNTIL_RAGDOLL_ENDS_FALL); // continuously check ragdoll position, follow it (teleport), measure speed and ground entity
	ADD_DEF_TASK(TASK_DI_WAIT_UNTIL_RAGDOLL_ENDS_FALL_END); // once it slowed down and is resting, tell it to unragdoll back into us, and un-hide ourselves
		
	// DI change: a separate task to play the idle anim, but CHECK if it's done playing, 
	// for when we have more than one ACT_IDLE. (fixing the long-standing Source bug with resetting ACT_IDLEs)
	ADD_DEF_TASK(TASK_PLAY_ACT_IDLE);
#endif
}


