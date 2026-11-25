/* Hidden-window acceptance build only. Drive the real setting widgets and
   handlers; close/reopen the screen to discard edits and verify saved values. */
static struct widget_instance *shadow_menu_spinner(struct widget_instance *widget)
{
	struct widget_instance *child, *found;
	struct pc_menu_setting *setting;
	char const *key = getenv("HALO_SHADOW_MENU_BOOLEAN");
	if (getenv("HALO_SHADOW_MENU_FRAME_BUDGET")) key = "display.shadow_frame_budget";
	if (!key) key = getenv("HALO_SHADOW_MENU_RESOLUTION") ? "display.shadow_resolution" : "display.modern_shadows";
	if (!widget) return NULL;
	setting = pc_menu_setting_get(widget->definition_tag_index);
	if (setting && !strcmp(setting->setting, key)) return widget;
	for (child = widget->child; child; child = child->next)
		if ((found = shadow_menu_spinner(child)) != NULL) return found;
	return NULL;
}

static void shadow_menu_action(struct widget_instance *widget, char const *action)
{
	long index;
	char const *name;
	boolean deleted = FALSE;
	for (index = 0; (name = pc_menu_function_name(index)) != NULL; index++)
		if (!strcmp(name, action))
		{
			pc_menu_event_function_invoke(widget, NULL, index, &deleted);
			return;
		}
	platform_log("SHADOW MENU missing action %s", action);
}

