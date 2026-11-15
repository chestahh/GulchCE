"""Private campaign-entry observations and bounded player input, never shipped."""
import hashlib
import json
from pathlib import Path
import subprocess
import sys

import coop_m0_trace as trace
import coop_m0_world as world

OUTPUT = trace.ROOT / 'build/m0/campaign-entry/generated'
trace.OUTPUT = OUTPUT

# Supply the same input blob consumed by the ordinary player-control path.
# No pose, mission flag, tutorial action flag or device state is assigned.
INPUT = r'''
	/* port: private M0 input fixture in a generated copy only. */
	if (local_player_index == 0 && getenv("M0_CAMPAIGN_INPUT"))
	{
		static long previous;
		static double started;
		FILE *file = fopen(getenv("M0_CAMPAIGN_INPUT"), "r");
		long sequence, action, buttons;
		double duration, forward, strafe, yaw, pitch;
		if (file)
		{
			if (fscanf(file, "%ld %lf %lf %lf %lf %lf %ld %ld", &sequence, &duration,
				&forward, &strafe, &yaw, &pitch, &action, &buttons) == 8 && sequence > 0 &&
				duration > 0 && duration <= 5 && forward >= -1 && forward <= 1 &&
				strafe >= -1 && strafe <= 1 && yaw >= -3 && yaw <= 3 &&
				pitch >= -3 && pitch <= 3 && (action == 0 || action == 1) && buttons >= 0 && buttons <= 31)
			{
				if (sequence > previous)
				{
					previous = sequence;
					started = coop_m0_clock();
					m0_emit("input_request", game_time_get(), sequence, duration, forward, strafe, action);
					m0_emit("input_look", game_time_get(), sequence, yaw, pitch, player_index, local_player_index);
					m0_emit("input_buttons", game_time_get(), sequence, buttons, player_index, local_player_index, 0);
				}
				if (sequence == previous && coop_m0_clock() - started < duration)
				{
					input->throttle.i = (real)forward;
					input->throttle.j = (real)strafe;
					if (magnitude_squared2d(&input->throttle) > 1.f)
						scale_vector2d(&input->throttle, 1.f / square_root(magnitude_squared2d(&input->throttle)), &input->throttle);
					input->facing_delta.yaw = (real)yaw * time_delta_sec;
					input->facing_delta.pitch = (real)pitch * time_delta_sec;
					SET_FLAG(input->unit_control_flags, _unit_control_action_bit, action);
					/* Private request bits: accept, back, jump, crouch, fire. */
					input->accept = (buttons & 1) != 0;
					input->back = (buttons & 2) != 0;
					SET_FLAG(input->unit_control_flags, _unit_control_jump_bit, (buttons & 4) != 0);
					SET_FLAG(input->unit_control_flags, _unit_control_crouch_modifier_bit, (buttons & 8) != 0);
					input->primary_trigger = (buttons & 16) ? 1.f : 0.f;
				}
			}
			fclose(file);
		}
	}
'''


def instrument(relative, original):
    name = Path(relative).stem
    if name == 'player_control':
        text = trace.HEADER.replace('TRACE_NAME', name) + original
        anchor = '\tplayer_control_action_test_check_reset_input_blob(input);'
        return trace.once(text, anchor, INPUT + anchor +
            '\n\tif (getenv("M0_CAMPAIGN_INPUT") && (input->accept || input->back))\n'
            '\t\tm0_emit("input_confirmation", game_time_get(), player_index, input->accept, input->back, player_control_globals->action_flags, 0);')
    if name == 'game':
        text = trace.HEADER.replace('TRACE_NAME', name) + original
        return trace.once(text, '\treturn player_spawn_count > 1;',
            '\tm0_emit("cooperative_query", 0, player_spawn_count, game_connection(), player_spawn_count > 1, 0, 0);\n\treturn player_spawn_count > 1;')
    if name == 'hs_runtime':
        text = trace.HEADER.replace('TRACE_NAME', name) + original
        anchor = '\tthread = datum_get(hs_thread_data, thread_index);\n\tif (thread->sleep_until != NONE)'
        return trace.once(text, anchor,
            '\tthread = datum_get(hs_thread_data, thread_index);\n'
            '\tm0_emit("script_wake", game_time_get(), thread_index, thread->script_index, thread->sleep_until, 0, 0);\n'
            '\tif (thread->sleep_until != NONE)')
    text = world.instrument(relative, original)
    if name == 'network_test':
        # Pause is polled from raw controller state, before menu-event dispatch.
        # Use the existing test controller for a bounded press; never invoke a
        # pause/restart handler or alter widget/session state from the fixture.
        text = 'extern double coop_m0_clock(void);\n' + text
        text = trace.once(text, '\t\tstatic long previous;',
            '\t\tstatic long previous;\n\t\tstatic double pause_until;\n'
            '\t\tif (getenv("M0_RAW_PAUSE"))\n'
            '\t\t\ttest_input_press(coop_m0_clock() < pause_until ? (1UL << HALO_CHAT_PAD_START) : 0);')
        text = trace.once(text, '\t\t\t\t\tevent_manager_post_button(controller, button);',
            '\t\t\t\t\tif (getenv("M0_RAW_PAUSE") && controller == 0 && button == _gamepad_binary_button_start)\n'
            '\t\t\t\t\t\tpause_until = coop_m0_clock() + .25;\n'
            '\t\t\t\t\telse\n'
            '\t\t\t\t\t\tevent_manager_post_button(controller, button);')
    if name == 'players':
        text = trace.once(text, '\tplayers_globals->input_disabled = !enable;',
            '\tm0_emit("input_enable", game_time_get(), enable, players_globals->input_disabled, 0, 0, 0);\n\tplayers_globals->input_disabled = !enable;')
    return text


def generate():
    OUTPUT.mkdir(parents=True, exist_ok=True)
    paths = ['port/windows/src/win32_net.c', 'port/linux/game/network_objects.c',
        'port/linux/game/network_distributed.c', 'port/linux/game/network_coop.c',
        'port/linux/game/network_test.c', 'source/interface/hud_messaging.c',
        'source/interface/hud_nav_points.c', 'source/scenario/scenario.c',
        'source/game/players.c', 'source/devices/devices.c', 'source/hs/hs_library_external.c',
        'source/game/player_control.c', 'source/game/game.c', 'source/hs/hs_runtime.c']
    manifest = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], text=True).strip(), 'files': {}}
    for relative in paths:
        original = (trace.ROOT / relative).read_bytes()
        target = OUTPUT / Path(relative).name
        target.write_text(instrument(relative, original.decode('utf-8').replace('\r\n', '\n')), encoding='utf-8')
        manifest['files'][relative] = {'source_sha256': hashlib.sha256(original).hexdigest(),
            'copy': str(target), 'copy_sha256': hashlib.sha256(target.read_bytes()).hexdigest()}
    (OUTPUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    (OUTPUT / 'compile.cmd').write_text(f'@echo off\n"{sys.executable}" "{Path(__file__).resolve()}" compile %*\n')


if __name__ == '__main__':
    if len(sys.argv) > 1:
        if sys.argv[1] != 'compile':
            raise SystemExit('Expected compile or no arguments')
        sys.exit(trace.compile_source(sys.argv[2:]))
    generate()
