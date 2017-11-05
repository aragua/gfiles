#include<gtk/gtk.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>

enum {
	COL_TYPE = 0,
	COL_NAME,
	COL_SIZE,
	COL_MOD,
	NUM_COLS
};

typedef struct _Data {
	gchar *curr_dir;
	gboolean show_hidden;
	GtkWidget *path;
	GtkWidget *store;
} Data;

#define DEFAULTPATH "/"
Data g_data;

static GtkTreeModel *create_and_fill_model(void)
{
	GtkListStore *store;
	GtkTreeIter iter;
	guint idx = 0;
	GDir *dir;
	GError *error;
	const char *filename;

	store =
	    gtk_list_store_new(NUM_COLS, G_TYPE_STRING, G_TYPE_STRING,
			       G_TYPE_STRING, G_TYPE_STRING);

	dir = g_dir_open(g_data.curr_dir, 0, &error);
	if (!dir) {
		g_error("Fail to find %s folder", dir);
		return NULL;
	}
	while ((filename = g_dir_read_name(dir))) {
		gchar *uri = NULL, *type = "-", *size = "-", *mod = "-";
		struct stat st;
		if (!filename)
			break;

		if (!g_data.show_hidden && (filename[0] == '.'))
			continue;

		uri = g_strdup_printf("%s/%s", g_data.curr_dir, filename);
		if (uri && (stat(uri, &st) == 0)) {
			switch (st.st_mode & S_IFMT) {
			case S_IFSOCK:
				type = "s";
				break;
			case S_IFLNK:
				type = "l";
				break;
			case S_IFREG:
				type = "r";
				break;
			case S_IFBLK:
				type = "b";
				break;
			case S_IFDIR:
				type = "d";
				break;
			case S_IFCHR:
				type = "c";
				break;
			case S_IFIFO:
				type = "f";
				break;
			default:
				type = "-";
				break;
			}
			if (st.st_size < 1024)
				size = g_strdup_printf("%lu", st.st_size);
			else if (st.st_size < 1024 * 1024)
				size =
				    g_strdup_printf("%luK", st.st_size / 1024);
			else if (st.st_size < 1024 * 1024 * 1024)
				size =
				    g_strdup_printf("%luM",
						    st.st_size / (1024 * 1024));
			else
				size =
				    g_strdup_printf("%luG",
						    st.st_size / (1024 *
								  1024 * 1024));
		}
		gtk_list_store_append(store, &iter);
		gtk_list_store_set(store, &iter, COL_TYPE, type,
				   COL_NAME, filename, COL_SIZE, size,
				   COL_MOD, mod, -1);
		idx++;
		g_free(uri);
		g_free(size);
	}
	return GTK_TREE_MODEL(store);
}

static void update_model(GtkWidget * view)
{
	GtkTreeModel *model;

	model = create_and_fill_model();

	gtk_tree_view_set_model(GTK_TREE_VIEW(view), model);

	/* The tree view has acquired its own reference to the
	 *  model, so we can drop ours. That way the model will
	 *  be freed automatically when the tree view is destroyed */

	g_object_unref(model);
}

static gchar * get_launcher(const gchar * filename)
{
	gchar * launcher = NULL;
	guchar *content;
	gsize sz;

	if (g_file_get_contents (filename, (gchar **)&content, &sz, NULL)) {
		gchar *mime_type, *type;
		mime_type = g_content_type_guess (filename, content, sz, NULL);
		type = g_content_type_from_mime_type (mime_type);
		g_free(mime_type);
		g_print("%s : %s\n", filename, type);
		/* TODO: find executable name */
		g_free (content);
	}

	return launcher;
}

static GtkWidget *create_view_and_model(GtkWidget * view)
{
	GtkCellRenderer *renderer;

	/* --- Column #0 --- */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
						    0,
						    "Type",
						    renderer,
						    "text", COL_TYPE, NULL);
	/* --- Column #1 --- */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
						    3,
						    "Name",
						    renderer,
						    "text", COL_NAME, NULL);
	/* --- Column #2 --- */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
						    1,
						    "Size",
						    renderer,
						    "text", COL_SIZE, NULL);
	/* --- Column #3 --- */
	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(view),
						    2,
						    "Modification",
						    renderer,
						    "text", COL_MOD, NULL);

	update_model(view);

	return view;
}

