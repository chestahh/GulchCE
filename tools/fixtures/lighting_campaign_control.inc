/* Test-build-only controller input for reaching a real campaign checkpoint.
   No teleportation, actor creation, mission-script changes or save conversion.
   Each command expires; a disconnected controller leaves the player still.
   The control path uses the game's file API: pass a data-root-relative name
   such as control.txt, not a Windows drive path (interpreted as an Xbox drive). */
static int campaign_route_input(XINPUT_GAMEPAD *pad)
{
	static int sequence = -1, lx, ly, rx, ry, buttons, rt, lt;
	static double expires, polled;
	char const *path = getenv("HALO_LIGHTING_ROUTE_CONTROL");
	double now = clock_seconds();
	FILE *file;
	int next, milliseconds, a, b, c, d, e, f, g;
	if (!path) return FALSE;
	if (now - polled > 0.02)
	{
		polled = now;
		file = fopen(path, "r");
		if (file)
		{
			if (fscanf(file, "%d %d %d %d %d %d %d %d %d", &next, &milliseconds,
				&a, &b, &c, &d, &e, &f, &g) == 9 && next != sequence)
			{
				sequence = next;
				expires = now + PIN(milliseconds, 0, 10000) * 0.001;
				lx = PIN(a, -32767, 32767); ly = PIN(b, -32767, 32767);
				rx = PIN(c, -32767, 32767); ry = PIN(d, -32767, 32767);
				buttons = e; rt = PIN(f, 0, 255); lt = PIN(g, 0, 255);
			}
			fclose(file);
		}
	}
	if (now < expires)
	{
		pad->sThumbLX = lx; pad->sThumbLY = ly; pad->sThumbRX = rx; pad->sThumbRY = ry;
		pad->bAnalogButtons[XINPUT_GAMEPAD_A] = buttons & 1 ? 255 : 0;
		pad->bAnalogButtons[XINPUT_GAMEPAD_B] = buttons & 2 ? 255 : 0;
		pad->bAnalogButtons[XINPUT_GAMEPAD_X] = buttons & 4 ? 255 : 0;
		pad->bAnalogButtons[XINPUT_GAMEPAD_Y] = buttons & 8 ? 255 : 0;
		if (buttons & 16) pad->wButtons |= XINPUT_GAMEPAD_START;
		pad->bAnalogButtons[XINPUT_GAMEPAD_RIGHT_TRIGGER] = (BYTE)rt;
		pad->bAnalogButtons[XINPUT_GAMEPAD_LEFT_TRIGGER] = (BYTE)lt;
	}
	return TRUE;
}

static void campaign_route_state(void)
{
	static double previous;
	double now = clock_seconds();
	struct observer_result const *camera;
	FILE *file;
	if (!getenv("HALO_LIGHTING_ROUTE_CONTROL") || now - previous < 0.5) return;
	previous = now;
	camera = observer_get_camera(0);
	if (!camera) return;
	file = fopen("route-state.txt", "w");
	if (!file) return;
	fprintf(file, "tick %ld\nposition %.6f %.6f %.6f\nforward %.6f %.6f %.6f\n",
		game_time_get(), camera->position.x, camera->position.y, camera->position.z,
		camera->forward.i, camera->forward.j, camera->forward.k);
	fclose(file);
}

/* GulchCE graphics contributions by chestahh: AGPL-3.0-or-later OR GPL-3.0-or-later.
   This grant covers chestahh's lighting, shadow and renderer changes in this file.
   Retain this credit and provenance when redistributing copies or modifications.
   Original GulchCE source: https://github.com/chestahh/GulchCE/tree/4314362cba8099cdcdad06ac10e7c060d0202dcc */
