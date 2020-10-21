#pragma once
#include "../IntervalHandInfo.h"

struct BalanceMod
{
	const CalcPatternMod _pmod = Balance;
	const std::string name = "BalanceMod";

#pragma region params

	float min_mod = 0.951774F;
	float max_mod = 1.02367F;
	float mod_base = 0.32652F;
	float buffer = 0.971948F;
	float scaler = 1.07741F;
	float other_scaler = 3.87114F;

	const std::vector<std::pair<std::string, float*>> _params{
		{ "min_mod", &min_mod },   { "max_mod", &max_mod },
		{ "mod_base", &mod_base }, { "buffer", &buffer },
		{ "scaler", &scaler },	   { "other_scaler", &other_scaler },
	};
#pragma endregion params and param map

	float pmod = neutral;

	void full_reset() { pmod = neutral; }

	auto operator()(const ItvHandInfo& itvhi) -> float
	{
		// nothing here
		if (itvhi.get_taps_nowi() == 0) {
			return neutral;
		}

		// same number of taps on each column
		if (itvhi.cols_equal_now()) {
			return min_mod;
		}

		// probably should NOT do this but leaving enabled for now so i can
		// verify structural changes dont change output diff
		// jack, dunno if this is worth bothering about? it would only matter
		// for tech and it may matter too much there? idk
		if (itvhi.get_col_taps_nowi(col_left) == 0 ||
			itvhi.get_col_taps_nowi(col_right) == 0) {
			return max_mod;
		}

		pmod = itvhi.get_col_prop_low_by_high();
		pmod = (mod_base + (buffer + (scaler / pmod)) / other_scaler);
		pmod = std::clamp(pmod, min_mod, max_mod);

		return pmod;
	}
};
