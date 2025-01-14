/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2022- sfwbar maintainers
 */

#include "sfwbar.h"
#include "trayitem.h"
#include "tray.h"

G_DEFINE_TYPE_WITH_CODE (Tray, tray, FLOW_GRID_TYPE, G_ADD_PRIVATE (Tray))

static GList *trays;

static GtkWidget *tray_mirror ( GtkWidget *src )
{
  GtkWidget *self;

  g_return_val_if_fail(IS_TRAY(src),NULL);

  self = tray_new();
  flow_grid_copy_properties(self,src);
  base_widget_copy_properties(self,src);

  return self;
}

static void tray_destroy ( GtkWidget *self )
{
  trays = g_list_remove(trays,self);
  GTK_WIDGET_CLASS(tray_parent_class)->destroy(self);
}

static void tray_class_init ( TrayClass *kclass )
{
  BASE_WIDGET_CLASS(kclass)->mirror = tray_mirror;
  GTK_WIDGET_CLASS(kclass)->destroy = tray_destroy;
  BASE_WIDGET_CLASS(kclass)->action_exec = NULL;
}

static void tray_init ( Tray *self )
{
}

GtkWidget *tray_new ( void )
{
  GtkWidget *self;
  GList *iter;

  self = GTK_WIDGET(g_object_new(tray_get_type(), NULL));
  gtk_grid_set_column_homogeneous(GTK_GRID(gtk_bin_get_child(GTK_BIN(self))), FALSE);

  if(!trays)
    sni_init();

  trays = g_list_append(trays, self);
  for(iter = sni_item_get_list(); iter; iter=g_list_next(iter))
    tray_item_new(iter->data, self);

  return self;
}

void tray_invalidate_all ( SniItem *sni )
{
  GList *iter;

  for(iter=trays; iter; iter=g_list_next(iter))
    flow_item_invalidate(flow_grid_find_child(iter->data,sni));
}

void tray_item_init_for_all ( SniItem *sni )
{
  GList *iter;

  for(iter=trays; iter; iter=g_list_next(iter))
    if(iter->data)
      tray_item_new(sni,iter->data);
}

void tray_item_destroy ( SniItem *sni )
{
  GList *iter;

  for(iter=trays; iter; iter=g_list_next(iter))
    flow_grid_delete_child(iter->data, sni);
}

void tray_update ( void )
{
  GList *iter;

  for(iter=trays; iter; iter=g_list_next(iter))
    if(iter->data)
      flow_grid_update(iter->data);
}
