gtk3(core): replace all GdkEvent direct field access with GdkEventController (GTK4 path)
GTK4 removes direct GdkEvent field access. All event handling must migrate to GdkEventController subclasses:

- Mouse: GtkGestureClick, GtkGestureSwipe, GtkGestureDrag
- Keyboard: GtkEventControllerKey
- Scroll: GtkEventControllerScroll
- Motion: GtkEventControllerMotion

This is a GTK4 blocker affecting all interactive widgets (mainwin, equalizer, playlist, skin).