void modern_shadow_menu_accept(struct widget_instance *root)
{
	static unsigned long next;
	static int step;
	static int visited_lighting, alternate_index;
	struct widget_instance *spinner;
	struct widget_instance *child;
	unsigned long now;
	char name[300];
	if (!getenv("HALO_SHADOW_MENU_ACCEPT") || !root) return;
	spinner = shadow_menu_spinner(root);
	if (!spinner)
	{
		struct widget_instance *link = named(root, "op_shadows", 0);
		boolean deleted = FALSE;
		if (!visited_lighting && strstr(tag_get_name(root->definition_tag_index), "\\modern\\lighting\\settings"))
		{
			visited_lighting = TRUE;
			platform_log("SHADOW MENU visited lighting");
			shadow_menu_action(root, "port frontend back");
			return;
		}
		if (!visited_lighting && !link)
		{
			struct widget_instance *lighting = named(root, "row_link_8", 0);
			if (lighting && ui_widget_port_dispatch_event(lighting, 0, 0, &deleted)) return;
		}
		if (!link) link = named(root, "row_link_18", 0);
		if (!link)
		{
			link = named(root, "row_link_6", 0);
			if (link && ui_widget_port_dispatch_event(link, 0, 0, &deleted))
				platform_log("SHADOW MENU entered advanced graphics");
			return;
		}
		if (link && ui_widget_port_dispatch_event(link, 0, 0, &deleted))
			platform_log("SHADOW MENU entered from video");
		return;
	}
	now = system_milliseconds();
	if (now < next) return;
	next = now + 750;
	if (getenv("HALO_SHADOW_MENU_FRAME_BUDGET"))
	{
		if (getenv("HALO_SHADOW_MENU_VERIFY"))
		{
			platform_log("SHADOW MENU frame budget restart %ld", config_integer("display.shadow_frame_budget"));
			platform_request_quit();
			return;
		}
		if (step < 7)
		{
			spinner->parameters.list.selected_index = step;
			shadow_menu_action(spinner, "port settings save");
			platform_log("SHADOW MENU frame budget %ld", config_integer("display.shadow_frame_budget"));
		}
		else
		{
			spinner->parameters.list.selected_index = 4;
			shadow_menu_action(spinner, "port settings save");
			platform_request_quit();
		}
		step++;
		return;
	}
	if (getenv("HALO_SHADOW_MENU_BOOLEAN"))
	{
		char const *key = getenv("HALO_SHADOW_MENU_BOOLEAN");
		/* Measure the only additional per-view CPU operation in Shared. A batch
		   makes this visible to the millisecond clock in the isolated menu run. */
		if (step == 0 && !getenv("HALO_SHADOW_MENU_VERIFY") && !strcmp(key, "display.flashlight_extra_slot"))
		{
			unsigned long start = system_milliseconds();
			int total = 0;
			for (int i = 0; i < 100000; i++) total += config_boolean(key);
			platform_log("SHADOW MENU budget: 100000 reads in %lu ms (sum %d)", system_milliseconds() - start, total);
		}
		for (child = spinner; child->parent; child = child->parent)
			child->parent->focused_child = child;
		if (getenv("HALO_SHADOW_MENU_VERIFY"))
		{
			platform_log("SHADOW MENU flashlight restart %d selected %d", config_boolean(key), spinner->parameters.list.selected_index);
			platform_request_quit();
			return;
		}
		if (step == 0) alternate_index = 1 - spinner->parameters.list.selected_index;
		if (step == 1 || step == 3 || step == 7) spinner->parameters.list.selected_index = alternate_index;
		if (step == 4 || step == 6) shadow_menu_action(spinner, "port settings defaults");
		if (step == 3 || step == 6 || step == 7) shadow_menu_action(spinner, "port settings save");
		platform_log("SHADOW MENU flashlight %d saved %d selected %d", step, config_boolean(key), spinner->parameters.list.selected_index);
		if (step == 1 || step == 4)
		{
			snprintf(name, sizeof(name), "%s", tag_get_name(root->definition_tag_index));
			ui_widgets_close_all();
			ui_widget_load_by_name_or_tag(name, NONE, NULL, NONE, NONE, NONE, NONE);
		}
		if (step++ == 7) platform_request_quit();
		return;
	}
	if (getenv("HALO_SHADOW_MENU_INSPECT"))
	{
		/* Follow the normal focus chain so the frontend scrolls the new row
		   into view and supplies its help text through the real update. */
		for (child = spinner; child->parent; child = child->parent)
			child->parent->focused_child = child;
		spinner->parameters.list.selected_index = step / 3;
		platform_log("SHADOW MENU inspect %d", step);
		if (++step == (getenv("HALO_SHADOW_MENU_RESOLUTION") ? 18 : 9)) platform_request_quit();
		return;
	}
	if (getenv("HALO_SHADOW_MENU_VERIFY"))
	{
		if (getenv("HALO_SHADOW_MENU_RESOLUTION"))
			platform_log("SHADOW MENU resolution restart %ld selected %d", config_integer("display.shadow_resolution"), spinner->parameters.list.selected_index);
		else platform_log("SHADOW MENU restart %s selected %d", config_string("display.modern_shadows"), spinner->parameters.list.selected_index);
		platform_request_quit();
		return;
	}
	if (getenv("HALO_SHADOW_MENU_RESOLUTION"))
	{
		if (step == 1) spinner->parameters.list.selected_index = 5;
		if (step >= 3 && step <= 8)
		{
			spinner->parameters.list.selected_index = step - 3;
			shadow_menu_action(spinner, "port settings save");
		}
		if (step == 9 || step == 12) shadow_menu_action(spinner, "port settings defaults");
		if (step == 12) shadow_menu_action(spinner, "port settings save");
		if (step == 13)
		{
			spinner->parameters.list.selected_index = 3;
			shadow_menu_action(spinner, "port settings save");
		}
		platform_log("SHADOW MENU resolution step %d saved %ld selected %d modern %s", step,
			config_integer("display.shadow_resolution"), spinner->parameters.list.selected_index, config_string("display.modern_shadows"));
		if (step == 2 || step == 10)
		{
			snprintf(name, sizeof(name), "%s", tag_get_name(root->definition_tag_index));
			ui_widgets_close_all();
			ui_widget_load_by_name_or_tag(name, NONE, NULL, NONE, NONE, NONE, NONE);
		}
		if (step++ == 14) platform_request_quit();
		return;
	}
	if (step == 1 || step == 3) spinner->parameters.list.selected_index = 1;
	if (step == 4 || step == 8) spinner->parameters.list.selected_index = 2;
	if (step == 5 || step == 7) shadow_menu_action(spinner, "port settings defaults");
	if (step == 3 || step == 4 || step == 7 || step == 8) shadow_menu_action(spinner, "port settings save");
	platform_log("SHADOW MENU step %d saved %s selected %d lights %d %d", step,
		config_string("display.modern_shadows"), spinner->parameters.list.selected_index,
		config_boolean("display.plasma_lighting"), config_boolean("display.muzzle_lighting"));
	if (step == 2 || step == 6)
	{
		snprintf(name, sizeof(name), "%s", tag_get_name(root->definition_tag_index));
		ui_widgets_close_all();
		ui_widget_load_by_name_or_tag(name, NONE, NULL, NONE, NONE, NONE, NONE);
	}
	if (step++ == 9) platform_request_quit();
}
