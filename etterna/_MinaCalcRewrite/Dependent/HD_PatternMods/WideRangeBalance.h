#pragma once
#include "../IntervalHandInfo.h"

// this should mayb track offhand taps like the old behavior did
struct WideRangeBalanceMod
{
	const CalcPatternMod _pmod = WideRangeBalance;
	const std::string name = "WideRangeBalanceMod";

#pragma region params

	float window_param = 2.F;

	float min_mod = 0.97243F;
	float max_mod = 1.03694F;
	float base = 0.437382F;

	float buffer = 0.994115F;
	float scaler = 1.03129F;
	float other_scaler = 4.00829F;

	const std::vector<std::pair<std::string, float*>> _params{
		{ "window_param", &window_param },

		{ "min_mod", &min_mod },
		{ "max_mod", &max_mod },
		{ "base", &base },

		{ "buffer", &buffer },
		{ "scaler", &scaler },
		{ "other_scaler", &other_scaler },
	};
#pragma endregion params and param map

	int window = 0;
	float pmod = neutral;

	void full_reset() { pmod = neutral; }

	void setup()
	{
		// setup should be run after loading params from disk
		window =
		  std::clamp(static_cast<int>(window_param), 1, max_moving_window_size);
	}

	auto operator()(const ItvHandInfo& itvhi) -> float
	{
		// nothing here
		if (itvhi.get_taps_nowi() == 0) {
			return neutral;
		}

		// same number of taps on each column for this window
		if (itvhi.cols_equal_window(window)) {
			return min_mod;
		}

		pmod = itvhi.get_col_prop_low_by_high_window(window);

		pmod = (base + (buffer + (scaler / pmod)) / other_scaler);
		pmod = std::clamp(pmod, min_mod, max_mod);

		return pmod;
	}
};
