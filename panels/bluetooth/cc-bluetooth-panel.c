/*
 *
 *  Copyright (C) 2013  Bastien Nocera <hadess@hadess.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <glib/gi18n-lib.h>
#include <libcinnamon-control-center/cc-shell.h>
#include <bluetooth-settings-widget.h>

#include "cc-bluetooth-panel.h"
#include "cc-bluetooth-resources.h"


CC_PANEL_REGISTER (CcBluetoothPanel, cc_bluetooth_panel)

#define BLUETOOTH_PANEL_PRIVATE(o) (G_TYPE_INSTANCE_GET_PRIVATE ((o), CC_TYPE_BLUETOOTH_PANEL, CcBluetoothPanelPrivate))

#define WID(s) GTK_WIDGET (gtk_builder_get_object (self->priv->builder, s))

#define BLUETOOTH_DISABLED_PAGE      "disabled-page"
#define BLUETOOTH_HW_DISABLED_PAGE   "hw-disabled-page"
#define BLUETOOTH_NO_DEVICES_PAGE    "no-devices-page"
#define BLUETOOTH_WORKING_PAGE       "working-page"

struct CcBluetoothPanelPrivate {
	GtkBuilder          *builder;
	GtkWidget           *stack;
	GtkWidget           *widget;
	GCancellable        *cancellable;

	GtkWidget           *main_box;	

	/* Killswitch */
	GtkWidget           *kill_switch_header;
	GDBusProxy          *rfkill, *properties;
	gboolean             airplane_mode;
	gboolean             hardware_airplane_mode;
	gboolean             has_airplane_mode;
};

static void cc_bluetooth_panel_finalize (GObject *object);
static void cc_bluetooth_panel_constructed (GObject *object);

static const char *
cc_bluetooth_panel_get_help_uri (CcPanel *panel)
{
	return "help:gnome-help/bluetooth";
}

static void
cc_bluetooth_panel_class_init (CcBluetoothPanelClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	CcPanelClass *panel_class = CC_PANEL_CLASS (klass);

	object_class->constructed = cc_bluetooth_panel_constructed;
	object_class->finalize = cc_bluetooth_panel_finalize;

	panel_class->get_help_uri = cc_bluetooth_panel_get_help_uri;

	g_type_class_add_private (klass, sizeof (CcBluetoothPanelPrivate));
}

static void
cc_bluetooth_panel_finalize (GObject *object)
{
	CcBluetoothPanel *self;

	self = CC_BLUETOOTH_PANEL (object);

	g_cancellable_cancel (self->priv->cancellable);
	g_clear_object (&self->priv->cancellable);

	g_clear_object (&self->priv->properties);
	g_clear_object (&self->priv->rfkill);
	g_clear_object (&self->priv->kill_switch_header);

	G_OBJECT_CLASS (cc_bluetooth_panel_parent_class)->finalize (object);
}

static void
cc_bluetooth_panel_constructed (GObject *object)
{
	CcBluetoothPanel *self = CC_BLUETOOTH_PANEL (object);

	G_OBJECT_CLASS (cc_bluetooth_panel_parent_class)->constructed (object);


}

static void
power_callback (GObject          *object,
		GParamSpec       *spec,
		CcBluetoothPanel *self)
{
	gboolean state;

	state = gtk_switch_get_active (GTK_SWITCH (WID ("switch_bluetooth")));
	g_debug ("Power switched to %s", state ? "on" : "off");
	g_dbus_proxy_call (self->priv->properties,
			   "Set",
			   g_variant_new_parsed ("('org.cinnamon.SettingsDaemon.Rfkill', 'BluetoothAirplaneMode', %v)",
						 g_variant_new_boolean (!state)),
			   G_DBUS_CALL_FLAGS_NONE,
			   -1,
			   self->priv->cancellable,
			   NULL, NULL);
}

static void
cc_bluetooth_panel_update_power (CcBluetoothPanel *self)
{
	GObject *toggle;
	gboolean sensitive, powered;
	const char *page;

	g_debug ("Updating airplane mode: has_airplane_mode %d, hardware_airplane_mode %d, airplane_mode %d",
		 self->priv->has_airplane_mode, self->priv->hardware_airplane_mode, self->priv->airplane_mode);

	if (self->priv->has_airplane_mode == FALSE) {
		g_debug ("No Bluetooth available");
		sensitive = FALSE;
		powered = FALSE;
		page = BLUETOOTH_NO_DEVICES_PAGE;
	} else if (self->priv->hardware_airplane_mode) {
		g_debug ("Bluetooth is Hard blocked");
		sensitive = FALSE;
		powered = FALSE;
		page = BLUETOOTH_HW_DISABLED_PAGE;
	} else if (self->priv->airplane_mode) {
		g_debug ("Default adapter is unpowered, but should be available");
		sensitive = TRUE;
		powered = FALSE;
		page = BLUETOOTH_DISABLED_PAGE;
	} else {
		g_debug ("Bluetooth is available and powered");
		sensitive = TRUE;
		powered = TRUE;
		page = BLUETOOTH_WORKING_PAGE;
	}

	gtk_widget_set_sensitive (WID ("box_power") , sensitive);

	toggle = G_OBJECT (WID ("switch_bluetooth"));
	g_signal_handlers_block_by_func (toggle, power_callback, self);
	gtk_switch_set_active (GTK_SWITCH (toggle), powered);
	g_signal_handlers_unblock_by_func (toggle, power_callback, self);

	gtk_stack_set_visible_child_name (GTK_STACK (self->priv->stack), page);
}

