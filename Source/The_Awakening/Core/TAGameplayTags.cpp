#include "Core/TAGameplayTags.h"

namespace TAGameplayTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Data_Damage, "Data.Damage", "SetByCaller magnitude of the damage GameplayEffect.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Dead, "State.Dead", "Owner is dead until respawn completes.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Invulnerable, "State.Invulnerable", "Normal and true damage are ignored unless the source is allowed.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(State_Undying, "State.Undying", "Normal and true damage cannot reduce Health below 1.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type, "Damage.Type", "Damage channel.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Normal, "Damage.Type.Normal", "Full formula: vulnerability, defense, reduction.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_True, "Damage.Type.True", "Amount times true-damage multipliers only.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Type_Forced, "Damage.Type.Forced", "Directly subtracts Health; ignores modifiers and protection.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Source, "Damage.Source", "Who or what dealt the damage.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Source_Enemy, "Damage.Source.Enemy", "Enemy attacks.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Source_Puzzle, "Damage.Source.Puzzle", "Puzzle penalties.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Source_Console, "Damage.Source.Console", "Debug console commands.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Source_Environment, "Damage.Source.Environment", "Kill volumes, traps and hazards.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage_Source_Script, "Damage.Source.Script", "Story and scripted events.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Effect_ClearOnRespawn, "Effect.ClearOnRespawn", "Duration effects removed on respawn.");

	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim, "Anim", "Character actions played through UTACharacterActionComponent.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_Pickup, "Anim.Pickup", "Pick up a document from the world.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_PutDown, "Anim.PutDown", "Put a document back into the world.");
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(Anim_InsertPage, "Anim.InsertPage", "Insert a collected document into the diary.");
}
