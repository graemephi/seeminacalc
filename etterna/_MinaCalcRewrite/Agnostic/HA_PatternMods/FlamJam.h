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
	float min_mod = 0.47151F;
	float max_mod = 0.960172F;
	float scaler = 2.74915F;
	float base = 0.0949467F;

	float group_tol = 32.2433F;
	float step_tol = 17.1501F;

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
		pmod = 0.974736F;
		for (auto& mp : fj.mod_parts) {
			pmod += mp;
		}
		pmod /= 4.59369F;
		pmod = std::clamp(base + pmod, min_mod, max_mod);

		// reset flags n stuff
		fj.handle_interval_end();

		return pmod;
	}
};
