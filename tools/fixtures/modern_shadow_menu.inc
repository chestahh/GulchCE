/* Hidden-window acceptance build only. Drive the real setting widgets and
   handlers; close/reopen the screen to discard edits and verify saved values. */
static struct widget_instance *shadow_menu_spinner(struct widget_instance *widget)
{
	struct widget_instance *child, *found;
	struct pc_menu_setting *setting;
	if (!widget) return NULL;
	setting = pc_menu_setting_get(widget->definition_tag_index);
	if (setting && !strcmp(setting->setting, "display.modern_shadows")) return widget;
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
	struct widget_instance *spinner;
	struct widget_instance *child;
	unsigned long now;
	char name[300];
	if (!getenv("HALO_SHADOW_MENU_ACCEPT") || !(spinner = shadow_menu_spinner(root))) return;
	now = system_milliseconds();
	if (now < next) return;
	next = now + 750;
	if (getenv("HALO_SHADOW_MENU_INSPECT"))
	{
		/* Follow the normal focus chain so the frontend scrolls the new row
		   into view and supplies its help text through the real update. */
		for (child = spinner; child->parent; child = child->parent)
			child->parent->focused_child = child;
		spinner->parameters.list.selected_index = step / 3;
		platform_log("SHADOW MENU inspect %d", step);
		if (++step == 9) platform_request_quit();
		return;
	}
	if (getenv("HALO_SHADOW_MENU_VERIFY"))
	{
		platform_log("SHADOW MENU restart %s selected %d", config_string("display.modern_shadows"), spinner->parameters.list.selected_index);
		platform_request_quit();
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
