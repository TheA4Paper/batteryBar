#include "gtk4-layer-shell.h"
#include <gtk/gtk.h>

GtkCssProvider* dynamic_provider;
GtkWidget* bar_w;
GtkLevelBar* bar;

static void load_css(){
	GtkCssProvider *provider = gtk_css_provider_new();
	dynamic_provider = gtk_css_provider_new();
	
	char *exe_path = g_file_read_link("/proc/self/exe", NULL);
    if (!exe_path) {
        g_warning("Could not determine executable path");
        return;
    }

    // Get the directory containing the executable
    char *exe_dir = g_path_get_dirname(exe_path);

    // Build path to CSS file
    char *css_path = g_build_filename(exe_dir, "style.css", NULL);

	GFile *css_file = g_file_new_for_path(css_path);

	gtk_css_provider_load_from_file(provider, css_file);

	gtk_style_context_add_provider_for_display(
    	gdk_display_get_default(),
	    GTK_STYLE_PROVIDER(provider),
    	GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	gtk_style_context_add_provider_for_display(
		gdk_display_get_default(),
		GTK_STYLE_PROVIDER(dynamic_provider),
		GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	g_object_unref(css_file);
	g_object_unref(provider);
}

static void update_css(double value, int state){
	char css_data[128];
	int red, green, blue;
	
	if(state == 1){
		red = 166;
		green = 255;
		blue = 218;
	}else if(state == 4){
		red = 0;
		green = 0;
		blue = 255;
	}else if(value < 0.5){
		red = 255;
		green = 255*value*2;
		blue = 0;
	}else{
		red = 255*(2-value*2);
		green = 255;
		blue = 0;
	}

	sprintf(css_data, ".battbar trough .filled {background-color: rgba(%d, %d, %d, 0.5);}", red, green, blue);

	gtk_css_provider_load_from_string(dynamic_provider, css_data);	
}

static gboolean update_bar(double value, int state){
	gtk_level_bar_set_value(bar, value/100);
	update_css(value/100, state);

	return G_SOURCE_CONTINUE;
}

static void on_battery_changed(GDBusProxy *proxy,
                      GVariant   *changed_properties,
                      GStrv       invalidated_properties,
                      gpointer    user_data)
{
	gboolean perc_changed = FALSE;
    gboolean state_changed = FALSE;
	
	if (g_variant_lookup_value(changed_properties, "Percentage", NULL)) {
        perc_changed = TRUE;
    }

    if (g_variant_lookup_value(changed_properties, "State", NULL)) {
        state_changed = TRUE;
    }

	double value = 1000;
	double state = 1000;
	if (state_changed || perc_changed) {
		GVariant *perc = g_dbus_proxy_get_cached_property(proxy, "Percentage");
        if (perc) {
            value = g_variant_get_double(perc);
            g_variant_unref(perc);
        }

        GVariant *gstate = g_dbus_proxy_get_cached_property(proxy, "State");
        if (gstate) {
            state = g_variant_get_uint32(gstate);
            g_variant_unref(gstate);
        }
		update_bar(value, state);
	}	
}

static void init_dbus_proxy(){
	GError *err = NULL;
    GDBusProxy *proxy = g_dbus_proxy_new_for_bus_sync(
        G_BUS_TYPE_SYSTEM,
        G_DBUS_PROXY_FLAGS_NONE,
        NULL, // GDBusInterfaceInfo
        "org.freedesktop.UPower",
        "/org/freedesktop/UPower/devices/battery_BATT",
        "org.freedesktop.UPower.Device",
        NULL,
        &err);

    if (!proxy) {
        g_printerr("Failed to connect to UPower: %s\n", err->message);
        g_error_free(err);
        return;
    }

	GVariant *val = g_dbus_proxy_get_cached_property(proxy, "Percentage");
	gdouble value = 0.0;
	guint32 state = 0;
    if (val) {
        value = g_variant_get_double(val);
        g_variant_unref(val);
    }

	GVariant *gstate = g_dbus_proxy_get_cached_property(proxy, "State");
    if (gstate) {
        state = g_variant_get_uint32(gstate);
        g_variant_unref(gstate);
    }

	update_bar(value, state);

	g_signal_connect(proxy, "g-properties-changed", G_CALLBACK(on_battery_changed), NULL);
}

static void activate(GtkApplication* app, void* _data) { 
    GtkWindow* gtk_window = GTK_WINDOW(gtk_application_window_new(app));
	load_css();
	gtk_window_set_default_size(gtk_window, -1, 1);

    gtk_layer_init_for_window(gtk_window);

    gtk_layer_set_layer(gtk_window, GTK_LAYER_SHELL_LAYER_OVERLAY);

	gtk_layer_set_exclusive_zone(gtk_window,0);
	//gtk_layer_auto_exclusive_zone_enable(gtk_window);

    gtk_layer_set_anchor(gtk_window, GTK_LAYER_SHELL_EDGE_TOP, true);
    gtk_layer_set_anchor(gtk_window, GTK_LAYER_SHELL_EDGE_LEFT, true);
    gtk_layer_set_anchor(gtk_window, GTK_LAYER_SHELL_EDGE_RIGHT, true);

	bar_w = gtk_level_bar_new();
	gtk_widget_add_css_class(bar_w, "battbar");
	bar = GTK_LEVEL_BAR(bar_w);

	gtk_level_bar_set_value(bar, 0.5);
	gtk_window_set_child(gtk_window, bar_w);

	init_dbus_proxy();

    gtk_window_present(gtk_window);
	
	GdkSurface *surface = gtk_native_get_surface(GTK_NATIVE(gtk_window));
	cairo_region_t *empty_region = cairo_region_create();
	gdk_surface_set_input_region(surface, empty_region);
	cairo_region_destroy(empty_region);
}

int main(int argc, char **argv) {
    GtkApplication* app = gtk_application_new("me.flatandblank.batterybar", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
