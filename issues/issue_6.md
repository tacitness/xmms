gtk3(libxmms): port dirbrowser from GtkCTree to GtkTreeView
libxmms/dirbrowser.c uses GtkCTree/GtkCTreeNode which were removed in GTK3.

Replace with GtkTreeView + GtkTreeStore hierarchy.

Compile errors:\n- unknown type name 'GtkCTree'\n- unknown type name 'GtkCTreeNode'\n- implicit declaration of 'gtk_ctree_*' functions\n- GTK_CLIST macro removed