static void
row_selected(GtkTreeView * tree_view,
	     GtkTreePath * path, GtkTreeViewColumn * column, gpointer user_data)
{
	Data *data = user_data;
	GtkTreeSelection *selection;
	GtkTreeModel *model;
	GtkTreeIter iter;

	/* This will only work in single or browse selection mode! */
	selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
	if (gtk_tree_selection_get_selected(selection, &model, &iter)) {
		gchar *type, *name, *size, *mod;

		gtk_tree_model_get(model, &iter, COL_TYPE, &type,
				   COL_NAME, &name, COL_SIZE, &size,
				   COL_MOD, &mod, -1);
		if (type[0] == 'd') {
			data->curr_dir =
			    g_strdup_printf("%s/%s", g_data.curr_dir, name);
			gtk_entry_set_text(GTK_ENTRY(g_data.path),
					   g_data.curr_dir);
			update_model(GTK_WIDGET(tree_view));
		} else {
			gchar *tmp, *mime;
			g_print("[%s] %s%s - %s - %s\n", type,
				data->curr_dir, name, size, mod);
			tmp = g_strdup_printf("%s/%s", g_data.curr_dir, name);
			mime = get_launcher (tmp);
			g_free(tmp);
			g_free(mime);
		}
		g_free(name);
	} else {
		g_print("no row selected.\n");
	}
}

void entry_activated(GtkEntry * entry, gpointer user_data)
{
	Data *data = user_data;
	const gchar * tmp;
	struct stat st;
	tmp = gtk_entry_get_text(GTK_ENTRY(data->path));
	if (!tmp) {
		gtk_entry_set_text(GTK_ENTRY(data->path), data->curr_dir);
		return;
	}
	if (memcmp(tmp, data->curr_dir, strlen(data->curr_dir) + 1) == 0) {
		return;
	}

	if (stat(tmp, &st) == 0) {
		if ((st.st_mode & S_IFMT) == S_IFDIR) {
			g_free(data->curr_dir);
			data->curr_dir = g_strdup(tmp);
			update_model(GTK_WIDGET(data->store));
			return;
		}
	}
	gtk_entry_set_text(GTK_ENTRY(data->path), data->curr_dir);
}

void previous_clicked(GtkButton * button, gpointer user_data)
{
	Data *data = user_data;
	gchar *last;
	struct stat st;
	last = strrchr(data->curr_dir, '/');
	if (last) {
		if (last != data->curr_dir)
			last[0] = 0;
		else
			last[1] = 0;
		if (stat(data->curr_dir, &st) == 0) {
			gtk_entry_set_text(GTK_ENTRY(data->path),
					   data->curr_dir);
			update_model(GTK_WIDGET(data->store));
		} else
			g_print("Not a directory\n");
	} else
		g_print("Can't find '/'\n");
}

static void
sb_selected (GtkPlacesSidebar  *sidebar,
               GObject           *location,
               GtkPlacesOpenFlags open_flags,
               gpointer           user_data)
{
	Data *data = user_data;
	gchar * tmp;
	tmp = g_file_get_path(G_FILE(location));

	if (tmp) {
		g_free(data->curr_dir);
		data->curr_dir = tmp;
		gtk_entry_set_text(GTK_ENTRY(data->path), data->curr_dir);
		update_model(GTK_WIDGET(data->store));
	} else
		g_print("file is NULL\n");
}

static void
show_hidden (GtkMenuItem * item, gpointer user_data)
{
  		g_print("show_hidden\n");
}

static void
show_panel (GtkMenuItem * item, gpointer user_data)
{
   		g_print("show_panel\n");
}