static void
airplane_mode_changed (GDBusProxy       *proxy,
		       GVariant         *changed_properties,
		       GStrv             invalidated_properties,
		       CcBluetoothPanel *self)
{
	GVariant *v;

	v = g_dbus_proxy_get_cached_property (self->priv->rfkill, "BluetoothAirplaneMode");
	self->priv->airplane_mode = g_variant_get_boolean (v);
	g_variant_unref (v);

	v = g_dbus_proxy_get_cached_property (self->priv->rfkill, "BluetoothHardwareAirplaneMode");
	self->priv->hardware_airplane_mode = g_variant_get_boolean (v);
	g_variant_unref (v);

	v = g_dbus_proxy_get_cached_property (self->priv->rfkill, "BluetoothHasAirplaneMode");
	self->priv->has_airplane_mode = g_variant_get_boolean (v);
	g_variant_unref (v);

	cc_bluetooth_panel_update_power (self);
}

static void
add_stack_page (CcBluetoothPanel *self,
		const char       *message,
		const char       *name)
{
	GtkWidget *label;

	label = gtk_label_new (message);
	gtk_stack_add_named (GTK_STACK (self->priv->stack), label, name);
	gtk_widget_show (label);
}

static void
panel_changed (GtkWidget        *settings_widget,
	       const char       *panel,
	       CcBluetoothPanel *self)
{
	CcShell *shell;
	GError *error = NULL;

	// shell = cc_panel_get_shell (CC_PANEL (self));
	// if (cc_shell_set_active_panel_from_id (shell, panel, NULL, &error) == FALSE) {
	// 	g_warning ("Failed to activate '%s' panel: %s", panel, error->message);
	// 	g_error_free (error);
	// }
}

static void
cc_bluetooth_panel_init (CcBluetoothPanel *self)
{
	GError *error = NULL;

	self->priv = BLUETOOTH_PANEL_PRIVATE (self);
	//FIXME
	g_resources_register (cc_bluetooth_get_resource ());

	self->priv->builder = gtk_builder_new ();
	gtk_builder_set_translation_domain (self->priv->builder, GETTEXT_PACKAGE);
	gtk_builder_add_from_resource (self->priv->builder,
                                       "/org/cinnamon/control-center/bluetooth/bluetooth.ui",
                                       &error);
	if (error != NULL) {
		g_warning ("Could not load ui: %s", error->message);
		g_error_free (error);
		return;
	}

	self->priv->cancellable = g_cancellable_new ();

	/* RFKill */
	self->priv->rfkill = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
							    G_DBUS_PROXY_FLAGS_NONE,
							    NULL,
							    "org.cinnamon.SettingsDaemon.Rfkill",
							    "/org/cinnamon/SettingsDaemon/Rfkill",
							    "org.cinnamon.SettingsDaemon.Rfkill",
							    NULL, NULL);
	self->priv->properties = g_dbus_proxy_new_for_bus_sync (G_BUS_TYPE_SESSION,
								G_DBUS_PROXY_FLAGS_NONE,
								NULL,
								"org.cinnamon.SettingsDaemon.Rfkill",
								"/org/cinnamon/SettingsDaemon/Rfkill",
								"org.freedesktop.DBus.Properties",
								NULL, NULL);

	self->priv->main_box = gtk_box_new (GTK_ORIENTATION_VERTICAL, 6);

    /* add kill switch widgets  */
    self->priv->kill_switch_header = g_object_ref (WID ("box_power"));

    gtk_box_pack_start (GTK_BOX (self->priv->main_box), GTK_WIDGET (self->priv->kill_switch_header), FALSE, FALSE, 0);

    gtk_widget_show_all (GTK_WIDGET (self->priv->kill_switch_header));


	self->priv->stack = gtk_stack_new ();
	add_stack_page (self, _("Bluetooth is disabled"), BLUETOOTH_DISABLED_PAGE);
	add_stack_page (self, _("No Bluetooth adapters found"), BLUETOOTH_NO_DEVICES_PAGE);
	add_stack_page (self, _("Bluetooth is disabled by hardware switch"), BLUETOOTH_HW_DISABLED_PAGE);

	self->priv->widget = bluetooth_settings_widget_new ();

	gtk_stack_add_named (GTK_STACK (self->priv->stack),
			     self->priv->widget, BLUETOOTH_WORKING_PAGE);
	gtk_widget_show (self->priv->widget);
	gtk_widget_show (self->priv->stack);
	gtk_widget_show (self->priv->main_box);

	gtk_box_pack_start (GTK_BOX (self->priv->main_box), GTK_WIDGET (self->priv->stack), TRUE, TRUE, 0);
	gtk_container_add (GTK_CONTAINER (self), self->priv->main_box);

	airplane_mode_changed (NULL, NULL, NULL, self);
	g_signal_connect (self->priv->rfkill, "g-properties-changed",
        G_CALLBACK (airplane_mode_changed), self);

	 g_signal_connect (G_OBJECT (WID ("switch_bluetooth")), "notify::active",
	 		  G_CALLBACK (power_callback), self);
}

void
cc_bluetooth_panel_register (GIOModule *module)
{
	cc_bluetooth_panel_register_type (G_TYPE_MODULE (module));
	g_io_extension_point_implement (CC_SHELL_PANEL_EXTENSION_POINT,
					CC_TYPE_BLUETOOTH_PANEL,
					"bluetooth", 0);
}

/* GIO extension stuff */
void
g_io_module_load (GIOModule *module)
{
	bindtextdomain (GETTEXT_PACKAGE, "/usr/share/locale");
	bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");

	/* register the panel */
	cc_bluetooth_panel_register (module);
}

void
g_io_module_unload (GIOModule *module)
{
}
