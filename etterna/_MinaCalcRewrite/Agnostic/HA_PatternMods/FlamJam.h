#pragma once
#include "../../PatternModHelpers.h"
#include "../HA_Sequencers/FlamSequencing.h"

// MAKE FLAM WIDE RANGE?
// ^ YES DO THIS
struct FlamJamMod
{
	const CalcPatternMod _pmod = FlamJam;
	const std::string name = "FlamJamMod";

#pragma region params
	float min_mod = 0.472256F;
	float max_mod = 0.976675F;
	float scaler = 2.53098F;
	float base = 0.0980244F;

	float group_tol = 36.9486F;
	float step_tol = 18.2871F;

	const std::vector<std::pair<std::string, float*>> _params{
		{ "min_mod", &min_mod },
		{ "max_mod", &max_mod },
		{ "scaler", &scaler },
		{ "base", &base },

		// params for fj_sequencing
		{ "group_tol", &group_tol },
		{ "step_tol", &step_tol },
	};
#pragma endregion params and param map

	// sequencer
	FJ_Sequencer fj;
	float pmod = neutral;

	void setup() { fj.set_params(group_tol, step_tol, scaler); }

	void advance_sequencing(const float& ms_now, const unsigned& notes)
	{
		fj(ms_now, notes);
	}

	auto operator()() -> float
	{
		// no flams
		if (fj.mod_parts[0] == 1.F) {
			return neutral;
		}

		// if (fj.the_fifth_flammament) {
		//	return min_mod;
		//}

		// water down single flams
		pmod = 1.08522F;
		for (auto& mp : fj.mod_parts) {
			pmod += mp;
		}
		pmod /= 5.06302F;
		pmod = std::clamp(base + pmod, min_mod, max_mod);

		// reset flags n stuff
		fj.handle_interval_end();

		return pmod;
	}
};
