/* Automatic CPU/presentation interval correlation; no GPU timers or waits.
Included by texture_cache_test.h after its logger. Counters describe work
between consecutive presents, including loading, pacing and diagnostics. */

struct android_texture_perf_counters
{
	unsigned long retries, failed, fallback, recoveries;
	unsigned long compact_ms;
};
static struct android_texture_perf_counters android_texture_perf_pressure;
static struct android_texture_perf_counters android_texture_perf_previous;
static Uint64 android_texture_perf_last, android_texture_perf_next_detail, android_texture_perf_next_window;
static Uint64 android_texture_perf_upload_ms, android_texture_perf_window_ms, android_texture_perf_max;
static unsigned long android_texture_perf_uploads, android_texture_perf_frames, android_texture_perf_slow;
static BOOL android_texture_perf_started;

void android_texture_test_pressure(unsigned long retries, unsigned long failed, unsigned long fallback,
	unsigned long recoveries, unsigned long elapsed)
{
	android_texture_perf_pressure.retries += retries;
	android_texture_perf_pressure.failed += failed;
	android_texture_perf_pressure.fallback += fallback;
	android_texture_perf_pressure.recoveries += recoveries;
	android_texture_perf_pressure.compact_ms += elapsed;
}

static void android_texture_perf_frame(unsigned long frame, Uint64 now)
{
	Uint64 interval, upload_ms;
	unsigned long uploads, retries, failed, fallback, recovered, compact_ms;

	if (!android_texture_perf_started)
	{
		android_texture_perf_started = TRUE;
		android_texture_perf_last = now;
		android_texture_perf_next_window = now + 10000;
	}
	else
	{
		interval = now - android_texture_perf_last;
		uploads = android_texture_test_uploads - android_texture_perf_uploads;
		upload_ms = android_texture_test_upload_ms - android_texture_perf_upload_ms;
		retries = android_texture_perf_pressure.retries - android_texture_perf_previous.retries;
		failed = android_texture_perf_pressure.failed - android_texture_perf_previous.failed;
		fallback = android_texture_perf_pressure.fallback - android_texture_perf_previous.fallback;
		recovered = android_texture_perf_pressure.recoveries - android_texture_perf_previous.recoveries;
		compact_ms = android_texture_perf_pressure.compact_ms - android_texture_perf_previous.compact_ms;
		android_texture_perf_frames++;
		android_texture_perf_slow += interval >= 34;
		android_texture_perf_window_ms += interval;
		if (interval > android_texture_perf_max) android_texture_perf_max = interval;
		if (now >= android_texture_perf_next_detail && (interval >= 34 || recovered || upload_ms >= 8))
		{
			android_texture_test_log("frame_cost: ms=%llu frame=%lu interval_ms=%llu uploads=%lu upload_CPU_ms=%llu "
				"retries=%lu failed=%lu fallback=%lu recovered=%lu recovery_ms=%lu",
				(unsigned long long)now, frame - 1, (unsigned long long)interval, uploads,
				(unsigned long long)upload_ms, retries, failed, fallback, recovered, compact_ms);
			android_texture_perf_next_detail = now + 1000;
		}
		if (now >= android_texture_perf_next_window)
		{
			android_texture_test_log("frame_window: ms=%llu frames=%lu interval_sum_ms=%llu max_interval_ms=%llu "
				"intervals_over_33ms=%lu (includes loading, pacing and diagnostics; not GPU timing)",
				(unsigned long long)now, android_texture_perf_frames, (unsigned long long)android_texture_perf_window_ms,
				(unsigned long long)android_texture_perf_max, android_texture_perf_slow);
			android_texture_perf_frames = android_texture_perf_slow = 0;
			android_texture_perf_window_ms = android_texture_perf_max = 0;
			android_texture_perf_next_window = now + 10000;
		}
	}
	android_texture_perf_last = now;
	android_texture_perf_uploads = android_texture_test_uploads;
	android_texture_perf_upload_ms = android_texture_test_upload_ms;
	android_texture_perf_previous = android_texture_perf_pressure;
}
