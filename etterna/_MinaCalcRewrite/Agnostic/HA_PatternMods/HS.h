#pragma once
#include "../../PatternModHelpers.h"
#include "../MetaIntervalInfo.h"

struct HSMod
{
	const CalcPatternMod _pmod = HS;
	// const vector<CalcPatternMod> _dbg = { HSS, HSJ };
	const std::string name = "HSMod";
	const int _tap_size = hand;

#pragma region params

	float min_mod = 0.619856F;
	float max_mod = 1.12585F;
	float mod_base = 0.403007F;
	float prop_buffer = 0.978612F;

	float total_prop_min = 0.592366F;
	float total_prop_max = 1.05693F;

	// was ~32/7, is higher now to push up light hs (maybe overkill tho)
	float total_prop_scaler = 5.5979F;
	float total_prop_base = 0.372531F;

	float split_hand_pool = 1.64555F;
	float split_hand_min = 0.929204F;
	float split_hand_max = 1.02726F;
	float split_hand_scaler = 0.934706F;

	float jack_pool = 1.38828F;
	float jack_min = 0.541868F;
	float jack_max = 1.02646F;
	float jack_scaler = 0.993603F;

	float decay_factor = 0.0513515F;

	const std::vector<std::pair<std::string, float*>> _params{
		{ "min_mod", &min_mod },
		{ "max_mod", &max_mod },
		{ "mod_base", &mod_base },
		{ "prop_buffer", &prop_buffer },

		{ "total_prop_scaler", &total_prop_scaler },
		{ "total_prop_min", &total_prop_min },
		{ "total_prop_max", &total_prop_max },
		{ "total_prop_base", &total_prop_base },

		{ "split_hand_pool", &split_hand_pool },
		{ "split_hand_min", &split_hand_min },
		{ "split_hand_max", &split_hand_max },
		{ "split_hand_scaler", &split_hand_scaler },

		{ "jack_pool", &jack_pool },
		{ "jack_min", &jack_min },
		{ "jack_max", &jack_max },
		{ "jack_scaler", &jack_scaler },

		{ "decay_factor", &decay_factor },
	};
#pragma endregion params and param map

	float total_prop = 0.F;
	float jumptrill_prop = 0.F;
	float jack_prop = 0.F;
	float last_mod = min_mod;
	float pmod = min_mod;
	float t_taps = 0.F;

	void decay_mod()
	{
		pmod = std::clamp(last_mod - decay_factor, min_mod, max_mod);
		last_mod = pmod;
	}

	// inline void set_dbg(vector<float> doot[], const int& i)
	//{
	//	doot[HSS][i] = jumptrill_prop;
	//	doot[HSJ][i] = jack_prop;
	//}

	auto operator()(const metaItvInfo& mitvi) -> float
	{
		const auto& itvi = mitvi._itvi;

		// empty interval, don't decay mod or update last_mod
		if (itvi.total_taps == 0) {
			return neutral;
		}

		// look ma no hands
		if (itvi.taps_by_size.at(_tap_size) == 0) {
			decay_mod();
			return pmod;
		}

		t_taps = static_cast<float>(itvi.total_taps);

		// when bark of dog into canyon scream at you
		total_prop = total_prop_base +
					 (static_cast<float>((itvi.taps_by_size.at(_tap_size) +
										  itvi.mixed_hs_density_tap_bonus) +
										 prop_buffer) /
					  (t_taps - prop_buffer) * total_prop_scaler);
		total_prop =
		  std::clamp(fastsqrt(total_prop), total_prop_min, total_prop_max);

		// downscale jumptrills for hs as well
		jumptrill_prop = std::clamp(
		  split_hand_pool - (static_cast<float>(mitvi.not_hs) / t_taps),
		  split_hand_min,
		  split_hand_max);

		// downscale by jack density rather than upscale, like cj does
		jack_prop = std::clamp(
		  jack_pool - (static_cast<float>(mitvi.actual_jacks) / t_taps),
		  jack_min,
		  jack_max);

		pmod =
		  std::clamp(total_prop * jumptrill_prop * jack_prop, min_mod, max_mod);

		if (mitvi.dunk_it) {
			pmod *= 1.02557F;
		}

		// set last mod, we're using it to create a decaying mod that won't
		// result in extreme spikiness if files alternate between js and
		// hs/stream
		last_mod = pmod;

		return pmod;
	}
};
