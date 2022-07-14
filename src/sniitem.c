/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2021-2022 Lev Babiev
 */

#include <glib.h>
#include <stdio.h>
#include <gio/gio.h>
#include <unistd.h>
#include "sfwbar.h"
#include "trayitem.h"
#include "tray.h"

struct sni_prop_wrapper {
  guint prop;
  sni_item_t *sni;
};

static gchar *sni_properties[] = { "Category", "Id", "Title", "Status",
  "IconName", "OverlayIconName", "AttentionIconName", "AttentionMovieName",
  "IconThemePath", "IconPixmap", "OverlayIconPixmap", "AttentionIconPixmap",
  "ToolTip", "WindowId", "ItemIsMenu", "Menu" };

GdkPixbuf *sni_item_get_pixbuf ( GVariant *v )
{
  gint32 x,y;
  GVariantIter *iter,*rgba;
  guchar *buff;
  guint32 *ptr;
  guchar t;
  cairo_surface_t *cs;
  GdkPixbuf *res;
  gint i=0;

  if(!v)
    return NULL;
  g_variant_get(v,"a(iiay)",&iter);
  if(!g_variant_iter_n_children(iter))
  {
    g_variant_iter_free(iter);
    return NULL;
  }
  g_variant_get(g_variant_iter_next_value(iter),"(iiay)",&x,&y,&rgba);
  if((x*y>0)&&(x*y*4 == g_variant_iter_n_children(rgba)))
  {
    buff = g_malloc(x*y*4);
    while(g_variant_iter_loop(rgba,"y",&t))
      buff[i++]=t;

    ptr=(guint32 *)buff;
    for(i=0;i<x*y;i++)
      ptr[i] = g_ntohl(ptr[i]);

    cs = cairo_image_surface_create_for_data(buff,CAIRO_FORMAT_ARGB32,x,y,
        cairo_format_stride_for_width(CAIRO_FORMAT_ARGB32,x));
    res = gdk_pixbuf_get_from_surface(cs,0,0,x,y);

    cairo_surface_destroy(cs);
    g_free(buff);
  }
  else
    res = NULL;
  if(iter)
    g_variant_iter_free(iter);
  if(rgba)
    g_variant_iter_free(rgba);
  return res;
}

void sni_item_set_icon ( sni_item_t *sni, gint icon, gint pix )
{
  if(icon==-1)
  {
    scale_image_set_image(sni->image,NULL,NULL);
    return;
  }
  if(sni->string[icon]!=NULL)
  {
    scale_image_set_image(sni->image,sni->string[icon],sni->string[SNI_PROP_THEME]);
    return;
  }
  if(sni->pixbuf[pix-SNI_PROP_ICONPIX]!=NULL)
  {
    scale_image_set_pixbuf(sni->image,sni->pixbuf[pix-SNI_PROP_ICONPIX]);
    return;
  }
  return;
}

void sni_item_prop_cb ( GDBusConnection *con, GAsyncResult *res,
    struct sni_prop_wrapper *wrap)
{
  GVariant *result, *inner;
  gchar *param;

