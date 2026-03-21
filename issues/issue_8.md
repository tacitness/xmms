gtk3(playlist): port playlist window to GtkListView / GtkColumnView (GTK4)
The playlist window (xmms/playlist_list.c) uses GtkCList which was removed in GTK3.

GTK3 replacement: GtkTreeView + GtkListStore
GTK4 replacement: GtkListView + GtkColumnView + GListModel

This is a significant rewrite as GtkCList had built-in column handling, sorting, and selection that must be rebuilt with the GObject-based model/view architecture.
