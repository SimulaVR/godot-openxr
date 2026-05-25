#ifndef SIMULA_MONADO_HUD_TIMING_H
#define SIMULA_MONADO_HUD_TIMING_H

#include <stdint.h>

#if defined(_WIN32)
#define SIMULA_OPENXR_EXPORT __declspec(dllexport)
#else
#define SIMULA_OPENXR_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SimulaOpenXRFrameTimingSample {
	uint64_t sample_id;
	double godot_frame_start_to_xr_wait_frame_ms;
	double xr_wait_frame_ms;
} SimulaOpenXRFrameTimingSample;

SIMULA_OPENXR_EXPORT void simula_openxr_debug_monado_hud_set_active(int active);
SIMULA_OPENXR_EXPORT void simula_openxr_debug_monado_hud_mark_godot_frame_start_now(void);
SIMULA_OPENXR_EXPORT uint32_t simula_openxr_debug_monado_hud_read_samples(
		SimulaOpenXRFrameTimingSample *out_samples,
		uint32_t max_samples);

#ifdef __cplusplus
}

bool simula_openxr_debug_monado_hud_is_active();
uint64_t simula_openxr_debug_monado_hud_now_ns();
uint64_t simula_openxr_debug_monado_hud_valid_godot_frame_start_ns(uint64_t before_xr_wait_frame_ns);
void simula_openxr_debug_monado_hud_record_sample(double godot_frame_start_to_xr_wait_frame_ms, double xr_wait_frame_ms);
#endif

#endif // SIMULA_MONADO_HUD_TIMING_H