  wrap->sni->ref--;
  result = g_dbus_connection_call_finish(con, res, NULL);
  if(result==NULL)
  {
    g_free(wrap);
    return;
  }
  g_variant_get(result, "(v)",&inner);
  if(wrap->prop<=SNI_PROP_THEME)
  {
    g_free(wrap->sni->string[wrap->prop]);
    if(inner && g_variant_is_of_type(inner,G_VARIANT_TYPE_STRING))
      g_variant_get(inner,"s",&param);
    else
      param=NULL;
    wrap->sni->string[wrap->prop] = param;
    g_debug("sni %s: property %s = %s",wrap->sni->dest,
        sni_properties[wrap->prop],wrap->sni->string[wrap->prop]);
  }
  if((wrap->prop>=SNI_PROP_ICONPIX)&&(wrap->prop<=SNI_PROP_ATTNPIX))
  {
    if(wrap->sni->pixbuf[wrap->prop-SNI_PROP_ICONPIX]!=NULL)
      g_object_unref(wrap->sni->pixbuf[wrap->prop-SNI_PROP_ICONPIX]);
    wrap->sni->pixbuf[wrap->prop-SNI_PROP_ICONPIX] =
      sni_item_get_pixbuf(inner);
  }
  if(wrap->prop == SNI_PROP_MENU && inner &&
      g_variant_is_of_type(inner,G_VARIANT_TYPE_OBJECT_PATH))
    {
      g_free(wrap->sni->menu_path);
      g_variant_get(inner,"o",&param);
      wrap->sni->menu_path = param;
      g_debug("sni %s: property %s = %s",wrap->sni->dest,
          sni_properties[wrap->prop],wrap->sni->menu_path);
    }
  if(wrap->prop == SNI_PROP_ISMENU)
    g_variant_get(inner,"b",&(wrap->sni->menu));
  if(wrap->sni->string[SNI_PROP_STATUS]!=NULL)
  {
    if(wrap->sni->string[SNI_PROP_STATUS][0]=='A')
    {
      gtk_widget_set_name(wrap->sni->image,"tray_active");
      sni_item_set_icon(wrap->sni,SNI_PROP_ICON,SNI_PROP_ICONPIX);
    }
    if(wrap->sni->string[SNI_PROP_STATUS][0]=='N')
    {
      gtk_widget_set_name(wrap->sni->image,"tray_attention");
      sni_item_set_icon(wrap->sni,SNI_PROP_ATTN,SNI_PROP_ATTNPIX);
    }
    if(wrap->sni->string[SNI_PROP_STATUS][0]=='P')
    {
      gtk_widget_set_name(wrap->sni->image,"tray_passive");
      sni_item_set_icon(wrap->sni,SNI_PROP_ICON,SNI_PROP_ICONPIX);
    }
  }

  if(inner)
    g_variant_unref(inner);
  if(result)
    g_variant_unref(result);
  g_free(wrap);
  tray_invalidate_all();
}

void sni_item_get_prop ( GDBusConnection *con, sni_item_t *sni,
    guint prop )
{
  struct sni_prop_wrapper *wrap;

  wrap = g_malloc(sizeof(struct sni_prop_wrapper));
  wrap->prop = prop;
  wrap->sni = sni;
  wrap->sni->ref++;

  g_dbus_connection_call(con, sni->dest, sni->path,
    "org.freedesktop.DBus.Properties", "Get",
    g_variant_new("(ss)", sni->host->item_iface, sni_properties[prop]),NULL,
    G_DBUS_CALL_FLAGS_NONE,-1,sni->cancel,
    (GAsyncReadyCallback)sni_item_prop_cb,wrap);
}

void sni_item_signal_cb (GDBusConnection *con, const gchar *sender,
         const gchar *path, const gchar *interface, const gchar *signal,
         GVariant *parameters, gpointer data)
{
  g_debug("sni: received signal %s from %s",signal,sender);
  if(g_strcmp0(signal,"NewTitle")==0)
    sni_item_get_prop(con,data,SNI_PROP_TITLE);
  if(g_strcmp0(signal,"NewStatus")==0)
    sni_item_get_prop(con,data,SNI_PROP_STATUS);
  if(g_strcmp0(signal,"NewToolTip")==0)
    sni_item_get_prop(con,data,SNI_PROP_TOOLTIP);
  if(g_strcmp0(signal,"NewIconThemePath")==0)
    sni_item_get_prop(con,data,SNI_PROP_THEME);
  if(g_strcmp0(signal,"NewIcon")==0)
  {
    sni_item_get_prop(con,data,SNI_PROP_ICON);
    sni_item_get_prop(con,data,SNI_PROP_ICONPIX);
  }
  if(g_strcmp0(signal,"NewOverlayIcon")==0)
  {
    sni_item_get_prop(con,data,SNI_PROP_OVLAY);
    sni_item_get_prop(con,data,SNI_PROP_OVLAYPIX);
  }
  if(g_strcmp0(signal,"NewAttentionIcon")==0)
  {
    sni_item_get_prop(con,data,SNI_PROP_ATTN);
    sni_item_get_prop(con,data,SNI_PROP_ATTNPIX);
  }
}

gboolean sni_item_scroll_cb ( GtkWidget *w, GdkEventScroll *event,
    gpointer data )
{
  sni_item_t *sni = data;
  GDBusConnection *con;
  gchar *dir;
  gint32 delta;

  if((event->direction == GDK_SCROLL_DOWN)||
      (event->direction == GDK_SCROLL_RIGHT))
    delta=1;
  else
    delta=-1;
  if((event->direction == GDK_SCROLL_DOWN)||
      (event->direction == GDK_SCROLL_UP))
    dir = "vertical";
  else
    dir = "horizontal";

  con = g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);
  g_dbus_connection_call(con, sni->dest, sni->path,
    sni->host->item_iface, "Scroll", g_variant_new("(si)", dir, delta),
    NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
  g_object_unref(con);
  return TRUE;
}

