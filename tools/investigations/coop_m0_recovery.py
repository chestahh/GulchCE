"""Generate non-shipping real-engine M0 recovery observations and controls."""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
import coop_m0_trace as trace

OUTPUT = trace.ROOT / 'build/m0/recovery/generated'
trace.OUTPUT = OUTPUT

HUD = r'''
void m0_recovery_hud(void)
{
    struct scenario *scenario = global_scenario_get();
    long index, found = NONE;
    if (hud_messaging_globals && scenario->hud_messages.index != NONE)
    {
        struct hud_message_text_definition *messages = HUD_MESSAGE_TEXT_DEFINITION_GET(scenario->hud_messages.index);
        for (index = 0; index < messages->messages.count; index++)
            if (hud_messaging_globals->objective.message == TAG_BLOCK_GET_ELEMENT(&messages->messages, index, struct hud_state_message_definition))
                found = index;
    }
    m0_emit("objective", game_time_get(), found, 0, 0, 0, 0);
}
void m0_recovery_objective(short ordinal)
{
    struct scenario *scenario = global_scenario_get();
    long index;
    short seen = 0;
    if (scenario->hud_messages.index != NONE)
    {
        struct hud_message_text_definition *messages = HUD_MESSAGE_TEXT_DEFINITION_GET(scenario->hud_messages.index);
        for (index = 0; index < messages->messages.count; index++)
        {
            struct hud_state_message_definition *message = TAG_BLOCK_GET_ELEMENT(&messages->messages, index, struct hud_state_message_definition);
            if (message->element_count == 1 && message->element_start_index >= 0 && message->element_start_index < messages->elements.count)
            {
                struct hud_state_message_element *element = TAG_BLOCK_GET_ELEMENT(&messages->elements, message->element_start_index, struct hud_state_message_element);
                if (element->type == _hud_message_type_text && seen++ == ordinal)
                {
                    scripted_hud_set_objective((short)index);
                    m0_emit("objective_fixture", game_time_get(), ordinal, index, 0, 0, 0);
                    return;
                }
            }
        }
    }
    m0_emit("objective_unavailable", game_time_get(), ordinal, 0, 0, 0, 0);
}
'''

NAV = r'''
void m0_recovery_nav(void)
{
    short local, index;
    if (!nav_point_data) return;
    for (local = 0; local < MAXIMUM_NUMBER_OF_LOCAL_PLAYERS; local++)
    {
        long player = local_player_get_player_index(local);
        if (player == NONE) continue;
        m0_emit("nav_player", game_time_get(), player, local, 0, 0, 0);
        for (index = 0; index < MAXIMUM_NUMBER_OF_NAV_POINTS; index++)
        {
            struct hud_nav_point_datum *nav = &nav_point_data[local].nav_points[index];
            if (nav->type != NONE)
                m0_emit("nav", game_time_get(), player, nav->type, nav->reference_index, nav->nav_index, nav->z_offset);
        }
    }
}
'''


def instrument(relative, original, domains=False):
    name = Path(relative).stem
    if name in ('hud_messaging', 'hud_nav_points'):
        return trace.HEADER.replace('TRACE_NAME', name) + original + (HUD if name == 'hud_messaging' else NAV)
    text = trace.instrument(relative, original)
    if name == 'network_coop':
        text = trace.once(text, '\tcheckpoint_players.valid = FALSE;\n\tcheckpoint_players.count = 0;',
                          '\tm0_emit("checkpoint_saved", game_time_get(), coop_host(), 0, 0, 0, 0);\n\tcheckpoint_players.valid = FALSE;\n\tcheckpoint_players.count = 0;')
        text = trace.once(text, 'void network_coop_host_tick(',
                          'static void m0_recovery_capture(void);\nstatic void m0_recovery_fixture(void);\n\nvoid network_coop_host_tick(')
        text = trace.once(text, '\thost_send_events();\n}', '\thost_send_events();\n'
            '\tm0_recovery_fixture();\n\tif (game_time_get() % 30 == 0) m0_recovery_capture();\n}')
        text = trace.once(text, '\tclient_play_presentation();', '\tclient_play_presentation();\n'
                          '\tif (game_time_get() % 30 == 0) m0_recovery_capture();')
        text = trace.once(text, '\tclient_apply_event(&events[index]);',
            '\tif (getenv("M0_DROP_OBJECTIVE") && events[index].kind == _coop_event_hud && events[index].type == _coop_hud_objective)\n'
            '\t{\n\t\tm0_emit("fault_drop_objective", game_time_get(), events[index].number, events[index].value, 0, 0, 0);\n\t\tcontinue;\n\t}\n'
            '\tclient_apply_event(&events[index]);')
        text += '\n' + Path(__file__).with_suffix('.c').read_text()
        if domains:
            text = trace.once(text, 'static void m0_recovery_capture(void);',
                              'static void m0_domains_capture(void);\nstatic void m0_domains_fixture(void);\nstatic void m0_recovery_capture(void);')
            text = trace.once(text, '\tm0_recovery_epoch();', '\tm0_domains_capture();\n\tm0_recovery_epoch();')
            text = trace.once(text, '\tif (!getenv("M0_RECOVERY") || !coop_host()) return;',
                              '\tif (getenv("M0_RECOVERY_DOMAINS")) { m0_domains_fixture(); return; }\n\tif (!getenv("M0_RECOVERY") || !coop_host()) return;')
            text += '\n' + Path(__file__).with_name('coop_m0_recovery_domains.c').read_text()
    elif name == 'network_distributed':
        text += '\nvoid m0_recovery_epoch(void)\n{\n\tm0_emit("epoch", game_time_get(), distributed_checkpoint_epoch, distributed_checkpoint_ready(), 0, 0, 0);\n}\n'
    return text


def generate(domains=False):
    OUTPUT.mkdir(parents=True, exist_ok=True)
    manifest = {'revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=trace.ROOT, text=True).strip(), 'files': {}}
    paths = ['port/windows/src/win32_net.c', 'port/linux/game/network_objects.c',
             'port/linux/game/network_distributed.c', 'port/linux/game/network_coop.c',
             'port/linux/game/network_test.c', 'source/interface/hud_messaging.c', 'source/interface/hud_nav_points.c']
    for relative in paths:
        source = trace.ROOT / relative
        original = source.read_bytes()
        target = OUTPUT / source.name
        target.write_text(instrument(relative, original.decode('utf8').replace('\r\n', '\n'), domains), encoding='utf8')
        manifest['files'][relative] = {'source_sha256': hashlib.sha256(original).hexdigest(),
            'copy': str(target), 'copy_sha256': hashlib.sha256(target.read_bytes()).hexdigest()}
    (OUTPUT / 'manifest.json').write_text(json.dumps(manifest, indent=2) + '\n')
    mode = ' --domains' if domains else ''
    (OUTPUT / 'compile.cmd').write_text(f'@echo off\n"{sys.executable}" "{Path(__file__).resolve()}"{mode} compile %*\n')
    print(OUTPUT / 'compile.cmd')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--domains', action='store_true', help='Additional persistent-state mutations; separate build artifacts')
    parser.add_argument('arguments', nargs=argparse.REMAINDER)
    args = parser.parse_args()
    if args.domains:
        OUTPUT = trace.ROOT / 'build/m0/recovery-domains/generated'
        trace.OUTPUT = OUTPUT
    if args.arguments:
        if args.arguments[0] != 'compile':
            parser.error('Expected compile or no positional arguments')
        sys.exit(trace.compile_source(args.arguments[1:]))
    generate(args.domains)
