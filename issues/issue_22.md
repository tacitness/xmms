fix(vis): OpenGL Spectrum Analyzer fails to render (GtkGLArea requires GL ≥ 3.2; GL 1.x fixed-function removed)
## Problem

When the OpenGL Spectrum Analyzer visualization plugin is opened, GDK emits:

```
Gdk-WARNING: gdk_gl_context_set_required_version - GL context versions less than 3.2 are not supported.
```

The plugin window opens but renders nothing (black/empty).

## Root Cause

`start_display()` calls:
```c
gtk_gl_area_set_required_version(GTK_GL_AREA(gl_area), 1, 0);
```

GtkGLArea (GDK) enforces a minimum of OpenGL 3.2 core profile on desktop Linux. The GL 1.x compatibility APIs used in the renderer (`glBegin/glEnd`, `glVertex3f`, `glColor3f`, `glMatrixMode`, `glFrustum`, `glTranslatef`, `glRotatef`, `glPushMatrix/glPopMatrix`) are not available in the core profile and produce no output even when they don't outright crash.

## Fix Required

1. Bump required version to `(3, 3)`
2. Replace fixed-function pipeline with modern GL 3.3 core:
   - Vertex + fragment shaders (GLSL 330 core)
   - VAO + dynamic VBO (interleaved position + colour)
   - Manual 4×4 matrix math for projection/model-view
   - `glDrawArrays(GL_TRIANGLES)` replacing `glBegin/glEnd` loops
3. Keep identical visual output (same coloured bars, same rotation controls)

## Component
- `Visualization/opengl_spectrum/opengl_spectrum.c`

## Labels
visualization, bug, P1-high
