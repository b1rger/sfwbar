/* This entire file is licensed under GNU General Public License v3.0
 *
 * Copyright 2020-2022 Lev Babiev
 */

#include <glib.h>
#include <unistd.h>
#include <gtk/gtk.h>
#include "sfwbar.h"

typedef struct taskbar_item {
  GtkWidget *widget;
  GtkWidget *label;
  GtkWidget *icon;
  window_t *win;
  action_t **actions;
} item_t;

static GList *taskbars;

void taskbar_invalidate ( GtkWidget *taskbar )
{
  g_object_set_data(G_OBJECT(taskbar),"invalid",GINT_TO_POINTER(TRUE));
}

void taskbar_invalidate_all ( void )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data)
      taskbar_invalidate(((widget_t *)iter->data)->widget);
}

item_t *taskbar_item_lookup ( GtkWidget *taskbar, void *parent )
{
  GList *iter;

  iter = g_object_get_data(G_OBJECT(taskbar),"items");
  for(;iter;iter=g_list_next(iter))
    if(((item_t *)iter->data)->win == parent)
      return iter->data;

  return NULL;
}

gboolean taskbar_click_cb ( GtkWidget *widget, GdkEventButton *ev,
    gpointer data )
{
  item_t *item = data;

  if(GTK_IS_BUTTON(widget) && ev->button != 1)
    return FALSE;

  if(ev->type == GDK_BUTTON_PRESS && ev->button >= 1 && ev->button <= 3)
    action_exec(gtk_bin_get_child(GTK_BIN(widget)),
        item->actions[ev->button],(GdkEvent *)ev,
        wintree_from_id(item->win->uid),NULL);
  return TRUE;
}

gboolean taskbar_scroll_cb ( GtkWidget *w, GdkEventScroll *event,
    gpointer data )
{
  item_t *item = data;
  gint button;

  switch(event->direction)
  {
    case GDK_SCROLL_UP:
      button = 4;
      break;
    case GDK_SCROLL_DOWN:
      button = 5;
      break;
    case GDK_SCROLL_LEFT:
      button = 6;
      break;
    case GDK_SCROLL_RIGHT:
      button = 7;
      break;
    default:
      button = 0;
  }
  if(button)
    action_exec(gtk_bin_get_child(GTK_BIN(w)),
        item->actions[button], (GdkEvent *)event,
        wintree_from_id(item->win->uid),NULL);

  return TRUE;
}

void taskbar_button_cb( GtkWidget *widget, gpointer data )
{
  item_t *item = data;

  if(item->actions[1])
    action_exec(widget,item->actions[1],NULL,item->win,NULL);
  else
  {
    if ( wintree_is_focused(item->win->uid) )
      wintree_minimize(item->win->uid);
    else
      wintree_focus(item->win->uid);
  }

  taskbar_invalidate_all();
}

void taskbar_item_init ( GtkWidget *taskbar, window_t *win )
{
  item_t *item;
  GtkWidget *box, *button;
  gint dir;
  gboolean icons, labels;
  gint title_width;

  if(!taskbar)
    return;

  item = taskbar_item_lookup(taskbar, win);
  if(item)
    return;
  item = g_malloc0(sizeof(item_t));

  icons = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar),"icons"));
  labels = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar),"labels"));

  if(!icons)
    labels = TRUE;

  item->widget = gtk_event_box_new();
  button = gtk_button_new();
  gtk_container_add(GTK_CONTAINER(item->widget),button);
  gtk_widget_set_name(button, "taskbar_normal");
  gtk_widget_style_get(button,"direction",&dir,NULL);
  box = gtk_grid_new();
  gtk_container_add(GTK_CONTAINER(button),box);
  title_width = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar),"title_width"));
  if(!title_width)
    title_width = -1;

  if(icons)
  {
    item->icon = scale_image_new();
    scale_image_set_image(item->icon,win->appid,NULL);
    gtk_grid_attach_next_to(GTK_GRID(box),item->icon,NULL,dir,1,1);
  }
  else
    item->icon = NULL;
  if(labels)
  {
    item->label = gtk_label_new(win->title);
    gtk_label_set_ellipsize (GTK_LABEL(item->label),PANGO_ELLIPSIZE_END);
    gtk_label_set_max_width_chars(GTK_LABEL(item->label),title_width);
    widget_set_css(item->label,NULL);
    gtk_grid_attach_next_to(GTK_GRID(box),item->label,item->icon,dir,1,1);
  }

  item->win = win;
  item->actions = g_object_get_data(G_OBJECT(taskbar),"actions");
  g_object_ref(G_OBJECT(item->widget));
  g_signal_connect(button,"clicked",G_CALLBACK(taskbar_button_cb),item);
  g_signal_connect(item->widget,"button_press_event",
      G_CALLBACK(taskbar_click_cb),item);
  gtk_widget_add_events(GTK_WIDGET(item->widget),GDK_SCROLL_MASK);
  g_signal_connect(item->widget,"scroll-event",
      G_CALLBACK(taskbar_scroll_cb),item);
  g_object_set_data(G_OBJECT(taskbar),"items", g_list_append(
      g_object_get_data(G_OBJECT(taskbar),"items"), item));
}

