#include "openxr/simula_monado_hud_timing.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <deque>
#include <mutex>
#if !defined(_WIN32)
#include <time.h>
#endif

namespace {

std::atomic<bool> g_monado_hud_timing_collection_active{false};
std::atomic<uint64_t> g_pending_monado_hud_godot_frame_start_ns{0};
std::mutex g_monado_hud_timing_sample_queue_mutex;
std::deque<SimulaOpenXRFrameTimingSample> g_monado_hud_timing_sample_queue;
uint64_t g_next_monado_hud_timing_sample_id = 1;

constexpr size_t MAX_QUEUED_TIMING_SAMPLES = 1024;
constexpr uint64_t MAX_REASONABLE_PRE_OPENXR_NS = 1000000000ULL;

void clear_timing_samples() {
	std::lock_guard<std::mutex> lock(g_monado_hud_timing_sample_queue_mutex);
	g_monado_hud_timing_sample_queue.clear();
}

} // namespace

extern "C" SIMULA_OPENXR_EXPORT void simula_openxr_debug_monado_hud_set_active(int active) {
	const bool should_be_active = active != 0;
	const bool was_active = g_monado_hud_timing_collection_active.exchange(should_be_active, std::memory_order_relaxed);

	if (was_active != should_be_active) {
		g_pending_monado_hud_godot_frame_start_ns.store(0, std::memory_order_relaxed);
		clear_timing_samples();
	}
}

extern "C" SIMULA_OPENXR_EXPORT void simula_openxr_debug_monado_hud_mark_godot_frame_start_now(void) {
	if (!simula_openxr_debug_monado_hud_is_active()) {
		return;
	}

	g_pending_monado_hud_godot_frame_start_ns.store(simula_openxr_debug_monado_hud_now_ns(), std::memory_order_relaxed);
}

extern "C" SIMULA_OPENXR_EXPORT uint32_t simula_openxr_debug_monado_hud_read_samples(
		SimulaOpenXRFrameTimingSample *out_samples,
		uint32_t max_samples) {
	if (out_samples == nullptr || max_samples == 0) {
		return 0;
	}

	std::lock_guard<std::mutex> lock(g_monado_hud_timing_sample_queue_mutex);
	const uint32_t count = std::min<uint32_t>(max_samples, static_cast<uint32_t>(g_monado_hud_timing_sample_queue.size()));
	for (uint32_t i = 0; i < count; i++) {
		out_samples[i] = g_monado_hud_timing_sample_queue.front();
		g_monado_hud_timing_sample_queue.pop_front();
	}
	return count;
}

bool simula_openxr_debug_monado_hud_is_active() {
	return g_monado_hud_timing_collection_active.load(std::memory_order_relaxed);
}

uint64_t simula_openxr_debug_monado_hud_now_ns() {
#if defined(_WIN32)
	const auto now = std::chrono::steady_clock::now().time_since_epoch();
	return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(now).count());
#else
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return static_cast<uint64_t>(now.tv_sec) * 1000000000ULL + static_cast<uint64_t>(now.tv_nsec);
#endif
}

uint64_t simula_openxr_debug_monado_hud_valid_godot_frame_start_ns(uint64_t before_xr_wait_frame_ns) {
	const uint64_t frame_start_ns = g_pending_monado_hud_godot_frame_start_ns.load(std::memory_order_relaxed);
	if (frame_start_ns == 0 || frame_start_ns > before_xr_wait_frame_ns) {
		return 0;
	}

	const uint64_t elapsed_ns = before_xr_wait_frame_ns - frame_start_ns;
	if (elapsed_ns > MAX_REASONABLE_PRE_OPENXR_NS) {
		return 0;
	}

	return frame_start_ns;
}

void simula_openxr_debug_monado_hud_record_sample(double godot_frame_start_to_xr_wait_frame_ms, double xr_wait_frame_ms) {
	if (!simula_openxr_debug_monado_hud_is_active()) {
		return;
	}

	std::lock_guard<std::mutex> lock(g_monado_hud_timing_sample_queue_mutex);
	if (g_monado_hud_timing_sample_queue.size() >= MAX_QUEUED_TIMING_SAMPLES) {
		g_monado_hud_timing_sample_queue.pop_front();
	}

	g_monado_hud_timing_sample_queue.push_back(
			SimulaOpenXRFrameTimingSample{
					g_next_monado_hud_timing_sample_id++,
					godot_frame_start_to_xr_wait_frame_ms,
					xr_wait_frame_ms,
			});
	g_pending_monado_hud_godot_frame_start_ns.store(0, std::memory_order_relaxed);
}