void files_init(GtkWidget * win)
{
	GtkWidget *mw, *sb, *tw, *sw, *pathbox, *prev, *mb, *mb_img, *mb_menu;
	GtkTreeViewColumn *col0, *col1, *col2, *col3;

	/* Create sidebar */
	sb = gtk_places_sidebar_new();
	gtk_scrolled_window_set_propagate_natural_width
	    (GTK_SCROLLED_WINDOW(sb), FALSE);
	gtk_scrolled_window_set_propagate_natural_height
	    (GTK_SCROLLED_WINDOW(sb), FALSE);
	gtk_widget_set_hexpand(sb, FALSE);
	gtk_places_sidebar_set_show_recent(GTK_PLACES_SIDEBAR(sb), FALSE);
	gtk_places_sidebar_set_show_trash(GTK_PLACES_SIDEBAR(sb), FALSE);
	gtk_places_sidebar_set_show_other_locations(GTK_PLACES_SIDEBAR(sb), FALSE);
	g_signal_connect(sb, "open-location",
			 G_CALLBACK(sb_selected), &g_data);

	/* create tree view */
	g_data.store = gtk_tree_view_new();
	create_view_and_model(g_data.store);
	gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW
						   (g_data.store), FALSE);
	g_signal_connect(g_data.store, "row-activated",
			 G_CALLBACK(row_selected), &g_data);
	sw = gtk_scrolled_window_new(NULL, NULL);
	gtk_container_add(GTK_CONTAINER(sw), g_data.store);
	gtk_scrolled_window_set_propagate_natural_width
	    (GTK_SCROLLED_WINDOW(sw), TRUE);
	gtk_scrolled_window_set_propagate_natural_height
	    (GTK_SCROLLED_WINDOW(sw), TRUE);
	col0 = gtk_tree_view_get_column (GTK_TREE_VIEW(g_data.store), 0);
	gtk_tree_view_column_set_resizable (col0, TRUE);
	col1 = gtk_tree_view_get_column (GTK_TREE_VIEW(g_data.store), 1);
	gtk_tree_view_column_set_resizable (col1, TRUE);
	col2 = gtk_tree_view_get_column (GTK_TREE_VIEW(g_data.store), 2);
	gtk_tree_view_column_set_resizable (col2, TRUE);
	col3 = gtk_tree_view_get_column (GTK_TREE_VIEW(g_data.store), 3);
	gtk_tree_view_column_set_resizable (col3, TRUE);

	/* Create path */
	g_data.path = gtk_entry_new();
	gtk_entry_set_text(GTK_ENTRY(g_data.path), g_data.curr_dir);
	g_signal_connect(g_data.path, "activate",
			 G_CALLBACK(entry_activated), &g_data);

	prev = gtk_button_new_with_label("<");
	g_signal_connect(prev, "clicked", G_CALLBACK(previous_clicked),
			 &g_data);


	mb = gtk_menu_button_new ();
    mb_img = gtk_image_new_from_icon_name("system-file-manager", GTK_ICON_SIZE_BUTTON);
	gtk_button_set_image(GTK_BUTTON(mb), mb_img);

    mb_menu = gtk_menu_new();
    if (mb_menu) {
        GtkWidget * item;
        item = gtk_menu_item_new_with_label ("Show hidden files");
        gtk_menu_attach(GTK_MENU(mb_menu), item, 0, 1, 0, 1);
        gtk_widget_show(item);
       	g_signal_connect(item, "activate", G_CALLBACK(show_hidden), &g_data);
        item = gtk_menu_item_new_with_label ("Show side panel");
        gtk_menu_attach(GTK_MENU(mb_menu), item, 0, 1, 1, 2);
        gtk_widget_show(item);
       	g_signal_connect(item, "activate", G_CALLBACK(show_panel), &g_data);
    }
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(mb), mb_menu);

	pathbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(pathbox), prev, FALSE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(pathbox), g_data.path, TRUE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(pathbox), mb, FALSE, TRUE, 2);

	/* create tree view container */
	tw = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_box_pack_start(GTK_BOX(tw), pathbox, FALSE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(tw), sw, TRUE, TRUE, 2);

	/* create main */
	mw = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
	gtk_box_pack_start(GTK_BOX(mw), sb, FALSE, TRUE, 2);
	gtk_box_pack_start(GTK_BOX(mw), tw, TRUE, TRUE, 2);

	/* Add browser in window */
	gtk_container_add(GTK_CONTAINER(win), mw);
}

int main(int argc, char **argv)
{
	GtkWidget *win;
	gtk_init(&argc, &argv);

	g_data.curr_dir = g_strdup(DEFAULTPATH);
	g_data.show_hidden = FALSE;

	/* Use dark theme */
	g_object_set(gtk_settings_get_default(),
		     "gtk-application-prefer-dark-theme", TRUE, NULL);

	win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_default_size(GTK_WINDOW(win), 700, 500);
	gtk_window_set_title(GTK_WINDOW(win), "File Browser");
	g_signal_connect(win, "destroy", G_CALLBACK(gtk_main_quit), NULL);

	files_init(win);

	gtk_widget_show_all(win);
	gtk_main();
	return 0;
}