gboolean sni_item_click_cb (GtkWidget *w, GdkEventButton *event, gpointer data)
{
  sni_item_t *sni = data;
  GDBusConnection *con;
  gchar *method=NULL;
  GtkAllocation alloc,walloc;
  GdkRectangle geo;
  gint32 x,y;

  if(event->type == GDK_BUTTON_PRESS)
  {
    con = g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);
    g_debug("sni %s: button: %d",sni->dest,event->button);
    if(event->button == 1)
    {
      if(sni->menu_path)
        sni_get_menu(sni,(GdkEvent *)event);
      else
      {
        if(sni->menu)
          method = "ContextMenu";
        else
          method = "Activate";
      }
    }
    if(event->button == 2)
      method = "SecondaryActivate";
    if(event->button == 3)
      method = "ContextMenu";

    gdk_monitor_get_geometry(gdk_display_get_monitor_at_window(
        gtk_widget_get_display(gtk_widget_get_toplevel(w)),
        gtk_widget_get_window(w)),&geo);
    gtk_widget_get_allocation(w,&alloc);
    gtk_widget_get_allocation(gtk_widget_get_toplevel(w),&walloc);

    if(bar_get_toplevel_dir(w) == GTK_POS_RIGHT)
      x = geo.width - walloc.width + event->x + alloc.x;
    else
    {
      if(bar_get_toplevel_dir(w) == GTK_POS_LEFT)
        x = walloc.width;
      else
        x = event->x + alloc.x;
    }

    if(bar_get_toplevel_dir(w) == GTK_POS_BOTTOM)
      y = geo.height - walloc.height;
    else
    {
      if(bar_get_toplevel_dir(w) == GTK_POS_TOP)
        y = walloc.height;
      else
        y = event->y + alloc.y;
    }

    // call event at 0,0 to avoid menu popping up under the bar
    if(method)
    {
      g_debug("sni: calling %s on %s at ( %d, %d )",method,sni->dest,x,y);
      g_dbus_connection_call(con, sni->dest, sni->path,
        sni->host->item_iface, method, g_variant_new("(ii)", 0, 0),
        NULL, G_DBUS_CALL_FLAGS_NONE, -1, NULL, NULL, NULL);
    }
    g_object_unref(con);
  }
  return TRUE;
}

sni_item_t *sni_item_new (GDBusConnection *con, SniHost *host,
    const gchar *uid)
{
  sni_item_t *sni;
  gchar *path;
  guint i;

  sni = g_malloc0(sizeof(sni_item_t));
  sni->uid = g_strdup(uid);
  sni->cancel = g_cancellable_new();
  path = strchr(uid,'/');
  if(path!=NULL)
  {
    sni->dest = g_strndup(uid,path-uid);
    sni->path = g_strdup(path);
  }
  else
  {
    sni->path = g_strdup("/StatusNotifierItem");
    sni->dest = g_strdup(uid);
  }
  sni->host = host;
  sni->signal = g_dbus_connection_signal_subscribe(con,sni->dest,
      sni->host->item_iface,NULL,sni->path,NULL,0,sni_item_signal_cb,sni,NULL);
  tray_item_init_for_all(sni);
  for(i=0;i<16;i++)
    sni_item_get_prop(con,sni,i);

  return sni;
}

void sni_item_free ( sni_item_t *sni )
{
  gint i;
  GDBusConnection *con;

  con = g_bus_get_sync(G_BUS_TYPE_SESSION,NULL,NULL);
  if(con)
  {
    g_dbus_connection_signal_unsubscribe(con,sni->signal);
    g_object_unref(con);
  }
  g_cancellable_cancel(sni->cancel);
  for(i=0;i<3;i++)
    if(sni->pixbuf[i]!=NULL)
      g_object_unref(sni->pixbuf[i]);
  for(i=0;i<MAX_STRING;i++)
    g_free(sni->string[i]);
  tray_item_destroy(sni);

  g_free(sni->menu_path);
  g_free(sni->uid);
  g_free(sni->path);
  g_free(sni->dest);
  g_free(sni);
  tray_invalidate_all();
}