void taskbar_item_init_for_all ( window_t *win )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data)
      taskbar_item_init(((widget_t *)iter->data)->widget, win );
}

void taskbar_item_destroy ( GtkWidget *taskbar, window_t *win )
{
  item_t *item;
  GList *iter;

  if(!taskbar)
    return;

  iter = g_object_get_data(G_OBJECT(taskbar),"items");
  for(;iter;iter=g_list_next(iter))
    if(((item_t *)iter->data)->win == win)
      break;

  if(!iter)
    return;

  item = iter->data;

  if(item)
  {
    gtk_widget_destroy(item->widget);
    g_object_unref(G_OBJECT(item->widget));
    g_free(item);
  }
  g_object_set_data(G_OBJECT(taskbar),"items",g_list_delete_link(
        g_object_get_data(G_OBJECT(taskbar),"items"),iter));
}

void taskbar_item_destroy_for_all ( window_t *win )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data)
      taskbar_item_destroy(((widget_t *)iter->data)->widget, win );
}

void taskbar_set_label ( GtkWidget *taskbar, window_t *win, gchar *title )
{
  item_t *item;

  if(!taskbar)
    return;

  if(!GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar),"labels")))
    return;

  item = taskbar_item_lookup(taskbar,win);

  if(!item)
    return;

  if(item->label)
    gtk_label_set_text(GTK_LABEL(item->label), title);
}

void taskbar_set_label_for_all ( window_t *win, gchar *title )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data)
      taskbar_set_label(((widget_t *)iter->data)->widget,win,title);
}

void taskbar_update( GtkWidget *taskbar )
{
  item_t *item;
  GList *iter;
  gchar *output;
  gboolean filter_output;

  if(!taskbar)
    return;

  if(!GPOINTER_TO_INT(g_object_get_data(
          G_OBJECT(taskbar),"invalid")))
    return;

  output = bar_get_output(taskbar);
  flow_grid_clean(taskbar);
  filter_output = GPOINTER_TO_INT(
      g_object_get_data(G_OBJECT(taskbar),"filter_output"));
  iter = g_object_get_data(G_OBJECT(taskbar),"items");
  for (; iter; iter = g_list_next(iter) )
  {
    item = iter->data;
    if(item)
    {
      if( !filter_output || !!item->win->output ||
          g_strcmp0(item->win->output,output))
      {
        if ( wintree_is_focused(item->win->uid) )
          gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(item->widget)),
              "taskbar_active");
        else
          gtk_widget_set_name(gtk_bin_get_child(GTK_BIN(item->widget)),
              "taskbar_normal");
        gtk_widget_unset_state_flags(gtk_bin_get_child(GTK_BIN(item->widget)),
            GTK_STATE_FLAG_PRELIGHT);

        widget_set_css(item->widget,GINT_TO_POINTER(TRUE));
        flow_grid_attach(taskbar,item->widget);
      }
    }
  }
  flow_grid_pad(taskbar);
  gtk_widget_show_all(taskbar);

  g_object_set_data(G_OBJECT(taskbar),"invalid", GINT_TO_POINTER(FALSE));
}

void taskbar_update_all ( void )
{
  GList *iter;

  for(iter=taskbars; iter; iter=g_list_next(iter))
    if(iter->data)
      taskbar_update(((widget_t *)iter->data)->widget);
}

void taskbar_init ( widget_t *lw )
{
  GList *iter;

  taskbars = g_list_append(taskbars,lw);
  g_object_set_data(G_OBJECT(lw->widget),"actions",lw->actions);

  for(iter=wintree_get_list(); iter; iter=g_list_next(iter))
    taskbar_item_init(lw->widget,iter->data);

  taskbar_invalidate(lw->widget);
}

